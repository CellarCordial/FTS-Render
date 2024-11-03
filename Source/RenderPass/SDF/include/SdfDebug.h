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
			FVector3F FrustumC;                     FLOAT Pad0;
			FVector3F FrustumD;                     FLOAT Pad1;
			FVector3F CameraPosition;               FLOAT Pad2;
			FVector3F SdfLower = FVector3F(-1.2f);  FLOAT Pad3;
			FVector3F SdfUpper = FVector3F(-1.2f);  FLOAT Pad4;
			FVector3F SdfExtent;                    FLOAT Pad5;
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
        
        TComPtr<ITexture> m_pSdfTexture;

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