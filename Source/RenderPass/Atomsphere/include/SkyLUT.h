#ifndef RENDER_PASS_SKY_LUT_H
#define RENDER_PASS_SKY_LUT_H

#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Vector.h"
#include "../../../Math/include/Matrix.h"
#include "AtmosphereProperties.h"


namespace FTS
{
	namespace Constant
	{
		struct SkyLUTPassConstant
		{
			FVector3F CameraPosition;
			INT32 dwMarchStepCount = 40;

			FVector3F SunDir;
			UINT32 bEnableMultiScattering = 1;

			FVector3F SunIntensity;
			FLOAT PAD = 0.0f;
		};
	}

	class FSkyLUTPass : public IRenderPass
	{
	public:
		FSkyLUTPass() { Type = ERenderPassType::Graphics; }

		BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
		BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;

		friend class FAtmosphereRender;

	private:
		Constant::SkyLUTPassConstant m_PassConstant;

		TComPtr<IBuffer> m_pPassConstantBuffer;
		TComPtr<ITexture> m_pSkyLUTTexture;

		ITexture* pTransmittanceTexture = nullptr;
		ITexture* pMultiScatteringTexture = nullptr;

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