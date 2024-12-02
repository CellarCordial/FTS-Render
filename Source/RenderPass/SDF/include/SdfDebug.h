#ifndef RENDER_PASS_SDF_MODE_H
#define RENDER_PASS_SDF_MODE_H

#include "../../../DynamicRHI/include/CommandList.h"
#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Vector.h"
#include "SdfGenerate.h"
#include "GlobalSdf.h"


namespace FTS 
{
    namespace Constant
    {
        struct GlobalSdfData
        {
            FLOAT fSceneGridSize = 0.0f;
            FVector3F SceneGridOrigin; 

            UINT32 dwMaxTraceSteps = 1024;
            FLOAT AbsThreshold = 0.01f;
            FLOAT fDefaultMarch = 0.0f;
        };

		struct SdfDebugPassConstants
		{
			FVector3F FrustumA;     FLOAT PAD0 = 0.0f;      
			FVector3F FrustumB;     FLOAT PAD1 = 0.0f;      
			FVector3F FrustumC;     FLOAT PAD2 = 0.0f;      
            FVector3F FrustumD;     FLOAT PAD3 = 0.0f;
            FVector3F CameraPosition; FLOAT PAD4 = 0.0f;

            GlobalSdfData SdfData;         
            FLOAT PAD5;
		};
    }


    class FSdfDebugPass : public IRenderPass
    {
	public:
		FSdfDebugPass() { Type = ERenderPassType::Graphics; }

        BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
        BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;

    private:
        std::vector<FLOAT> m_SdfData;
        BOOL m_bResourcesWrited = false;
        Constant::GlobalSdfData m_GlobalSdfData;         
        Constant::SdfDebugPassConstants m_PassConstants;
        
        ITexture* m_pSdfTexture = nullptr;

        TComPtr<IBindingLayout> m_pBindingLayout;
        
        TComPtr<IShader> m_pVS;
        TComPtr<IShader> m_pPS;
        
        TComPtr<IFrameBuffer> m_pFrameBuffer;
        TComPtr<IGraphicsPipeline> m_pPipeline;

        TComPtr<IBindingSet> m_pBindingSet;
        FGraphicsState m_GraphicsState;
    };

	class FSdfDebugRender
	{
    public:
        BOOL Setup(IRenderGraph* pRenderGraph);
        IRenderPass* GetLastPass() { return &m_SdfDebugPass; }

    private:
        FSdfGeneratePass m_SdfGeneratePass;
        FGlobalSdfPass m_GlobalSdfPass;
        FSdfDebugPass m_SdfDebugPass;
	};
}










#endif