#include "DX12Device.h"
#include <combaseapi.h>
#include <d3d12.h>
#include <d3dcommon.h>
#include <memory>
#include <minwindef.h>
#include <mutex>
#include <string>
#include <winbase.h>
#include <winerror.h>
#include <wtypesbase.h>

#include "DX12CommandList.h"
#include "DX12Descriptor.h"
#include "DX12Forward.h"
#include "DX12FrameBuffer.h"
#include "DX12Pipeline.h"
#include "DX12Resource.h"
#include "DX12Shader.h"


namespace FTS
{
    BOOL FDX12EventQuery::Start(ID3D12Fence* pD3D12Fence, UINT64 stFenceCounter)
    {
        if (pD3D12Fence == nullptr) return false;

        m_bStarted = true;
        m_pD3D12Fence = pD3D12Fence;
        m_stFenceCounter = stFenceCounter;
        m_bResolved = false;

        return true;
    }

    BOOL FDX12EventQuery::Poll()
    {
        if (!m_bStarted) return false;
        if (m_bResolved) return true;
        
        if (m_pD3D12Fence == nullptr) return false;

        if (m_pD3D12Fence->GetCompletedValue() >= m_stFenceCounter)
        {
             m_bResolved = true;
             m_pD3D12Fence = nullptr;
        }

        return m_bResolved;
    }

    void FDX12EventQuery::Wait(HANDLE WaitEvent)
    {
        if (!m_bStarted || m_bResolved || m_pD3D12Fence == nullptr) return;

        WaitForFence(m_pD3D12Fence.Get(), m_stFenceCounter, WaitEvent);
    }

    void FDX12EventQuery::Reset()
    {
        m_bStarted = false;
        m_bResolved = false;
        m_pD3D12Fence = nullptr;
    }

    FDX12TimerQuery::FDX12TimerQuery(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps, UINT32 dwQueryIndex) :
        m_cpContext(cpContext), 
        m_pDescriptorHeaps(pDescriptorHeaps), 
        m_dwBeginQueryIndex(dwQueryIndex * 2), 
        m_dwEndQueryIndex(dwQueryIndex * 2 + 1)
    {
    }

    BOOL FDX12TimerQuery::Poll()
    {
        if (!m_bStarted) return false;
        if (m_pD3D12Fence == nullptr) return true; 

        if (m_pD3D12Fence->GetCompletedValue() >= m_stFenceCounter)
        {
             m_bResolved = true;
             m_pD3D12Fence = nullptr;
        }
        
        return false;
    }

    FLOAT FDX12TimerQuery::GetTime(HANDLE WaitEvent, UINT64 stFrequency)
    {
        if (!m_bResolved)
        {
            if (m_pD3D12Fence != nullptr)
            {
                WaitForFence(m_pD3D12Fence.Get(), m_stFenceCounter, WaitEvent);
                m_pD3D12Fence = nullptr;
            }

            D3D12_RANGE BufferRange{
                m_dwBeginQueryIndex * sizeof(UINT64),
                (m_dwBeginQueryIndex + 2) * sizeof(UINT64)
            };

            UINT64* pstData;
            FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(m_cpContext->pTimerQueryResolveBuffer.Get());
            if (FAILED(pDX12Buffer->m_pD3D12Resource->Map(0, &BufferRange, reinterpret_cast<void**>(&pstData)))) return 0.0f;

            m_bResolved = true;
            m_fTime = static_cast<FLOAT>(static_cast<DOUBLE>(pstData[m_dwEndQueryIndex] - pstData[m_dwBeginQueryIndex]) - static_cast<DOUBLE>(stFrequency));

            pDX12Buffer->m_pD3D12Resource->Unmap(0, nullptr);
        }

        return m_fTime;
    }

    void FDX12TimerQuery::Reset()
    {
        m_bStarted = false;
        m_bResolved = false;
        m_fTime = 0.0f;
        m_pD3D12Fence = nullptr;
    }

    FDX12Device::FDX12Device(const FDX12DeviceDesc& crDesc) : m_Desc(crDesc)
    {
    }

    FDX12Device::~FDX12Device() noexcept
    {
        WaitForIdle();
        if (m_FenceEvent)
        {
            CloseHandle(m_FenceEvent);
            m_FenceEvent = nullptr;
        }
    }


    BOOL FDX12Device::Initialize()
    {
        m_Context.pDevice = m_Desc.pD3D12Device;

        if (m_Desc.pD3D12GraphicsCommandQueue) m_pCmdQueues[static_cast<UINT8>(ECommandQueueType::Graphics)] = std::make_unique<FDX12CommandQueue>(m_Context, m_Desc.pD3D12GraphicsCommandQueue);
        if (m_Desc.pD3D12ComputeCommandQueue) m_pCmdQueues[static_cast<UINT8>(ECommandQueueType::Compute)] = std::make_unique<FDX12CommandQueue>(m_Context, m_Desc.pD3D12ComputeCommandQueue);
        if (m_Desc.pD3D12CopyCommandQueue) m_pCmdQueues[static_cast<UINT8>(ECommandQueueType::Copy)] = std::make_unique<FDX12CommandQueue>(m_Context, m_Desc.pD3D12CopyCommandQueue);

        m_pDescriptorHeaps = std::make_unique<FDX12DescriptorHeaps>(m_Context, m_Desc.dwMaxTimerQueries);
        m_pDescriptorHeaps->RenderTargetHeap.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_Desc.dwRenderTargetViewHeapSize, false);
        m_pDescriptorHeaps->DepthStencilHeap.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, m_Desc.dwDepthStencilViewHeapSize, false);
        m_pDescriptorHeaps->ShaderResourceHeap.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_Desc.dwShaderResourceViewHeapSize, true);
        m_pDescriptorHeaps->SamplerHeap.Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, m_Desc.dwSamplerHeapSize, true);

        m_FenceEvent = CreateEvent(nullptr, false, false, nullptr);
        m_pD3D12CmdListsToExecute.reserve(64u);

        return true;
    }


    BOOL FDX12Device::CreateHeap(const FHeapDesc& crDesc, CREFIID criid, void** ppvHeap)
    {
        FDX12Heap* pHeap = new FDX12Heap(&m_Context, crDesc);
        if (!pHeap->Initialize() || !pHeap->QueryInterface(criid, ppvHeap))
        {
            LOG_ERROR("Call to IDevice::CreateHeap failed.");
            return false;
        }
        return true;
    }

    BOOL FDX12Device::CreateTexture(const FTextureDesc& crDesc, CREFIID criid, void** ppvTexture)
    {
        FDX12Texture* pTexture = new FDX12Texture(&m_Context, m_pDescriptorHeaps.get(), crDesc);
        if (!pTexture->Initialize() || !pTexture->QueryInterface(criid, ppvTexture))
        {
            LOG_ERROR("Call to IDevice::CreateTexture failed.");
            return false;
        }
        return true;
    }
    
    BOOL FDX12Device::CreateStagingTexture(const FTextureDesc& crDesc, ECpuAccessMode CpuAccess, CREFIID criid, void** ppvStagingTexture)
    {
        if (CpuAccess == ECpuAccessMode::None)
        {
            LOG_ERROR("Call to IDevice::CreateStagingTexture failed for using ECpuAccessMode::None.");
            return false;
        }

        FDX12StagingTexture* pStagingTexture = new FDX12StagingTexture(&m_Context, crDesc, CpuAccess);
        if (!pStagingTexture->Initialize(m_pDescriptorHeaps.get()) || !pStagingTexture->QueryInterface(criid, ppvStagingTexture))
        {
            LOG_ERROR("Call to IDevice::CreateStagingTexture failed.");
            return false;
        }
        return true;
    }
    
    BOOL FDX12Device::CreateBuffer(const FBufferDesc& crDesc, CREFIID criid, void** ppvBuffer)
    {
        FDX12Buffer* pBuffer = new FDX12Buffer(&m_Context, m_pDescriptorHeaps.get(), crDesc);
        if (!pBuffer->Initialize() || !pBuffer->QueryInterface(criid, ppvBuffer))
        {
            LOG_ERROR("Call to IDevice::CreateBuffer failed.");
            return false;
        }
        return true;
    }

    BOOL FDX12Device::CreateTextureForNative(void* pNativeTexture, const FTextureDesc& crDesc, CREFIID criid, void** ppvTexture)
    {
        FDX12Texture* pTexture = new FDX12Texture(&m_Context, m_pDescriptorHeaps.get(), crDesc);
        if (!pTexture->QueryInterface(criid, ppvTexture))
        {
            LOG_ERROR("Call to IDevice::CreateTexture failed.");
            return false;
        }
        pTexture->m_pD3D12Resource = static_cast<ID3D12Resource*>(pNativeTexture);
        return true;
    }

    BOOL FDX12Device::CreateBufferForNative(void* pNativeBuffer, const FBufferDesc& crDesc, CREFIID criid, void** ppvBuffer)
    {
        FDX12Buffer* pBuffer = new FDX12Buffer(&m_Context, m_pDescriptorHeaps.get(), crDesc);
        if (!pBuffer->QueryInterface(criid, ppvBuffer))
        {
            LOG_ERROR("Call to IDevice::CreateBuffer failed.");
            return false;
        }
        pBuffer->m_pD3D12Resource = static_cast<ID3D12Resource*>(pNativeBuffer);
        return true;
    }


    BOOL FDX12Device::CreateShader(const FShaderDesc& crDesc, const void* cpvBinary, UINT64 stBinarySize, CREFIID criid, void** ppvShader)
    {
        FDX12Shader* pShader = new FDX12Shader(&m_Context, crDesc);
        if (!pShader->Initialize(cpvBinary, stBinarySize) || !pShader->QueryInterface(criid, ppvShader))
        {
            LOG_ERROR("Call to IDevice::CreateShader failed.");
            return false;
        }
        return true;
    }


    BOOL FDX12Device::CreateShaderLibrary(const void* cpvBinary, UINT64 stBinarySize, CREFIID criid, void** ppvShaderLibrary)
    {
        FDX12ShaderLibrary* pShaderLibrary = new FDX12ShaderLibrary(&m_Context);
        if (!pShaderLibrary->Initialize(cpvBinary, stBinarySize) || !pShaderLibrary->QueryInterface(criid, ppvShaderLibrary))
        {
            LOG_ERROR("Call to IDevice::CreateShaderLibrary failed.");
            return false;
        }
        return true;
    }

    BOOL FDX12Device::CreateSampler(const FSamplerDesc& crDesc, CREFIID criid, void** ppvSampler)
    {
        FDX12Sampler* pSampler = new FDX12Sampler(&m_Context, crDesc);
        if (!pSampler->Initialize() || !pSampler->QueryInterface(criid, ppvSampler))
        {
            LOG_ERROR("Call to IDevice::CreateSampler failed.");
            return false;
        }
        return true;
    }

    BOOL FDX12Device::CreateInputLayout(const FVertexAttributeDesc* cpDesc, UINT32 dwAttributeNum, IShader* pVertexShader, CREFIID criid, void** ppvInputLayout)
    {
        TStackArray<FVertexAttributeDesc, gdwMaxVertexAttributes> Attributes(dwAttributeNum);
        for (UINT32 ix = 0; ix < dwAttributeNum; ++ix)
        {
            Attributes[ix] = cpDesc[ix];
        }

        // pVertexShader is not used in dx12.

        FDX12InputLayout* pInputLayout = new FDX12InputLayout(&m_Context);
        if (!pInputLayout->Initialize(Attributes) || !pInputLayout->QueryInterface(criid, ppvInputLayout))
        {
            LOG_ERROR("Call to IDevice::CreateInputLayout failed.");
            return false;
        }
        return true;
    }
    
    BOOL FDX12Device::CreateEventQuery(CREFIID criid, void** ppvEventQuery)
    {
        FDX12EventQuery* pEventQuery = new FDX12EventQuery();
        if (!pEventQuery->Initialize() || !pEventQuery->QueryInterface(criid, ppvEventQuery))
        {
            LOG_ERROR("Call to IDevice::CreateEventQuery failed.");
            return false;
        }
        return true;
    }

    BOOL FDX12Device::SetEventQuery(IEventQuery* pQuery, ECommandQueueType QueueType)
    {
        if (pQuery == nullptr) return false;

        FDX12EventQuery* pDX12EventQuery = CheckedCast<FDX12EventQuery*>(pQuery);
        FDX12CommandQueue* pQueue = GetQueue(QueueType);
        return pDX12EventQuery->Start(pQueue->pD3D12Fence.Get(), pQueue->stLastSubmittedValue);
    }

    BOOL FDX12Device::PollEventQuery(IEventQuery* pQuery, BOOL* pbResult)
    {
        if (pQuery == nullptr || pbResult == nullptr) return false;
        
        FDX12EventQuery* pDX12EventQuery = CheckedCast<FDX12EventQuery*>(pQuery);

        *pbResult = pDX12EventQuery->Poll();

        return true;
    }

    BOOL FDX12Device::WaitEventQuery(IEventQuery* pQuery)
    {
        if (pQuery == nullptr) return false;

        FDX12EventQuery* pDX12EventQuery = CheckedCast<FDX12EventQuery*>(pQuery);

        pDX12EventQuery->Wait(m_FenceEvent);
        return true;
    }

    BOOL FDX12Device::ResetEventQuery(IEventQuery* pQuery)
    {
        if (pQuery == nullptr) return false;

        FDX12EventQuery* pDX12EventQuery = CheckedCast<FDX12EventQuery*>(pQuery);
        pDX12EventQuery->Reset();
        
        return true;
    }
    
    BOOL FDX12Device::CreateTimerQuery(CREFIID criid, void** ppvTimeQuery)
    {
        if (m_Context.pTimerQueryHeap != nullptr)
        {
            std::lock_guard LockGuard(m_Mutex);

            if (m_Context.pTimerQueryHeap != nullptr)
            {
                D3D12_QUERY_HEAP_DESC Desc{};
                Desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
                Desc.Count = static_cast<UINT32>(m_pDescriptorHeaps->TimeQueries.GetCapacity() * 2); // Use 2 D3D12 queries per 1 TimerQuery

                auto pTimerQueryHeap = m_Context.pTimerQueryHeap;
                if (FAILED(m_Context.pDevice->CreateQueryHeap(&Desc, IID_PPV_ARGS(pTimerQueryHeap.GetAddressOf())))) return false;

                FBufferDesc BufferDesc{};
                BufferDesc.stByteSize = static_cast<UINT64>(Desc.Count) * 8;
                BufferDesc.CpuAccess = ECpuAccessMode::Read;
                BufferDesc.strName = "TimerQueryResolveBuffer";

                FDX12Buffer* pBuffer = new FDX12Buffer(&m_Context, m_pDescriptorHeaps.get(), BufferDesc);
                if (!pBuffer->Initialize() || !pBuffer->QueryInterface(IID_IBuffer, PPV_ARG(m_Context.pTimerQueryResolveBuffer.GetAddressOf())))
                {
                    LOG_ERROR("Call to IDevice::CreateBuffer failed.");
                    return false;
                }
            }  
        }

        FDX12TimerQuery* pTimerQuery = new FDX12TimerQuery(&m_Context, m_pDescriptorHeaps.get(), m_pDescriptorHeaps->TimeQueries.Allocate());
        if (!pTimerQuery->Initialize() || !pTimerQuery->QueryInterface(criid, ppvTimeQuery))
        {
            LOG_ERROR("Call to IDevice::CreateTimerQuery failed.");
            return false;
        }
        return true;
    }

    BOOL FDX12Device::PollTimerQuery(ITimerQuery* pQuery)
    {
        FDX12TimerQuery* pDX12TimerQuery = CheckedCast<FDX12TimerQuery*>(pQuery);
        ReturnIfFalse(pDX12TimerQuery->Poll());
        return true;
    }

    BOOL FDX12Device::GetTimerQueryTime(ITimerQuery* pQuery, FLOAT* pfTimeInSeconds)
    {
        FDX12TimerQuery* pDX12TimerQuery = CheckedCast<FDX12TimerQuery*>(pQuery);

        UINT64 stFrequency = 0;
        GetQueue(ECommandQueueType::Graphics)->pD3D12CommandQueue->GetTimestampFrequency(&stFrequency);
        *pfTimeInSeconds = pDX12TimerQuery->GetTime(m_FenceEvent, stFrequency);

        return true;
    }

    BOOL FDX12Device::ResetTimerQuery(ITimerQuery* pQuery)
    {
        FDX12TimerQuery* pDX12TimerQuery = CheckedCast<FDX12TimerQuery*>(pQuery);

        pDX12TimerQuery->Reset();

        return true;
    }
    
    EGraphicsAPI FDX12Device::GetGraphicsAPI() const
    {
        return EGraphicsAPI::D3D12;
    }

    void* FDX12Device::GetNativeDescriptorHeap(EDescriptorHeapType Type) const
    {
        switch (Type)
        {
        case EDescriptorHeapType::RenderTargetView: return static_cast<void*>(m_pDescriptorHeaps->RenderTargetHeap.GetShaderVisibleHeap());
        case EDescriptorHeapType::DepthStencilView: return static_cast<void*>(m_pDescriptorHeaps->DepthStencilHeap.GetShaderVisibleHeap());
        case EDescriptorHeapType::ShaderResourceView: return static_cast<void*>(m_pDescriptorHeaps->ShaderResourceHeap.GetShaderVisibleHeap());
        case EDescriptorHeapType::Sampler: return static_cast<void*>(m_pDescriptorHeaps->SamplerHeap.GetShaderVisibleHeap());      
        default: return nullptr;
        }
    }

    void* FDX12Device::GetNativeObject() const
    {
        return static_cast<void*>(m_Desc.pD3D12Device);
    }

    
    BOOL FDX12Device::CreateFrameBuffer(const FFrameBufferDesc& crDesc, CREFIID criid, void** ppvFrameBuffer)
    {
        FDX12FrameBuffer* pFrameBuffer = new FDX12FrameBuffer(&m_Context, m_pDescriptorHeaps.get(), crDesc);
        if (!pFrameBuffer->Initialize() || !pFrameBuffer->QueryInterface(criid, ppvFrameBuffer))
        {
            LOG_ERROR("Call to IDevice::CreateFrameBuffer failed.");
            return false;
        }
        return true;
    }

    BOOL FDX12Device::CreateGraphicsPipeline(const FGraphicsPipelineDesc& crDesc, IFrameBuffer* pFrameBuffer, CREFIID criid, void** ppvGraphicsPipeline)
    {
        BOOL bAllowInputLayout = crDesc.pInputLayout != nullptr;

        UINT64 stHash = 0;
        for (UINT32 ix = 0; ix < crDesc.pBindingLayouts.Size(); ++ix)
        {
            HashCombine(stHash, crDesc.pBindingLayouts[ix]);
        }

        HashCombine(stHash, bAllowInputLayout ? 1u : 0u);

        TComPtr<IDX12RootSignature> pDX12RootSignature = m_pDescriptorHeaps->RootSignatureMap[stHash];

        if (pDX12RootSignature == nullptr)
        {
            ReturnIfFalse(BuildRootSignature(
                crDesc.pBindingLayouts, 
                bAllowInputLayout, 
                IID_IDX12RootSignature, 
                PPV_ARG(&pDX12RootSignature)
            ));

            FDX12RootSignature* pTempRootSignature = CheckedCast<FDX12RootSignature*>(pDX12RootSignature.Get());

            pTempRootSignature->m_dwHashIndex = stHash;
            m_pDescriptorHeaps->RootSignatureMap[stHash] = pDX12RootSignature.Get();
        }

        FDX12GraphicsPipeline* pGraphicsPipeline = new FDX12GraphicsPipeline(&m_Context, crDesc, pDX12RootSignature, pFrameBuffer->GetInfo());
        if (!pGraphicsPipeline->Initialize() || !pGraphicsPipeline->QueryInterface(criid, ppvGraphicsPipeline))
        {
            LOG_ERROR("Call to IDevice::CreateGraphicsPipeline failed.");
            return false;
        }
        return true;
    }

    BOOL FDX12Device::CreateComputePipeline(const FComputePipelineDesc& crDesc, CREFIID criid, void** ppvComputePipeline)
    {
        BOOL bAllowInputLayout = false;

        UINT64 stHash = 0;
        for (const auto& crpBindingLayout : crDesc.pBindingLayouts)
        {
            HashCombine(stHash, crpBindingLayout);
        }

        HashCombine(stHash, bAllowInputLayout ? 1u : 0u);

        TComPtr<IDX12RootSignature> pDX12RootSignature = m_pDescriptorHeaps->RootSignatureMap[stHash];

        if (pDX12RootSignature == nullptr)
        {
            ReturnIfFalse(BuildRootSignature(
                crDesc.pBindingLayouts, 
                bAllowInputLayout, 
                IID_IDX12RootSignature, 
                PPV_ARG(&pDX12RootSignature)
            ));

            FDX12RootSignature* pTempRootSignature = CheckedCast<FDX12RootSignature*>(pDX12RootSignature.Get());

            pTempRootSignature->m_dwHashIndex = stHash;
            m_pDescriptorHeaps->RootSignatureMap[stHash] = pDX12RootSignature.Get();
        }


        FDX12ComputePipeline* pComputePipeline = new FDX12ComputePipeline(&m_Context, crDesc, pDX12RootSignature);
        if (!pComputePipeline->Initialize() || !pComputePipeline->QueryInterface(criid, ppvComputePipeline))
        {
            LOG_ERROR("Call to IDevice::CreateComputePipeline failed.");
            return false;
        }
        return true;
    }

    BOOL FDX12Device::CreateBindingLayout(const FBindingLayoutDesc& crDesc, CREFIID criid, void** ppvBindingLayout)
    {
        FDX12BindingLayout* pBindingLayout = new FDX12BindingLayout(&m_Context, crDesc);
        if (!pBindingLayout->Initialize() || !pBindingLayout->QueryInterface(criid, ppvBindingLayout))
        {
            LOG_ERROR("Call to IDevice::CreateBindingLayout failed.");
            return false;
        }
        return true;
    }

    BOOL FDX12Device::CreateBindlessLayout(const FBindlessLayoutDesc& crDesc, CREFIID criid, void** ppvBindlessLayout)
    {
        FDX12BindlessLayout* pBindlessLayout = new FDX12BindlessLayout(&m_Context, crDesc);
        if (!pBindlessLayout->Initialize() || !pBindlessLayout->QueryInterface(criid, ppvBindlessLayout))
        {
            LOG_ERROR("Call to IDevice::CreateBindlessLayout failed.");
            return false;
        }
        return true;
    }

    BOOL FDX12Device::CreateBindingSet(const FBindingSetDesc& crDesc, IBindingLayout* pLayout, CREFIID criid, void** ppvBindingSet)
    {
        FDX12BindingSet* pBindingSet = new FDX12BindingSet(&m_Context, m_pDescriptorHeaps.get(), crDesc, pLayout);
        if (!pBindingSet->Initialize() || !pBindingSet->QueryInterface(criid, ppvBindingSet))
        {
            LOG_ERROR("Call to IDevice::CreateBindingSet failed.");
            return false;
        }
        return true;
    }
    
    BOOL FDX12Device::CreateBindlessSet(IBindingLayout* pLayout, CREFIID criid, void** ppvBindlessSet)
    {
        // pLayout is useless on dx12.

        FDX12BindlessSet* pBindlessSet = new FDX12BindlessSet(&m_Context, m_pDescriptorHeaps.get());
        if (!pBindlessSet->Initialize() || !pBindlessSet->QueryInterface(criid, ppvBindlessSet))
        {
            LOG_ERROR("Call to IDevice::CreateDescriptorTable failed.");
            return false;
        }
        return true;
    }

    BOOL FDX12Device::CreateCommandList(const FCommandListDesc& crDesc, CREFIID criid, void** ppvCmdList)
    {
        FDX12CommandList* pCommandList = new FDX12CommandList(&m_Context, m_pDescriptorHeaps.get(), this, GetQueue(crDesc.QueueType), crDesc);
        if (!pCommandList || !pCommandList->Initialize() || !pCommandList->QueryInterface(criid, ppvCmdList))
        {
            LOG_ERROR("Call to IDevice::CreateCommandList failed.");
            return false;
        }
        return true;
    }

    UINT64 FDX12Device::ExecuteCommandLists(ICommandList* const* pcpCommandLists, UINT64 stCommandListsNum, ECommandQueueType ExecutionQueueType)
    {
        m_pD3D12CmdListsToExecute.resize(stCommandListsNum);
        for (UINT64 ix = 0; ix < stCommandListsNum; ++ix)
        {
            FDX12CommandList* pDX12CmdList = CheckedCast<FDX12CommandList*>(pcpCommandLists[ix]);
            m_pD3D12CmdListsToExecute[ix] = pDX12CmdList->m_pActiveCmdList->pD3D12CommandList.Get();
        }

        FDX12CommandQueue* pCmdQueue = GetQueue(ExecutionQueueType);
        pCmdQueue->pD3D12CommandQueue->ExecuteCommandLists(static_cast<UINT32>(m_pD3D12CmdListsToExecute.size()), m_pD3D12CmdListsToExecute.data());
        pCmdQueue->stLastSubmittedValue++;
        pCmdQueue->pD3D12CommandQueue->Signal(pCmdQueue->pD3D12Fence.Get(), pCmdQueue->stLastSubmittedValue);

        for (UINT64 ix = 0; ix < stCommandListsNum; ++ix)
        {
            FDX12CommandList* pDX12CmdList = CheckedCast<FDX12CommandList*>(pcpCommandLists[ix]);

            pCmdQueue->pCommandListsInFlight.push_front(pDX12CmdList->Executed(
                pCmdQueue->pD3D12Fence.Get(),
                pCmdQueue->stLastSubmittedValue
            ));
        }

        if (FAILED(m_Context.pDevice->GetDeviceRemovedReason()))
        {
            LOG_CRITICAL("Device removed.");
            return INVALID_SIZE_64;
        }

        return pCmdQueue->stLastSubmittedValue;
    }

    BOOL FDX12Device::QueueWaitForCommandList(ECommandQueueType WaitQueueType, ECommandQueueType ExecutionQueueType, UINT64 stInstance)
    {
        FDX12CommandQueue* pWaitQueue = GetQueue(WaitQueueType);
        FDX12CommandQueue* pExecutionQueue = GetQueue(ExecutionQueueType);
        ReturnIfFalse(stInstance <= pExecutionQueue->stLastSubmittedValue);

        if (FAILED(pWaitQueue->pD3D12CommandQueue->Wait(pExecutionQueue->pD3D12Fence.Get(), stInstance))) return false;   
        return true;
    }

#if RAY_TRACING
	BOOL FDX12Device::CreateRayTracingPipeline(const RayTracing::FPipelineDesc& crDesc, CREFIID criid, void** ppvPipeline)
	{

        return true;
	}

	BOOL FDX12Device::CreateAccelStruct(const RayTracing::FAccelStructDesc& crDesc, CREFIID criid, void** ppvAccelStruct)
	{

		return true;
	}

	FMemoryRequirements FDX12Device::GetAccelStructMemoryRequirements(RayTracing::IAccelStruct* pAccelStruct)
	{

        return FMemoryRequirements{};
	}

	BOOL FDX12Device::BindAccelStructMemory(RayTracing::IAccelStruct* pAccelStruct, IHeap* pHeap, UINT64 stOffset /*= 0*/)
	{

		return true;
	}
#endif

	void FDX12Device::WaitForIdle()
    {
        for (const auto& crpQueue : m_pCmdQueues)
        {
            if (crpQueue == nullptr) continue;

            if (crpQueue->UpdateLastCompletedValue() < crpQueue->stLastSubmittedValue)
            {
                WaitForFence(crpQueue->pD3D12Fence.Get(), crpQueue->stLastSubmittedValue, m_FenceEvent);
            }
        }
    }
    
    void FDX12Device::RunGarbageCollection()
    {
        for (const auto& crpQueue : m_pCmdQueues)
        {
            if (crpQueue == nullptr) continue;
            
            crpQueue->UpdateLastCompletedValue();

            while (!crpQueue->pCommandListsInFlight.empty())
            {
                if (crpQueue->stLastCompletedValue >= crpQueue->pCommandListsInFlight.back()->stSubmittedValue) 
                {
                    crpQueue->pCommandListsInFlight.pop_back();
                }
                else
                {
                    break;
                }
            }
        }
    }


    BOOL FDX12Device::BuildRootSignature(
        const FPipelineBindingLayoutArray& crpBindingLayouts,
        BOOL bAllowInputLayout,
        CREFIID criid,
        void** ppvRootSignature
    ) const
    {
        FDX12RootSignature* pRootSignature = new FDX12RootSignature(&m_Context, m_pDescriptorHeaps.get());
        if (!pRootSignature || 
            !pRootSignature->Initialize(
                crpBindingLayouts, 
                bAllowInputLayout
            ) || 
            !pRootSignature->QueryInterface(criid, ppvRootSignature)
        )
        {
            LOG_ERROR("Call to IDevice::CreateRootSignature failed.");
            return false;
        }
        return true;
    }

    FDX12CommandQueue* FDX12Device::GetQueue(ECommandQueueType Type) const
    {
        return m_pCmdQueues[static_cast<UINT8>(Type)].get();
    }


}