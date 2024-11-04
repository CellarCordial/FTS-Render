#ifndef RENDER_PASS_SDF_MODE_H
#define RENDER_PASS_SDF_MODE_H

#include "../../../DynamicRHI/include/CommandList.h"
#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Vector.h"
#include "../../../Math/include/Bounds.h"
#include "../../../Scene/include/Camera.h"
#include "SdfGenerate.h"


namespace FTS 
{
    namespace Constant
    {
		struct SdfDebugPassConstants
		{
			FVector3F FrustumA;                     UINT32 dwMaxTraceSteps = 1024;
			FVector3F FrustumB;                     FLOAT fAbsThreshold = 0.01f;
			FVector3F FrustumC;                     FLOAT Pad0 = 0.0f;
			FVector3F FrustumD;                     FLOAT Pad1 = 0.0f;
			FVector3F CameraPosition;               FLOAT Pad2 = 0.0f;
			FVector3F SdfLower = FVector3F(-2.4f);  FLOAT Pad3 = 0.0f;
			FVector3F SdfUpper = FVector3F(2.4f);   FLOAT Pad4 = 0.0f;
			FVector3F SdfExtent;                    FLOAT Pad5 = 0.0f;
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
        
        ITexture* m_pSdfTexture;

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
        FSdfDebugPass m_SdfDebugPass;
	};
}










#endif