#include "../include/GlobalSdf.h"
#include "../../../Shader/ShaderCompiler.h"
#include "../../../Gui/include/GuiPanel.h"
#include "../../../Scene/include/Scene.h"

namespace FTS
{
#define THREAD_GROUP_SIZE_X 8
#define THREAD_GROUP_SIZE_Y 8
#define THREAD_GROUP_SIZE_Z 8

	BOOL FGlobalSdfPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
	{
		// Binding Layout.
		{
			FBindingLayoutItemArray BindingLayoutItems(3);
			BindingLayoutItems[0] = FBindingLayoutItem::CreatePushConstants(0, sizeof(Constant::GlobalSdfConstants));
			BindingLayoutItems[1] = FBindingLayoutItem::CreateSampler(0);
			BindingLayoutItems[2] = FBindingLayoutItem::CreateTexture_UAV(0);
			ReturnIfFalse(pDevice->CreateBindingLayout(
				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems },
				IID_IBindingLayout,
				PPV_ARG(m_pBindingLayout.GetAddressOf())
			));

			FBindingLayoutItemArray DynamicBindingLayoutItems(1);
			DynamicBindingLayoutItems[0] = FBindingLayoutItem::CreateStructuredBuffer_SRV(0);
			ReturnIfFalse(pDevice->CreateBindingLayout(
				FBindingLayoutDesc{ .BindingLayoutItems = DynamicBindingLayoutItems },
				IID_IBindingLayout,
				PPV_ARG(m_pDynamicBindingLayout.GetAddressOf())
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
			CSCompileDesc.strDefines.push_back("THREAD_GROUP_SIZE_Z=" + std::to_string(THREAD_GROUP_SIZE_Z));
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
			PipelineDesc.pBindingLayouts.PushBack(m_pDynamicBindingLayout.Get());
			PipelineDesc.pBindingLayouts.PushBack(m_pBindlessLayout.Get());
			ReturnIfFalse(pDevice->CreateComputePipeline(PipelineDesc, IID_IComputePipeline, PPV_ARG(m_pPipeline.GetAddressOf())));
		}


		// Texture.
		{
			ReturnIfFalse(pDevice->CreateTexture(
				FTextureDesc::CreateReadWrite(
					gdwGlobalSdfResolution,
					gdwGlobalSdfResolution,
					gdwGlobalSdfResolution,
					EFormat::R32_FLOAT, 
					"GlobalSdfTexture"
				),
				IID_ITexture,
				PPV_ARG(m_pGlobalSdfTexture.GetAddressOf())
			));
			ReturnIfFalse(pCache->Collect(m_pGlobalSdfTexture.Get()));
		}

		// Binding Set.
		{
			ISampler* pLinearClampSampler;
			ReturnIfFalse(pCache->Require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));
 
			FBindingSetItemArray BindingSetItems(3);
			BindingSetItems[0] = FBindingSetItem::CreatePushConstants(0, sizeof(Constant::GlobalSdfConstants));
			BindingSetItems[1] = FBindingSetItem::CreateSampler(0, pLinearClampSampler);
			BindingSetItems[2] = FBindingSetItem::CreateTexture_UAV(0, m_pGlobalSdfTexture.Get());
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

		m_GlobalBox = FBounds3F(- 1.0f * gdwMaxGIDistance / 2.0f, 1.0f * gdwMaxGIDistance / 2.0f);
		ReturnIfFalse(pCache->CollectConstants("GlobalBox", &m_GlobalBox));

		ReturnIfFalse(pCache->GetWorld()->Each<Event::UpdateSceneGrid>(
			[this](FEntity* pEntity, Event::UpdateSceneGrid* pEvent) -> BOOL
			{
				pEvent->AddEvent(
					[this]() -> BOOL 
					{
						this->Type &= ~ERenderPassType::Exclude; 
						return true;
					}
				);
				return true;
			}
		));

		return true;
	}

	BOOL FGlobalSdfPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		ReturnIfFalse(pCmdList->Open());

		IDevice* pDevice = pCmdList->GetDevice();
		// Update.
		{
			ReturnIfFalse((pCache->GetWorld()->Each<FMesh, FDistanceField, FTransform>(
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
			)));
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

			FBindingSetItemArray BindingSetItems(1);
			BindingSetItems[0] = FBindingSetItem::CreateStructuredBuffer_SRV(0, m_pModelSdfDataBuffer.Get());
			ReturnIfFalse(pDevice->CreateBindingSet(
				FBindingSetDesc{ .BindingItems = BindingSetItems },
				m_pDynamicBindingLayout.Get(),
				IID_IBindingSet,
				PPV_ARG(m_pDynamicBindingSet.GetAddressOf())
			));

			m_pBindlessSet->Resize(static_cast<UINT32>(m_pMeshSdfTextures.size()), false);
			for (UINT32 ix = 0; ix < m_pMeshSdfTextures.size(); ++ix)
			{
				m_pBindlessSet->SetSlot(FBindingSetItem::CreateTexture_SRV(0, m_pMeshSdfTextures[ix].Get()), ix + 1);
			}

			m_ComputeState.pBindingSets.PushBack(m_pDynamicBindingSet.Get());
			m_ComputeState.pBindingSets.PushBack(m_pBindlessSet.Get());
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

