#include "../include/GlobalSdf.h"
#include "../../../Shader/ShaderCompiler.h"
#include "../../../Gui/include/GuiPanel.h"
#include "../../../Scene/include/Scene.h"

namespace FTS
{
#define THREAD_GROUP_SIZE_X 8
#define THREAD_GROUP_SIZE_Y 8
#define THREAD_GROUP_SIZE_Z 8
#define GLOBAL_SDF_RESOLUTION 256

	BOOL FGlobalSdfPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
	{
		// Binding Layout.
		{
			FBindingLayoutItemArray BindingLayoutItems(4);
			BindingLayoutItems[0] = FBindingLayoutItem::CreatePushConstants(0, sizeof(Constant::GlobalSdfConstants));
			BindingLayoutItems[1] = FBindingLayoutItem::CreateSampler(0);
			BindingLayoutItems[2] = FBindingLayoutItem::CreateTexture_UAV(0);
			BindingLayoutItems[3] = FBindingLayoutItem::CreateStructuredBuffer_SRV(0);
			ReturnIfFalse(pDevice->CreateBindingLayout(
				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems },
				IID_IBindingLayout,
				PPV_ARG(m_pBindingLayout.GetAddressOf())
			));

			FBindingLayoutItemArray BindlessLayoutItems(1);
			BindlessLayoutItems[0] = FBindingLayoutItem::CreateBindless_SRV();
			ReturnIfFalse(pDevice->CreateBindlessLayout(
				FBindlessLayoutDesc{ 
					.BindingLayoutItems = BindlessLayoutItems, 
					.dwFirstSlot = 1 
				}, 
				IID_IBindingLayout, 
				PPV_ARG(m_pBindlessLayout.GetAddressOf())
			));
		}

		// Shader.
		{
			FShaderCompileDesc CSCompileDesc;
			CSCompileDesc.strShaderName = "SDF/SdfMerge.hlsl";
			CSCompileDesc.strEntryPoint = "CS";
			CSCompileDesc.Target = EShaderTarget::Compute;
			CSCompileDesc.strDefines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			CSCompileDesc.strDefines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			CSCompileDesc.strDefines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Z));
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
			PipelineDesc.pBindingLayouts.PushBack(m_pBindlessLayout.Get());
			ReturnIfFalse(pDevice->CreateComputePipeline(PipelineDesc, IID_IComputePipeline, PPV_ARG(m_pPipeline.GetAddressOf())));
		}


		// Texture.
		{
			ReturnIfFalse(pDevice->CreateTexture(
				FTextureDesc::CreateReadWrite(
					GLOBAL_SDF_RESOLUTION, 
					GLOBAL_SDF_RESOLUTION, 
					GLOBAL_SDF_RESOLUTION, 
					EFormat::R32_FLOAT, 
					"GlobalSdfTexture"
				),
				IID_ITexture,
				PPV_ARG(m_pGlobalSdfTexture.GetAddressOf())
			));
		}

		// Binding Set.
		{
			ISampler* pLinearClampSampler;
			ReturnIfFalse(pCache->Require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));
 
			FBindingSetItemArray BindingSetItems(4);
			BindingSetItems[0] = FBindingSetItem::CreatePushConstants(0, sizeof(Constant::GlobalSdfConstants));
			BindingSetItems[1] = FBindingSetItem::CreateSampler(0, pLinearClampSampler);
			BindingSetItems[2] = FBindingSetItem::CreateTexture_UAV(0, m_pGlobalSdfTexture.Get());
			BindingSetItems[3] = FBindingSetItem::CreateStructuredBuffer_SRV(0, m_pModelSdfDataBuffer.Get());
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
			m_ComputeState.pBindingSets.PushBack(m_pBindlessSet.Get());
			m_ComputeState.pPipeline = m_pPipeline.Get();
		}


		pCache->GetWorld()->Each<Event::OnModelTransform>(
			[this](FEntity* pEntity, Event::OnModelTransform* pEvent) -> BOOL
			{
				pEvent->DelegateEvent.AddEvent(
					[this]() -> BOOL 
					{
						this->Type &= ~ERenderPassType::Exclude; 
						return true;
					}
				);
				return true;
			}
		);

		return true;
	}

	BOOL FGlobalSdfPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		ReturnIfFalse(pCmdList->Open());

		IDevice* pDevice = pCmdList->GetDevice();
		// Update.
		{
			pCache->GetWorld()->Each<FMesh, FDistanceField, FTransform>(
				[this, pCache](FEntity* pEntity, FMesh* pMesh, FDistanceField* pDF, FTransform* pTransform) -> BOOL
				{
					m_ModelSdfDatas.emplace_back(
						Constant::ModelSdfData{
							.LocalMatrix = pDF->LocalMatrix,
							.WorldMatrix = pDF->WorldMatrix,
							.CoordMatrix = pDF->CoordMatrix,
							.SdfLower = pDF->SdfBox.m_Lower,
							.SdfUpper = pDF->SdfBox.m_Upper
						}
					);

					auto& rSdfTexture = m_pMeshSdfTextures.emplace_back();
					ReturnIfFalse(pCache->Require(pDF->strSdfTextureName.c_str())->QueryInterface(
						IID_ITexture, 
						PPV_ARG(rSdfTexture.GetAddressOf())
					));
					return true;
				}
			);
			ReturnIfFalse(pDevice->CreateBuffer(
				FBufferDesc::CreateStructured(
					m_ModelSdfDatas.size() * sizeof(Constant::ModelSdfData),
					sizeof(Constant::ModelSdfData),
					true,
					"ModelSdfDataBuffer"
				),
				IID_IBuffer,
				PPV_ARG(m_pModelSdfDataBuffer.GetAddressOf())
			));
			ReturnIfFalse(pCache->Collect(m_pModelSdfDataBuffer.Get()));

			ReturnIfFalse(pCmdList->WriteBuffer(
				m_pModelSdfDataBuffer.Get(), 
				m_ModelSdfDatas.data(), 
				m_ModelSdfDatas.size() * sizeof(Constant::ModelSdfData)
			));

			m_pBindlessSet->Resize(static_cast<UINT32>(m_pMeshSdfTextures.size()), false);
			for (UINT32 ix = 0; ix < m_pMeshSdfTextures.size(); ++ix)
			{
				m_pBindlessSet->SetSlot(FBindingSetItem::CreateTexture_SRV(0, m_pMeshSdfTextures[ix].Get()), ix + 1);
			}
		}


		ReturnIfFalse(pCmdList->SetComputeState(m_ComputeState));
		
		

		return true;
	}

	BOOL FGlobalSdfPass::FinishPass()
	{
		m_ModelSdfDatas.clear();
		m_pMeshSdfTextures.clear();
		return true;
	}

}

