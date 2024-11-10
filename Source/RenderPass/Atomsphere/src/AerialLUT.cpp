#include "../include/AerialLUT.h"
#include "../../../Shader/ShaderCompiler.h"
#include "../../../Gui/include/GuiPanel.h"
#include "../../../Scene/include/Light.h"
#include "../../../Scene/include/Camera.h"
#include <string>
#include <vector>

namespace FTS
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
#define AERIAL_LUT_RES_X 200
#define AERIAL_LUT_RES_Y 150
#define AERIAL_LUT_RES_Z 32

	BOOL FAerialLUTPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
	{
		// BindingLayout.
		{
			FBindingLayoutItemArray BindingLayoutItems(8);
			BindingLayoutItems[0] = FBindingLayoutItem::CreateConstantBuffer(0, false);
			BindingLayoutItems[1] = FBindingLayoutItem::CreateConstantBuffer(1);
			BindingLayoutItems[2] = FBindingLayoutItem::CreateTexture_SRV(0);
			BindingLayoutItems[3] = FBindingLayoutItem::CreateTexture_SRV(1);
			BindingLayoutItems[4] = FBindingLayoutItem::CreateTexture_SRV(2);
			BindingLayoutItems[5] = FBindingLayoutItem::CreateSampler(0);
			BindingLayoutItems[6] = FBindingLayoutItem::CreateSampler(1);
			BindingLayoutItems[7] = FBindingLayoutItem::CreateTexture_UAV(0);
			ReturnIfFalse(pDevice->CreateBindingLayout(
				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems },
				IID_IBindingLayout,
				PPV_ARG(m_pBindingLayout.GetAddressOf())
			));
		}

		// Shader.
		{
			FShaderCompileDesc CSCompileDesc;
			CSCompileDesc.strShaderName = "Atmosphere/AerialLUT.hlsl";
			CSCompileDesc.strEntryPoint = "CS";
			CSCompileDesc.Target = EShaderTarget::Compute;
			CSCompileDesc.strDefines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			CSCompileDesc.strDefines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			FShaderData CSData = ShaderCompile::CompileShader(CSCompileDesc);

			FShaderDesc CSDesc;
			CSDesc.strEntryName = "CS";
			CSDesc.ShaderType = EShaderType::Compute;
			ReturnIfFalse(pDevice->CreateShader(CSDesc, CSData.Data(), CSData.Size(), IID_IShader, PPV_ARG(m_pCS.GetAddressOf())));
		}

		// Pipeline.
		{
			FComputePipelineDesc PipelineDesc;
			PipelineDesc.CS = m_pCS.Get();
			PipelineDesc.pBindingLayouts.PushBack(m_pBindingLayout.Get());
			ReturnIfFalse(pDevice->CreateComputePipeline(PipelineDesc, IID_IComputePipeline, PPV_ARG(m_pPipeline.GetAddressOf())));
		}

		// Buffer.
		{
			ReturnIfFalse(pDevice->CreateBuffer(
				FBufferDesc::CreateConstant(sizeof(Constant::AerialLUTPassConstant)), 
				IID_IBuffer, 
				PPV_ARG(m_pPassConstantBuffer.GetAddressOf())
			));
		}

		// Texture.
		{
			ReturnIfFalse(pDevice->CreateTexture(
				FTextureDesc::CreateReadWrite(
					AERIAL_LUT_RES_X,
					AERIAL_LUT_RES_Y,
					AERIAL_LUT_RES_Z,
					EFormat::RGBA32_FLOAT,
					"AerialLUTTexture"
				),
				IID_ITexture,
				PPV_ARG(m_pAerialLUTTexture.GetAddressOf())
			));
			ReturnIfFalse(pCache->Collect(m_pAerialLUTTexture.Get()));
		}

		// Binding Set.
		{
			IBuffer* pAtmospherePropertiesBuffer;
			ITexture *pTransmittanceTexture, *pMultiScatteringTexture, *pShadowMapTexture;
			ISampler* pLinearClampSampler, *pPointClampSampler;
			ReturnIfFalse(pCache->Require("AtmospherePropertiesBuffer")->QueryInterface(IID_IBuffer, PPV_ARG(&pAtmospherePropertiesBuffer)));
			ReturnIfFalse(pCache->Require("TransmittanceTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pTransmittanceTexture)));
			ReturnIfFalse(pCache->Require("MultiScatteringTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pMultiScatteringTexture)));
			ReturnIfFalse(pCache->Require("ShadowMapTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pShadowMapTexture)));
			ReturnIfFalse(pCache->Require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));
			ReturnIfFalse(pCache->Require("PointClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointClampSampler)));

			FBindingSetItemArray BindingSetItems(8);
			BindingSetItems[0] = FBindingSetItem::CreateConstantBuffer(0, pAtmospherePropertiesBuffer);
			BindingSetItems[1] = FBindingSetItem::CreateConstantBuffer(1, m_pPassConstantBuffer.Get());
			BindingSetItems[2] = FBindingSetItem::CreateTexture_SRV(0, pMultiScatteringTexture);
			BindingSetItems[3] = FBindingSetItem::CreateTexture_SRV(1, pTransmittanceTexture);
			BindingSetItems[4] = FBindingSetItem::CreateTexture_SRV(2, pShadowMapTexture);
			BindingSetItems[5] = FBindingSetItem::CreateSampler(0, pLinearClampSampler);
			BindingSetItems[6] = FBindingSetItem::CreateSampler(1, pPointClampSampler);
			BindingSetItems[7] = FBindingSetItem::CreateTexture_UAV(0, m_pAerialLUTTexture.Get());
			ReturnIfFalse(pDevice->CreateBindingSet(
				FBindingSetDesc{ .BindingItems = BindingSetItems },
				m_pBindingLayout.Get(),
				IID_IBindingSet,
				PPV_ARG(m_pBindingSet.GetAddressOf())
			));
		}

		// Compute State.
		{
			m_ComputeState.pBindingSets.PushBack(m_pBindingSet.Get());
			m_ComputeState.pPipeline = m_pPipeline.Get();
		}

		return true;
	}

	BOOL FAerialLUTPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		ReturnIfFalse(pCmdList->Open());

		// Update Constant.
		{
			FLOAT* pfWorldScale;
			ReturnIfFalse(pCache->RequireConstants("WorldScale", PPV_ARG(&pfWorldScale)));
			m_PassConstant.fWorldScale = *pfWorldScale;

			ReturnIfFalse(pCache->GetWorld()->Each<FCamera>(
				[this](FEntity* pEntity, FCamera* pCamera) -> BOOL
				{
					m_PassConstant.fCameraHeight = m_PassConstant.fWorldScale * pCamera->Position.y;
					m_PassConstant.CameraPosiiton = pCamera->Position;

					auto Frustum = pCamera->GetFrustumDirections();
					m_PassConstant.FrustumA = Frustum.A;
					m_PassConstant.FrustumB = Frustum.B;
					m_PassConstant.FrustumC = Frustum.C;
					m_PassConstant.FrustumD = Frustum.D;
					return true;
				}
			));
			ReturnIfFalse(pCache->GetWorld()->Each<FDirectionalLight>(
				[this](FEntity* pEntity, FDirectionalLight* pLight) -> BOOL
				{
					m_PassConstant.SunDir = pLight->Direction;
					m_PassConstant.fSunTheta = std::asin(-pLight->Direction.y);
					m_PassConstant.ShadowViewProj = pLight->ViewProj;
					return true;
				}
			));

			ReturnIfFalse(pCmdList->WriteBuffer(m_pPassConstantBuffer.Get(), &m_PassConstant, sizeof(Constant::AerialLUTPassConstant)));
		}


		FVector2I ThreadGroupNum = {
			static_cast<UINT32>(Align(AERIAL_LUT_RES_X, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X),
			static_cast<UINT32>(Align(AERIAL_LUT_RES_Y, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y),
		};

		ReturnIfFalse(pCmdList->SetComputeState(m_ComputeState));
		ReturnIfFalse(pCmdList->Dispatch(ThreadGroupNum.x, ThreadGroupNum.y));

		ReturnIfFalse(pCmdList->Close());
		return true;
	}
}
