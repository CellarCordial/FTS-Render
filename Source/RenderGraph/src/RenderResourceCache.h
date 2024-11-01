#ifndef RENDER_GRAPH_RESOURCE_CACHE_H
#define RENDER_GRAPH_RESOURCE_CACHE_H

#include "../include/RenderGraph.h"

#include "../../Core/include/ComRoot.h"
#include "../../Core/include/ComCli.h"
#include <string>
#include <unordered_map>

namespace FTS
{
    
    class FRenderResourceCache : 
        public TComObjectRoot<FComMultiThreadModel>,
        public IRenderResourceCache
    {
    public:
        BEGIN_INTERFACE_MAP(FRenderResourceCache)
            INTERFACE_ENTRY(IID_IRenderResourceCache, IRenderResourceCache)
        END_INTERFACE_MAP

        FRenderResourceCache(FWorld* pWorld) : m_pWorld(pWorld)
        {
        }

        BOOL Initialize();

        BOOL Collect(IResource* pResource) override;
        IResource* Require(const CHAR* pstrName) override;

		BOOL CollectConstants(const CHAR* strName, void* pvData, UINT64 stElementNum = 1) override;
		BOOL RequireConstants(const CHAR* strName, void** ppvData, UINT64* pstElementNum = nullptr) override;

        FWorld* GetWorld() const override { return m_pWorld; }

    private:

        struct ResourceData
        {
            TComPtr<IResource> pResource;
        };

        std::unordered_map<std::string, ResourceData> m_ResourceNameMap;
		std::unordered_map<std::string, std::pair<void*, UINT64>> m_DataNameMap;

        FWorld* m_pWorld;
    };


}













#endif
