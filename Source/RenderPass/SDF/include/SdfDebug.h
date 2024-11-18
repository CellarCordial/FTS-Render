#ifndef RENDER_PASS_SDF_MODE_H
#define RENDER_PASS_SDF_MODE_H

#include "../../../DynamicRHI/include/CommandList.h"
#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Vector.h"
#include "../../../Math/include/Bounds.h"
#include "../../../Scene/include/Scene.h"
#include "SdfGenerate.h"
#include "GlobalSdf.h"


namespace FTS 
{
    namespace Constant
    {
		struct SdfDebugPassConstants
		{
			FVector3F FrustumA;                     UINT32 dwMaxTraceSteps = 1024;
			FVector3F FrustumB;                     FLOAT fAbsThreshold = 0.01f;
			FVector3F FrustumC;                     FLOAT fChunkSize = 0.0f;
            FVector3F FrustumD;                     UINT32 dwChunkNumPerAxis = 0;
            FVector3F CameraPosition;               FLOAT fSceneGridSize = 0.0f;
            FVector3F SceneGridOrigin;              FLOAT fMaxGIDistance = 0.0f;
            FLOAT fChunkDiagonal = 0.0f;            FVector3F PAD;
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