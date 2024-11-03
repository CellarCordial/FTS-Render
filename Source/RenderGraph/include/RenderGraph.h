#ifndef RENDER_GRAPH_H
#define RENDER_GRAPH_H

#include "../../Core/include/ComIntf.h"
#include "../../DynamicRHI/include/Device.h"
#include "../../Core/include/Entity.h"
#include <unordered_set>

namespace FTS 
{
    enum class ERenderPassType : UINT8
    {
        Invalid,
        Graphics,
        Compute,
        
        Precompute,
		Exclude
    };  
	FTS_ENUM_CLASS_FLAG_OPERATORS(ERenderPassType);

    
    extern const IID IID_IRenderResourceCache;
    struct IRenderResourceCache : public IUnknown
    {
        virtual BOOL Collect(IResource* pResource) = 0;
        virtual IResource* Require(const CHAR* strName) = 0;
		
        virtual BOOL CollectConstants(const CHAR* strName, void* pvData, UINT64 stElementNum = 1) = 0;
		virtual BOOL RequireConstants(const CHAR* strName, void** ppvData, UINT64* pstElementNum = nullptr) = 0;

        virtual FWorld* GetWorld() const = 0;

		virtual ~IRenderResourceCache() = default;
    };

    
    struct IRenderPass
    {
        virtual ~IRenderPass() = default;

        virtual BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) = 0;
        virtual BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) = 0;
        
        virtual BOOL FinishPass() { return true; }

        void Precede(IRenderPass* pPass) 
        {
            if (pPass && pPass->dwIndex != INVALID_SIZE_32)
			{
				SuccessorsIndex.insert(pPass->dwIndex);
				pPass->DependentsIndex.insert(dwIndex);
            }
        }

        void Succeed(IRenderPass* pPass) 
        {
            if (pPass && pPass->dwIndex != INVALID_SIZE_32)
			{
				DependentsIndex.insert(pPass->dwIndex);
				pPass->SuccessorsIndex.insert(dwIndex);
            }
        }

        std::unordered_set<UINT32> DependentsIndex;
        std::unordered_set<UINT32> SuccessorsIndex;
        ERenderPassType Type = ERenderPassType::Invalid;
        UINT32 dwIndex = INVALID_SIZE_32;
    };


    extern const IID IID_IRenderGraph;
    struct IRenderGraph : public IUnknown
    {
        virtual BOOL Compile() = 0;
        virtual BOOL Execute() = 0;

        virtual void Reset() = 0;
        virtual void AddPass(IRenderPass* pPass) = 0;

        virtual IDevice* GetDevice() const = 0;
        virtual IRenderResourceCache* GetResourceCache() const = 0;

		virtual ~IRenderGraph() = default;
    };

    BOOL CreateRenderGraph(IDevice* pDevice, std::function<void()> PresentFunc, FWorld* pWorld, CREFIID criid, void** ppvRenderGraph);
}


















#endif