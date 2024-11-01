#ifndef RENDER_PASS_ATMOSPHERE_DEBUG_H
#define RENDER_PASS_ATMOSPHERE_DEBUG_H

#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Matrix.h"
#include "../../../Scene/include/Geometry.h"

namespace FTS
{
	namespace Constant
	{
		struct AtmosphereDebugPassConstant0
		{
			FMatrix4x4 ViewProj;
			FMatrix4x4 WorldMatrix;
		};

		struct AtmosphereDebugPassConstant1
		{
			FVector3F SunDirection;
			FLOAT fSunTheta = 0.0f;

			FVector3F SunRadiance;
			FLOAT fMaxAerialDistance = 2000.0f;

			FVector3F CameraPos;
			FLOAT fWorldScale = 0.0f;

			FMatrix4x4 ShadowViewProj;

			FVector2F JitterFactor;
			FVector2F BlueNoiseUVFactor;

			FVector3F GroundAlbedo;
			FLOAT PAD = 0.0f;
		};
	}


	class FAtmosphereDebugPass : public IRenderPass
	{
	public:
		FAtmosphereDebugPass() { Type = ERenderPassType::Graphics; }

		BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache);
		BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache);

		friend class FAtmosphereRender;

	private:
		BOOL m_bWritedResource = false;
		FLOAT m_fJitterRadius = 1.0f;
		Constant::AtmosphereDebugPassConstant0 m_PassConstant0;
		Constant::AtmosphereDebugPassConstant1 m_PassConstant1;
		FImage m_BlueNoiseImage;

		ITexture* m_pAerialLUTTexture = nullptr;		
		ITexture* m_pTransmittanceTexture = nullptr;
		ITexture* m_pMultiScatteringTexture = nullptr;
		ITexture* m_pFinalTexture = nullptr;

		TComPtr<IBuffer> m_pPassConstant1Buffer;
		TComPtr<ITexture> m_pBlueNoiseTexture;

		TComPtr<IBindingLayout> m_pBindingLayout;
		TComPtr<IInputLayout> m_pInputLayout;

		TComPtr<IShader> m_pVS;
		TComPtr<IShader> m_pPS;

		TComPtr<IFrameBuffer> m_pFrameBuffer;
		TComPtr<IGraphicsPipeline> m_pPipeline;

		TComPtr<IBindingSet> m_pBindingSet;
		FGraphicsState m_GraphicsState;

		FDrawArguments* m_DrawArguments = nullptr;
		UINT64 m_stDrawArgumentsSize = 0;
	};

}












#endif