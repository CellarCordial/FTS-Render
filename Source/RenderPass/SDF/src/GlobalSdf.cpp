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
		ReturnIfFalse(pCache->GetWorld()->Each<Event::UpdateSceneGrid>(
			[this](FEntity* pEntity, Event::UpdateSceneGrid* pEvent) -> BOOL
			{
				pEvent->AddEvent(
					[this]()
					{
						Type &= ~ERenderPassType::Exclude;
						return true;
					}
				);
				return true;
			}
		));

		ReturnIfFalse(PipelineSetup(pDevice));
		ReturnIfFalse(ComputeStateSetup(pDevice, pCache));
		return true;
	}

	BOOL FGlobalSdfPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		ReturnIfFalse(pCmdList->Open());

		if (!m_bGlobalSdfInited)
		{
			ReturnIfFalse(pCmdList->SetComputeState(m_ClearPassComputeState));

			BOOL bResOfGLobalSdfPassExecute = pCache->GetWorld()->Each<FSceneGrid>(
				[&](FEntity* pEntity, FSceneGrid* pGrid) -> BOOL
				{
					for (UINT32 ix = 0; ix < pGrid->Chunks.size(); ++ix)
					{
						auto& rPassConstants = m_PassConstants.emplace_back();
						rPassConstants.fGIMaxDistance = gfSceneGridSize;

						UINT32 dwChunkNumPerAxis = gdwGlobalSdfResolution / gdwVoxelNumPerChunk;
						rPassConstants.VoxelOffset = {
							ix % dwChunkNumPerAxis,
							(ix / dwChunkNumPerAxis) % dwChunkNumPerAxis,
							ix / (dwChunkNumPerAxis * dwChunkNumPerAxis)
						};
						rPassConstants.VoxelOffset *= gdwVoxelNumPerChunk;
						ReturnIfFalse(pCmdList->SetPushConstants(&rPassConstants, sizeof(Constant::GlobalSdfConstants)));

						FVector3I ThreadGroupNum = {
							Align(gdwVoxelNumPerChunk, static_cast<UINT32>(THREAD_GROUP_SIZE_X)) / THREAD_GROUP_SIZE_X,
							Align(gdwVoxelNumPerChunk, static_cast<UINT32>(THREAD_GROUP_SIZE_Y)) / THREAD_GROUP_SIZE_Y,
							Align(gdwVoxelNumPerChunk, static_cast<UINT32>(THREAD_GROUP_SIZE_Z)) / THREAD_GROUP_SIZE_Z
						};
						ReturnIfFalse(pCmdList->Dispatch(ThreadGroupNum.x, ThreadGroupNum.y, ThreadGroupNum.z));
					}
					return true;
				}
			);

			ReturnIfFalse(bResOfGLobalSdfPassExecute);
			ReturnIfFalse(pCmdList->Close());
			m_bGlobalSdfInited = true;
			return true;
		}

		BOOL bUpdateModelSdfDataRes = (pCache->GetWorld()->Each<FSceneGrid>(
			[&](FEntity* pEntity, FSceneGrid* pGrid) -> BOOL
			{
				for (const auto& crChunk : pGrid->Chunks)
				{
					if (crChunk.bModelMoved)
					{
						for (const auto* cpModel : crChunk.pModelEntities)
						{
							FDistanceField* pDF = cpModel->GetComponent<FDistanceField>();
							m_ModelSdfDatas.emplace_back(
								Constant::ModelSdfData{
									.LocalMatrix = pDF->LocalMatrix,
									.WorldMatrix = pDF->WorldMatrix,
									.CoordMatrix = pDF->CoordMatrix,
									.SdfLower = pDF->SdfBox.m_Lower,
									.SdfUpper = pDF->SdfBox.m_Upper
								}
							);
						}
					}
				}
				return true;
			}
		));
		ReturnIfFalse(bUpdateModelSdfDataRes);

		IDevice* pDevice = pCmdList->GetDevice();

		// Buffer
		{
			if (m_ModelSdfDatas.size() > m_dwModelSdfDataDefaultCount)
			{
				m_pModelSdfDataBuffer.Reset();
				m_dwModelSdfDataDefaultCount = static_cast<UINT32>(m_ModelSdfDatas.size());

				ReturnIfFalse(pDevice->CreateBuffer(
					FBufferDesc::CreateStructured(
						m_ModelSdfDatas.size() * sizeof(Constant::ModelSdfData),
						sizeof(Constant::ModelSdfData),
						true
					),
					IID_IBuffer,
					PPV_ARG(m_pModelSdfDataBuffer.GetAddressOf())
				));

				// Compute State.
				{
					m_ComputeState.pBindingSets[1] = m_pDynamicBindingSet.Get();
				}
			}

			ReturnIfFalse(!m_ModelSdfDatas.empty());
			ReturnIfFalse(pCmdList->WriteBuffer(
				m_pModelSdfDataBuffer.Get(),
				m_ModelSdfDatas.data(),
				m_ModelSdfDatas.size() * sizeof(Constant::ModelSdfData)
			));
		}


		UINT32 dwModelIndex = 0;
		BOOL bResOfGLobalSdfPassExecute = pCache->GetWorld()->Each<FSceneGrid>(
			[&](FEntity* pEntity, FSceneGrid* pGrid) -> BOOL
			{
				for (UINT32 ix = 0; ix < pGrid->Chunks.size(); ++ix)
				{
					auto& rChunk = pGrid->Chunks[ix];
					if (rChunk.bModelMoved)
					{
						// Buffer & Texture.
						{
							for (const auto* cpEntity : rChunk.pModelEntities)
							{
								FDistanceField* pDF = cpEntity->GetComponent<FDistanceField>();

								auto& rSdfTexture = m_pMeshSdfTextures.emplace_back();
								ReturnIfFalse(pCache->Require(pDF->strSdfTextureName.c_str())->QueryInterface(
									IID_ITexture,
									PPV_ARG(&rSdfTexture)
								));
							}
						}

						if (!m_pMeshSdfTextures.empty())
						{
							// Binding Set.
							{
								m_pBindlessSet->Resize(static_cast<UINT32>(m_pMeshSdfTextures.size()), false);
								for (UINT32 ix = 0; ix < m_pMeshSdfTextures.size(); ++ix)
								{
									m_pBindlessSet->SetSlot(FBindingSetItem::CreateTexture_SRV(0, m_pMeshSdfTextures[ix]), ix + 1);
								}
							}

							ReturnIfFalse(pCmdList->SetComputeState(m_ComputeState));
						}
						else
						{
							ReturnIfFalse(pCmdList->SetComputeState(m_ClearPassComputeState));
						}

						auto& rPassConstants = m_PassConstants.emplace_back();
						rPassConstants.dwModelSdfBegin = dwModelIndex;
						rPassConstants.dwModelSdfEnd = dwModelIndex + rChunk.pModelEntities.size();
						rPassConstants.fGIMaxDistance = gfSceneGridSize;

						UINT32 dwChunkNumPerAxis = gdwGlobalSdfResolution / gdwVoxelNumPerChunk;
						rPassConstants.VoxelOffset = {
							ix % dwChunkNumPerAxis,
							(ix / dwChunkNumPerAxis) % dwChunkNumPerAxis,
							ix / (dwChunkNumPerAxis * dwChunkNumPerAxis)
						};
						rPassConstants.VoxelOffset *= gdwVoxelNumPerChunk;

						FLOAT fVoxelSize = gfSceneGridSize / gdwGlobalSdfResolution;
						FLOAT fOffset = -gfSceneGridSize * 0.5f + fVoxelSize * 0.5f;
						rPassConstants.VoxelWorldMatrix = {
							fVoxelSize, 0.0f,		0.0f,		0.0f,
							0.0f,		fVoxelSize, 0.0f,		0.0f,
							0.0f,		0.0f,		fVoxelSize, 0.0f,
							fOffset,	fOffset,	fOffset,	1.0f
						};
						ReturnIfFalse(pCmdList->SetPushConstants(&rPassConstants, sizeof(Constant::GlobalSdfConstants)));

						FVector3I ThreadGroupNum = {
							Align(gdwVoxelNumPerChunk, static_cast<UINT32>(THREAD_GROUP_SIZE_X)) / THREAD_GROUP_SIZE_X,
							Align(gdwVoxelNumPerChunk, static_cast<UINT32>(THREAD_GROUP_SIZE_Y)) / THREAD_GROUP_SIZE_Y,
							Align(gdwVoxelNumPerChunk, static_cast<UINT32>(THREAD_GROUP_SIZE_Z)) / THREAD_GROUP_SIZE_Z
						};

						ReturnIfFalse(pCmdList->Dispatch(ThreadGroupNum.x, ThreadGroupNum.y, ThreadGroupNum.z));

						m_pMeshSdfTextures.clear();
						rChunk.bModelMoved = false;
						dwModelIndex += rChunk.pModelEntities.size();
					}
				}
				return true;
			}
		);

		ReturnIfFalse(bResOfGLobalSdfPassExecute);
		ReturnIfFalse(pCmdList->Close());

		return true;
	}

	BOOL FGlobalSdfPass::FinishPass()
	{
		m_ModelSdfDatas.clear();
		m_PassConstants.clear();
		return true;
	}


	BOOL FGlobalSdfPass::PipelineSetup(IDevice* pDevice)
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

			FBindingLayoutItemArray ClearBindingLayoutItems(2);
			ClearBindingLayoutItems[0] = FBindingLayoutItem::CreatePushConstants(0, sizeof(Constant::GlobalSdfConstants));
			ClearBindingLayoutItems[1] = FBindingLayoutItem::CreateTexture_UAV(0);
			ReturnIfFalse(pDevice->CreateBindingLayout(
				FBindingLayoutDesc{ .BindingLayoutItems = ClearBindingLayoutItems },
				IID_IBindingLayout,
				PPV_ARG(m_pClearPassBindingLayout.GetAddressOf())
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


			CSCompileDesc.strShaderName = "SDF/SdfClear.hlsl";
			FShaderData ClearPassCSData = ShaderCompile::CompileShader(CSCompileDesc);
			ReturnIfFalse(pDevice->CreateShader(CSDesc, ClearPassCSData.Data(), ClearPassCSData.Size(), IID_IShader, PPV_ARG(m_pClearPassCS.GetAddressOf())));
		}

		// Pipeline.
		{
			FComputePipelineDesc PipelineDesc;
			PipelineDesc.CS = m_pCS.Get();
			PipelineDesc.pBindingLayouts.PushBack(m_pBindingLayout.Get());
			PipelineDesc.pBindingLayouts.PushBack(m_pDynamicBindingLayout.Get());
			PipelineDesc.pBindingLayouts.PushBack(m_pBindlessLayout.Get());
			ReturnIfFalse(pDevice->CreateComputePipeline(PipelineDesc, IID_IComputePipeline, PPV_ARG(m_pPipeline.GetAddressOf())));

			FComputePipelineDesc ClearPassPipelineDesc;
			ClearPassPipelineDesc.CS = m_pClearPassCS.Get();
			ClearPassPipelineDesc.pBindingLayouts.PushBack(m_pClearPassBindingLayout.Get());
			ReturnIfFalse(pDevice->CreateComputePipeline(ClearPassPipelineDesc, IID_IComputePipeline, PPV_ARG(m_pClearPassPipeline.GetAddressOf())));
		}

		return true;
	}

	BOOL FGlobalSdfPass::ComputeStateSetup(IDevice* pDevice, IRenderResourceCache* pCache)
	{
		// Buffer.
		{
			ReturnIfFalse(pDevice->CreateBuffer(
				FBufferDesc::CreateStructured(
					m_dwModelSdfDataDefaultCount * sizeof(Constant::ModelSdfData),
					sizeof(Constant::ModelSdfData),
					true
				),
				IID_IBuffer,
				PPV_ARG(m_pModelSdfDataBuffer.GetAddressOf())
			));
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

			FBindingSetItemArray DynamicBindingSetItems(1);
			DynamicBindingSetItems[0] = FBindingSetItem::CreateStructuredBuffer_SRV(0, m_pModelSdfDataBuffer.Get());
			ReturnIfFalse(pDevice->CreateBindingSet(
				FBindingSetDesc{ .BindingItems = DynamicBindingSetItems },
				m_pDynamicBindingLayout.Get(),
				IID_IBindingSet,
				PPV_ARG(m_pDynamicBindingSet.GetAddressOf())
			));

			ReturnIfFalse(pDevice->CreateBindlessSet(m_pBindlessLayout.Get(), IID_IBindlessSet, PPV_ARG(m_pBindlessSet.GetAddressOf())));


			FBindingSetItemArray ClearPassBindingSetItems(2);
			ClearPassBindingSetItems[0] = FBindingSetItem::CreatePushConstants(0, sizeof(Constant::GlobalSdfConstants));
			ClearPassBindingSetItems[1] = FBindingSetItem::CreateTexture_UAV(0, m_pGlobalSdfTexture.Get());
			ReturnIfFalse(pDevice->CreateBindingSet(
				FBindingSetDesc{ .BindingItems = ClearPassBindingSetItems },
				m_pClearPassBindingLayout.Get(),
				IID_IBindingSet,
				PPV_ARG(m_pClearPassBindingSet.GetAddressOf())
			));
		}

		// Compute State.
		{
			m_ComputeState.pBindingSets.PushBack(m_pBindingSet.Get());
			m_ComputeState.pBindingSets.PushBack(m_pDynamicBindingSet.Get());
			m_ComputeState.pBindingSets.PushBack(m_pBindlessSet.Get());
			m_ComputeState.pPipeline = m_pPipeline.Get();

			m_ClearPassComputeState.pBindingSets.PushBack(m_pClearPassBindingSet.Get());
			m_ClearPassComputeState.pPipeline = m_pClearPassPipeline.Get();
		}

		return true;
	}

}

