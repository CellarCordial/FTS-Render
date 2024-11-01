#ifndef RENDER_PASS_SKY_H
#define RENDER_PASS_SKY_H

#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Vector.h"
#include "../../../Math/include/Matrix.h"
#include "AtmosphereProperties.h"


namespace FTS
{
	namespace Constant
	{
		struct SkyPassConstant
		{
			FVector3F FrustumA; FLOAT PAD0 = 0.0f;
			FVector3F FrustumB; FLOAT PAD1 = 0.0f;
			FVector3F FrustumC; FLOAT PAD2 = 0.0f;
			FVector3F FrustumD; FLOAT PAD3 = 0.0f;
		};
	}

	class FSkyPass : public IRenderPass
	{
	public:
		FSkyPass() { Type = ERenderPassType::Graphics; }

		BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
		BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;

		friend class FAtmosphereRender;

	private:
		Constant::SkyPassConstant m_PassConstant;

		TComPtr<ITexture> m_pDepthTexture;

		TComPtr<ISampler> m_pSampler; // U_Wrap VW_Clamp Linear

		TComPtr<IBindingLayout> m_pBindingLayout;
		
		TComPtr<IShader> m_pVS;
		TComPtr<IShader> m_pPS;
		
		TComPtr<IFrameBuffer> m_pFrameBuffer;
		TComPtr<IGraphicsPipeline> m_pPipeline;
		
		TComPtr<IBindingSet> m_pBindingSet;
		FGraphicsState m_GraphicsState;
	};
}





#endif