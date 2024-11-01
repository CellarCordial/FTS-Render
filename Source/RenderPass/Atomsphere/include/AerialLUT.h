#ifndef RENDER_PASS_AERIAL_LUT_H
#define RENDER_PASS_AERIAL_LUT_H

#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Vector.h"
#include "../../../Math/include/Matrix.h"
#include "AtmosphereProperties.h"


namespace FTS
{
	namespace Constant
	{
		struct AerialLUTPassConstant
		{
			FVector3F SunDir;			FLOAT fSunTheta = 0.0f;
			FVector3F FrustumA;			FLOAT fMaxAerialDistance = 2000.0f;
			FVector3F FrustumB;			INT32 dwPerSliceMarchStepCount = 1;
			FVector3F FrustumC;			FLOAT fCameraHeight = 0.0f;
			FVector3F FrustumD;			UINT32 bEnableMultiScattering = true;
			FVector3F CameraPosiiton;	UINT32 bEnableShadow = true;
			FLOAT fWorldScale = 0.0f;	FVector3F PAD;
			FMatrix4x4 ShadowViewProj;
		};
	}


	class FAerialLUTPass : public IRenderPass
	{
	public:
		FAerialLUTPass() { Type = ERenderPassType::Compute; }

		BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
		BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;

		friend class FAtmosphereRender;

	private:
		BOOL m_bWritedBuffer = false;
		Constant::AerialLUTPassConstant m_PassConstant;

		TComPtr<IBuffer> m_pPassConstantBuffer;
		TComPtr<ITexture> m_pAerialLUTTexture;

		TComPtr<IBindingLayout> m_pBindingLayout;

		TComPtr<IShader> m_pCS;
		TComPtr<IComputePipeline> m_pPipeline;
		
		TComPtr<IBindingSet> m_pBindingSet;
		FComputeState m_ComputeState;
	};


}



















#endif