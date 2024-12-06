#include "../include/MultiScatteringLUT.h"
#include "../../../Shader/ShaderCompiler.h"
#include "../../../Scene/include/Light.h"
#include <cySampleElim.h>
#include <cyPoint.h>
#include <string>
#include <random>
#include <vector>
    
namespace FTS 
{
#define DIRECTION_SAMPLE_COUNT 64
#define THREAD_GROUP_SIZE_X 16 
#define THREAD_GROUP_SIZE_Y 16 
#define MULTI_SCATTERING_LUT_RES 256

    void FMultiScatteringLUTPass::CreatePoissonDiskSamples()
    {
		std::default_random_engine RandomEngine{ std::random_device()() };
		std::uniform_real_distribution<FLOAT> Distribution(0, 1);

		std::vector<cy::Point2f> rawPoints;
		for (UINT32 ix = 0; ix < DIRECTION_SAMPLE_COUNT * 10; ++ix)
		{
			rawPoints.push_back({ Distribution(RandomEngine), Distribution(RandomEngine) });
		}

		std::vector<cy::Point2f> outputPoints(DIRECTION_SAMPLE_COUNT);

		cy::WeightedSampleElimination<cy::Point2f, FLOAT, 2> Elimination;
		Elimination.SetTiling(true);
		Elimination.Eliminate(
			rawPoints.data(), rawPoints.size(),
			outputPoints.data(), outputPoints.size()
		);

		for (auto& p : outputPoints) m_DirSamples.push_back({ p.x, p.y });
    }


    BOOL FMultiScatteringLUTPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
    {
		// Binding Layout.
		{
			FBindingLayoutItemArray BindingLayoutItems(6);
			BindingLayoutItems[0] = FBindingLayoutItem::CreateConstantBuffer(0, false);
			BindingLayoutItems[1] = FBindingLayoutItem::CreateConstantBuffer(1);
			BindingLayoutItems[2] = FBindingLayoutItem::CreateTexture_SRV(0);
			BindingLayoutItems[3] = FBindingLayoutItem::CreateStructuredBuffer_SRV(1);
			BindingLayoutItems[4] = FBindingLayoutItem::CreateTexture_UAV(0);
			BindingLayoutItems[5] = FBindingLayoutItem::CreateSampler(0);
			ReturnIfFalse(pDevice->CreateBindingLayout(
				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems },
				IID_IBindingLayout,
				PPV_ARG(m_pBindingLayout.GetAddressOf())
			));
		}

        // Shader.
		{
			FShaderCompileDesc CSCompileDesc;
			CSCompileDesc.strShaderName = "Atmosphere/MultiScatteringLUT.hlsl";
			CSCompileDesc.strEntryPoint = "CS";
			CSCompileDesc.Target = EShaderTarget::Compute;
			CSCompileDesc.strDefines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			CSCompileDesc.strDefines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			CSCompileDesc.strDefines.push_back("DIRECTION_SAMPLE_COUNT=" + std::to_string(DIRECTION_SAMPLE_COUNT));
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
                FBufferDesc::CreateConstant(sizeof(Constant::MultiScatteringPassConstant)), 
                IID_IBuffer, 
                PPV_ARG(m_pPassConstantBuffer.GetAddressOf())
            ));
			ReturnIfFalse(pDevice->CreateBuffer(
                FBufferDesc::CreateStructured(sizeof(FVector2I) * DIRECTION_SAMPLE_COUNT, sizeof(FVector2I), true), 
                IID_IBuffer, 
                PPV_ARG(m_pDirScampleBuffer.GetAddressOf())
            ))

		}

		// Texture.
		{
			ReturnIfFalse(pDevice->CreateTexture(
				FTextureDesc::CreateReadWrite(
					MULTI_SCATTERING_LUT_RES,
					MULTI_SCATTERING_LUT_RES,
					EFormat::RGBA32_FLOAT,
					"MultiScatteringTexture"
				),
				IID_ITexture,
				PPV_ARG(m_pMultiScatteringTexture.GetAddressOf())
			));
			ReturnIfFalse(pCache->Collect(m_pMultiScatteringTexture.Get()));
		}

		// Binding Set.
		{
			ISampler* pLinearClampSampler;
			IBuffer* pAtmospherePropertiesBuffer;
			ITexture* pTransmittanceTexture;
			ReturnIfFalse(pCache->Require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));
			ReturnIfFalse(pCache->Require("AtmospherePropertiesBuffer")->QueryInterface(IID_IBuffer, PPV_ARG(&pAtmospherePropertiesBuffer)));
			ReturnIfFalse(pCache->Require("TransmittanceTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pTransmittanceTexture)));

			FBindingSetItemArray BindingSetItems(6);
			BindingSetItems[0] = FBindingSetItem::CreateConstantBuffer(0, pAtmospherePropertiesBuffer);
			BindingSetItems[1] = FBindingSetItem::CreateConstantBuffer(1, m_pPassConstantBuffer.Get());
			BindingSetItems[2] = FBindingSetItem::CreateTexture_SRV(0, pTransmittanceTexture);
			BindingSetItems[3] = FBindingSetItem::CreateStructuredBuffer_SRV(1, m_pDirScampleBuffer.Get());
			BindingSetItems[4] = FBindingSetItem::CreateTexture_UAV(0, m_pMultiScatteringTexture.Get());
			BindingSetItems[5] = FBindingSetItem::CreateSampler(0, pLinearClampSampler);
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

    BOOL FMultiScatteringLUTPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
    {
		ReturnIfFalse(pCmdList->Open());

		// Update Constant.
		{
			ReturnIfFalse(pCache->GetWorld()->Each<FDirectionalLight>(
				[this](FEntity* pEntity, FDirectionalLight* pLight) -> BOOL
				{
					m_PassConstants.SunIntensity = FVector3F(pLight->fIntensity * pLight->Color);
					return true;
				}
			));
			FVector3F* pGroundAlbedo;
			ReturnIfFalse(pCache->RequireConstants("GroundAlbedo", PPV_ARG(&pGroundAlbedo)));
			m_PassConstants.GroundAlbedo = *pGroundAlbedo;

			ReturnIfFalse(pCmdList->WriteBuffer(m_pPassConstantBuffer.Get(), &m_PassConstants, sizeof(Constant::MultiScatteringPassConstant)));
		}
		
		if (!m_bResourceWrited)
		{
			ReturnIfFalse(pCmdList->WriteBuffer(m_pDirScampleBuffer.Get(), m_DirSamples.data(), m_DirSamples.size() * sizeof(FVector2F)));
			m_bResourceWrited = true;
		}

		FVector2I ThreadGroupNum = {
			static_cast<UINT32>(Align(MULTI_SCATTERING_LUT_RES, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X),
			static_cast<UINT32>(Align(MULTI_SCATTERING_LUT_RES, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y),
		};

		ReturnIfFalse(pCmdList->SetComputeState(m_ComputeState));
		ReturnIfFalse(pCmdList->Dispatch(ThreadGroupNum.x, ThreadGroupNum.y));

		ReturnIfFalse(pCmdList->Close());

        return true;
    }

	BOOL FMultiScatteringLUTPass::FinishPass()
	{
		if (!m_DirSamples.empty())
		{
			m_DirSamples.clear();
			m_DirSamples.shrink_to_fit();
		}

		return true;
	}

}
