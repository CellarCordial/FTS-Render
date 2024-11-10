#include "../include/TransmittanceLUT.h"
#include "../../../Shader/ShaderCompiler.h"
#include "../../../Math/include/Common.h"
#include "../../../Gui/include/GuiPanel.h"
#include <string>

namespace FTS
{
#define THREAD_GROUP_SIZE_X 16 
#define THREAD_GROUP_SIZE_Y 16 
#define RAY_STEP_COUNT 1000
#define TRANSMITTANCE_LUT_RES 256

    BOOL FTransmittanceLUTPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
    {
		// Binding Layout.
		{
			FBindingLayoutItemArray BindingLayoutItems(2);
			BindingLayoutItems[0] = FBindingLayoutItem::CreateConstantBuffer(0, false);
			BindingLayoutItems[1] = FBindingLayoutItem::CreateTexture_UAV(0);

			ReturnIfFalse(pDevice->CreateBindingLayout(
				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems },
				IID_IBindingLayout,
				PPV_ARG(m_pBindingLayout.GetAddressOf())
			));
		}

        // Shader.
        {
			FShaderCompileDesc CSCompileDesc;
			CSCompileDesc.strShaderName = "Atmosphere/TransmittanceLUT.hlsl";
			CSCompileDesc.strEntryPoint = "CS";
			CSCompileDesc.Target = EShaderTarget::Compute;
			CSCompileDesc.strDefines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			CSCompileDesc.strDefines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			CSCompileDesc.strDefines.push_back("STEP_COUNT=" + std::to_string(RAY_STEP_COUNT));
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
				FBufferDesc::CreateConstant(sizeof(Constant::AtmosphereProperties), false, "AtmospherePropertiesBuffer"),
				IID_IBuffer,
				PPV_ARG(m_pAtomspherePropertiesBuffer.GetAddressOf())
			));
			ReturnIfFalse(pCache->Collect(m_pAtomspherePropertiesBuffer.Get()));
		}
        
        // Texture.
		{
			FTextureDesc TextureDesc = FTextureDesc::CreateReadWrite(
				TRANSMITTANCE_LUT_RES,
				TRANSMITTANCE_LUT_RES,
				EFormat::RGBA32_FLOAT,
				"TransmittanceTexture"
			);
			ReturnIfFalse(pDevice->CreateTexture(TextureDesc, IID_ITexture, PPV_ARG(m_pTransmittanceTexture.GetAddressOf())));
			ReturnIfFalse(pCache->Collect(m_pTransmittanceTexture.Get()));
		}

		// Binding Set.
		{
			FBindingSetItemArray BindingSetItems(2);
			BindingSetItems[0] = FBindingSetItem::CreateConstantBuffer(0, m_pAtomspherePropertiesBuffer.Get());
			BindingSetItems[1] = FBindingSetItem::CreateTexture_UAV(0, m_pTransmittanceTexture.Get());
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

    BOOL FTransmittanceLUTPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
    {
		ReturnIfFalse(pCmdList->Open());

		// Update Constant.
		{
			ReturnIfFalse(pCache->GetWorld()->Each<Constant::AtmosphereProperties>(
				[this](FEntity* pEntity, Constant::AtmosphereProperties* pProperties) -> BOOL
				{
					m_StandardAtomsphereProperties = pProperties->ToStandardUnit();
					return true;
				}
			));
			ReturnIfFalse(pCmdList->WriteBuffer(m_pAtomspherePropertiesBuffer.Get(), &m_StandardAtomsphereProperties, sizeof(Constant::AtmosphereProperties)));
		}


		FVector2I ThreadGroupNum = {
			static_cast<UINT32>(Align(TRANSMITTANCE_LUT_RES, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X),
			static_cast<UINT32>(Align(TRANSMITTANCE_LUT_RES, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y),
		};

		ReturnIfFalse(pCmdList->SetComputeState(m_ComputeState));
		ReturnIfFalse(pCmdList->Dispatch(ThreadGroupNum.x, ThreadGroupNum.y));

		ReturnIfFalse(pCmdList->Close());
        return true;
    }


}