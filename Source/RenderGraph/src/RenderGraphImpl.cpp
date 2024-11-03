#include "RenderGraphImpl.h"
#include "RenderResourceCache.h"
#include <imgui.h>

#include <glfw3native.h>
#include <d3d12.h>
#include <dxgiformat.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_glfw.h>
#include <memory>
#include <minwindef.h>
#include <queue>
#include <span>
#include <synchapi.h>
#include <unordered_map>

namespace FTS
{
    const IID IID_IRenderGraph = { 0xb6f63862, 0x3c47, 0x4b3d, { 0xbf, 0x68, 0x2e, 0x63, 0x67, 0xa8, 0xaa, 0x9c } };
    const IID IID_IRenderResourceCache = { 0x168eec06, 0xb9bf, 0x41f0, { 0x9e, 0x1a, 0xcb, 0xe9, 0x21, 0xed, 0x63, 0x0f } };

    BOOL CreateRenderGraph(IDevice* pDevice, std::function<void()> PresentFunc, FWorld* pWorld, CREFIID criid, void** ppvRenderGraph)
    {
        FRenderGraph* pRenderGraph = new FRenderGraph(pDevice, PresentFunc);
        if (!pRenderGraph->Initialize(pWorld) || !pRenderGraph->QueryInterface(criid, ppvRenderGraph))
        {
            LOG_ERROR("Create render graph failed.");
            return false;
        }
        return true;
    }
    

    FRenderGraph::FRenderGraph(IDevice* pDevice, std::function<void()> PresentFunc) :
        m_pDevice(pDevice), m_PresentFunc(PresentFunc)
    {
    }

    BOOL FRenderGraph::Initialize(FWorld* pWorld)
    {
        ReturnIfFalse(m_pDevice != nullptr && pWorld != nullptr);

        FRenderResourceCache* pCache = new FRenderResourceCache(pWorld);
        if (!pCache->Initialize() || !pCache->QueryInterface(IID_IRenderResourceCache, PPV_ARG(m_pResourceCache.GetAddressOf())))
        {
            LOG_ERROR("Render Graph Initialize Failed.");
            return false;
        }
        return true;
    }
        
    void FRenderGraph::Reset()
    {
        m_stGraphicsWaitValue = 0;
        m_stComputeWaitValue = 0;

        m_PassAsyncTypes.clear();
        m_pCmdLists.clear();
    }

    void FRenderGraph::AddPass(IRenderPass* pPass)
    {
        if (pPass != nullptr)
        {
			if (pPass->Type == ERenderPassType::Precompute)
			{
				m_pPrecomputePasses.emplace_back(pPass);
				pPass->dwIndex = static_cast<UINT32>(m_pPrecomputePasses.size() - 1);
			}
			else
			{
				m_pPasses.emplace_back(pPass);
				pPass->dwIndex = static_cast<UINT32>(m_pPasses.size() - 1);
			}
        }
    }

	BOOL FRenderGraph::TopologyPasses(BOOL bIsPrecompute)
	{
        std::span<IRenderPass*> Passes;
        if (bIsPrecompute) Passes = m_pPasses;
        else               Passes = m_pPrecomputePasses;

		std::queue<UINT32> Nodes;
		std::vector<UINT32> TopologyOrder;
		std::vector<UINT32> DependentList(Passes.size());

		for (UINT32 ix = 0; ix < Passes.size(); ++ix)
		{
			if (Passes[ix]->DependentsIndex.empty()) Nodes.push(ix);
			DependentList[ix] = Passes[ix]->DependentsIndex.size();
		}

		while (!Nodes.empty())
		{
			UINT32 dwIndex = Nodes.front();
			TopologyOrder.push_back(dwIndex);
			Nodes.pop();

			for (UINT32 ix : Passes[dwIndex]->SuccessorsIndex)
			{
				DependentList[ix]--;
				if (DependentList[ix] == 0) Nodes.push(ix);
			}
		}

		if (TopologyOrder.size() != Passes.size())
		{
			LOG_ERROR("Render pass topology occurs error.");
			return false;
		}

		for (UINT32 ix : DependentList)
		{
			if (ix != 0)
			{
				LOG_ERROR("There is a DAG in RenderPass.");
				return false;
			}
		}

		std::unordered_map<UINT32, UINT32> TopologyMap;
		for (UINT32 ix = 0; ix < Passes.size(); ++ix)
		{
			TopologyMap[TopologyOrder[ix]] = ix;
		}

		std::sort(
			Passes.begin(),
			Passes.end(),
			[&TopologyMap](const auto& crpPass0, const auto& crpPass1)
			{
				return TopologyMap[crpPass0->dwIndex] < TopologyMap[crpPass1->dwIndex];
			}
		);
	}

	BOOL FRenderGraph::Compile()
    {
		ReturnIfFalse(TopologyPasses(true));
		m_pPrecomputeCmdLists.resize(m_pPrecomputePasses.size());
		for (UINT32 ix = 0; ix < m_pPrecomputePasses.size(); ++ix)
		{
			ReturnIfFalse(m_pDevice->CreateCommandList(
				FCommandListDesc{ .QueueType = ECommandQueueType::Graphics },
				IID_ICommandList,
				PPV_ARG(m_pPrecomputeCmdLists[ix].GetAddressOf())
			));
			ReturnIfFalse(m_pPrecomputePasses[ix]->Compile(m_pDevice.Get(), m_pResourceCache.Get()));
		}


		ReturnIfFalse(TopologyPasses(false));

        m_pCmdLists.resize(m_pPasses.size());
        m_PassAsyncTypes.resize(m_pPasses.size());
        for (UINT32 ix = 0; ix < m_pPasses.size(); ++ix)
        {
            if ((m_pPasses[ix]->Type & ERenderPassType::Graphics) == ERenderPassType::Graphics)
            {
                ReturnIfFalse(m_pDevice->CreateCommandList(
                    FCommandListDesc{ .QueueType = ECommandQueueType::Graphics }, 
                    IID_ICommandList, 
                    PPV_ARG(m_pCmdLists[ix].GetAddressOf())
                ));
            }
            else if ((m_pPasses[ix]->Type & ERenderPassType::Compute) == ERenderPassType::Compute)
            {
                ReturnIfFalse(m_pDevice->CreateCommandList(
                    FCommandListDesc{ .QueueType = ECommandQueueType::Compute }, 
                    IID_ICommandList, 
                    PPV_ARG(m_pCmdLists[ix].GetAddressOf())
                ));
            }
            else 
            {
                LOG_ERROR("Invalid render pass type.");
                return false;
            }

            ReturnIfFalse(m_pPasses[ix]->Compile(m_pDevice.Get(), m_pResourceCache.Get()));

            for (UINT32 dwIndex : m_pPasses[ix]->DependentsIndex) 
            {
                if (m_pPasses[dwIndex]->Type != m_pPasses[ix]->Type)
                {
                    m_PassAsyncTypes[ix] |= EPassAsyncType::Wait;
                }
            }
            
            for (UINT32 dwIndex : m_pPasses[ix]->SuccessorsIndex) 
            {
                if (m_pPasses[dwIndex]->Type != m_pPasses[ix]->Type)
                {
                    m_PassAsyncTypes[ix] |= EPassAsyncType::Signal;
                }
            }
        }

        return true;
    }


	BOOL FRenderGraph::Precompute()
	{
		std::vector<ICommandList*> pComputeCmdLists;

		for (UINT32 ix = 0; ix < m_pPrecomputePasses.size(); ++ix)
		{
			if (m_pPrecomputePasses[ix]->Type == ERenderPassType::Exclude) continue;
			
			ReturnIfFalse(m_pPrecomputePasses[ix]->Execute(m_pPrecomputeCmdLists[ix].Get(), m_pResourceCache.Get()));
			pComputeCmdLists.push_back(m_pPrecomputeCmdLists[ix].Get());

			m_pPrecomputePasses[ix]->Type = ERenderPassType::Exclude;
		}

		m_pDevice->ExecuteCommandLists(pComputeCmdLists.data(), pComputeCmdLists.size(), ECommandQueueType::Graphics);
		m_pDevice->WaitForIdle();

		for (auto* pPass : m_pPrecomputePasses) pPass->FinishPass();

		return true;
	}


    BOOL FRenderGraph::Execute()
    {
		ReturnIfFalse(Precompute());

        for (UINT32 ix = 0; ix < m_pPasses.size(); ++ix)
        {
            ReturnIfFalse(m_pPasses[ix]->Execute(m_pCmdLists[ix].Get(), m_pResourceCache.Get()));
        }

        std::vector<ICommandList*> pGraphicsCmdLists;
        std::vector<ICommandList*> pComputeCmdLists;

        for (UINT32 ix = 0; ix < m_pCmdLists.size(); ++ix)
        {
			if ((m_pPasses[ix]->Type & ERenderPassType::Graphics) == ERenderPassType::Graphics)
			{
				if ((m_PassAsyncTypes[ix] & EPassAsyncType::Wait) == EPassAsyncType::Wait)
				{
					ReturnIfFalse(m_pDevice->QueueWaitForCommandList(
						ECommandQueueType::Graphics,
						ECommandQueueType::Compute,
						m_stGraphicsWaitValue
					));
				}

				pGraphicsCmdLists.emplace_back(m_pCmdLists[ix].Get());

				if ((m_PassAsyncTypes[ix] & EPassAsyncType::Signal) == EPassAsyncType::Signal)
				{
					m_stComputeWaitValue = m_pDevice->ExecuteCommandLists(
						pGraphicsCmdLists.data(),
						pGraphicsCmdLists.size(),
						ECommandQueueType::Graphics
					);
					ReturnIfFalse(m_stGraphicsWaitValue != INVALID_SIZE_64);
					pGraphicsCmdLists.clear();
				}
			}
			else if ((m_pPasses[ix]->Type & ERenderPassType::Compute) == ERenderPassType::Compute)
			{
				if ((m_PassAsyncTypes[ix] & EPassAsyncType::Wait) == EPassAsyncType::Wait)
				{
					ReturnIfFalse(m_pDevice->QueueWaitForCommandList(
						ECommandQueueType::Compute,
						ECommandQueueType::Graphics,
						m_stComputeWaitValue
					));
				}

				pComputeCmdLists.emplace_back(m_pCmdLists[ix].Get());

				if ((m_PassAsyncTypes[ix] & EPassAsyncType::Signal) == EPassAsyncType::Signal)
				{
					m_stGraphicsWaitValue = m_pDevice->ExecuteCommandLists(
						pComputeCmdLists.data(),
						pComputeCmdLists.size(),
						ECommandQueueType::Compute
					);
					ReturnIfFalse(m_stGraphicsWaitValue != INVALID_SIZE_64);
					pComputeCmdLists.clear();
				}
			}
		}

        m_pDevice->ExecuteCommandLists(pGraphicsCmdLists.data(), pGraphicsCmdLists.size(), ECommandQueueType::Graphics);
		m_pDevice->WaitForIdle();

		for (auto* pPass : m_pPasses) pPass->FinishPass();

		m_pDevice->RunGarbageCollection();
        m_PresentFunc();

        return true;
    }


}