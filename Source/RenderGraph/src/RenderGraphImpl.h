#ifndef RENDER_GRAPH_IMPL_H
#define RENDER_GRAPH_IMPL_H

#include "../include/RenderGraph.h"

#include "../../Core/include/ComRoot.h"
#include "../../Core/include/ComCli.h"
#include "../../DynamicRHI/include/Device.h"

namespace FTS 
{
    enum class EPassAsyncType
    {
        None        = 0,
        Wait        = 0x001,
        Signal      = 0x002,
        WaitSignal  = 0x004
    };
    FTS_ENUM_CLASS_FLAG_OPERATORS(EPassAsyncType)


    class FRenderGraph :
        public TComObjectRoot<FComMultiThreadModel>,
        public IRenderGraph
    {
    public:
        BEGIN_INTERFACE_MAP(FRenderGraph)
            INTERFACE_ENTRY(IID_IRenderGraph, IRenderGraph)
        END_INTERFACE_MAP

        FRenderGraph(IDevice* pDevice, std::function<void()> PresentFunc);
        ~FRenderGraph() { Reset(); }

        BOOL Initialize(FWorld* pWorld);

        BOOL Compile() override;
        BOOL Execute() override;
        
        void Reset() override;

        IDevice* GetDevice() const override { return m_pDevice.Get(); }
        IRenderResourceCache* GetResourceCache() const override { return m_pResourceCache.Get(); }

        void AddPass(IRenderPass* pPass) override;

        BOOL Precompute();
    
    private:
        BOOL TopologyPasses(BOOL bIsPrecompute);

    private:
        TComPtr<IDevice> m_pDevice;
        std::function<void()> m_PresentFunc;

        TComPtr<IRenderResourceCache> m_pResourceCache;

        std::vector<EPassAsyncType> m_PassAsyncTypes;

		std::vector<TComPtr<ICommandList>> m_pCmdLists;
		std::vector<TComPtr<ICommandList>> m_pPrecomputeCmdLists;

        std::vector<IRenderPass*> m_pPasses;
        std::vector<IRenderPass*> m_pPrecomputePasses;

        UINT64 m_stGraphicsWaitValue = 0;
        UINT64 m_stComputeWaitValue = 0;

        UINT64 m_stFrameCount = 0;
    };
}


#endif