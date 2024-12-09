#include "../include/GlobalSdf.h"
#include "../../../Shader/ShaderCompiler.h"

namespace FTS
{
#define THREAD_GROUP_SIZE_X 8
#define THREAD_GROUP_SIZE_Y 8
#define THREAD_GROUP_SIZE_Z 8

	BOOL FGlobalSdfPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
	{
		pCache->GetWorld()->GetGlobalEntity()->GetComponent<Event::UpdateGlobalSdf>()->AddEvent(
			[this]() 
			{ 
				ContinuePrecompute();
				return true;
			}
		);

		ReturnIfFalse(PipelineSetup(pDevice));
		ReturnIfFalse(ComputeStateSetup(pDevice, pCache));
		return true;
	}

	BOOL FGlobalSdfPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		ReturnIfFalse(pCmdList->Open());

		// Texture.
		{
			BOOL bUpdateModelSdfTestureRes = (pCache->GetWorld()->Each<FSceneGrid>(
				[&](FEntity* pEntity, FSceneGrid* pGrid) -> BOOL
				{
					for (const auto& crChunk : pGrid->Chunks)
					{
						if (crChunk.bModelMoved)
						{
							UINT32 dwCounter = 0;
							for (const auto* cpModel : crChunk.pModelEntities)
							{
								FDistanceField* pDF = cpModel->GetComponent<FDistanceField>();
								for (const auto& crMeshDF : pDF->MeshDistanceFields)
								{
									const FDistanceField::MeshDistanceField* pMeshSdf = &crMeshDF;
									auto Iter = std::find(m_cpMeshSdfs.begin(), m_cpMeshSdfs.end(), pMeshSdf);
									if (Iter == m_cpMeshSdfs.end())
									{
										m_cpMeshSdfs.emplace_back(&crMeshDF);
									}
								}
							}
						}
					}
					return true;
				}
			));
			ReturnIfFalse(bUpdateModelSdfTestureRes);
		}

		// Binding Set.
		{
			m_pBindlessSet->Resize(static_cast<UINT32>(m_cpMeshSdfs.size()), false);
			for (UINT32 ix = 0; ix < m_cpMeshSdfs.size(); ++ix)
			{
				ITexture* pSdfTexture;
				ReturnIfFalse(pCache->Require(m_cpMeshSdfs[ix]->strSdfTextureName.c_str())->QueryInterface(
					IID_ITexture,
					PPV_ARG(&pSdfTexture)
				));
				m_pBindlessSet->SetSlot(FBindingSetItem::CreateTexture_SRV(ix, pSdfTexture));
			}
		}

		// Buffer
		{
			BOOL bUpdateModelSdfDataRes = (pCache->GetWorld()->Each<FSceneGrid>(
				[&](FEntity* pEntity, FSceneGrid* pGrid) -> BOOL
				{
					for (const auto& crChunk : pGrid->Chunks)
					{
						if (crChunk.bModelMoved)
						{
							UINT32 dwCounter = 0;
							for (const auto* cpModel : crChunk.pModelEntities)
							{
								FDistanceField* pDF = cpModel->GetComponent<FDistanceField>();
								FTransform* pTrans = cpModel->GetComponent<FTransform>();

								for (const auto& crMeshDF : pDF->MeshDistanceFields)
								{
									const FDistanceField::MeshDistanceField* pMeshSdf = &crMeshDF;
									auto Iter = std::find(m_cpMeshSdfs.begin(), m_cpMeshSdfs.end(), pMeshSdf);
									ReturnIfFalse(Iter != m_cpMeshSdfs.end());

									FDistanceField::TransformData Data = crMeshDF.GetTransformed(pTrans);
									m_ModelSdfDatas.emplace_back(
										Constant::ModelSdfData{
											.CoordMatrix = Data.CoordMatrix,
											.SdfLower = Data.SdfBox.m_Lower,
											.SdfUpper = Data.SdfBox.m_Upper,
											.dwMeshSdfIndex = static_cast<UINT32>(std::distance(m_cpMeshSdfs.begin(), Iter))
										}
									);
								}
							}
						}
					}
					return true;
				}
			));
			ReturnIfFalse(bUpdateModelSdfDataRes);

			IDevice* pDevice = pCmdList->GetDevice();

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

				m_pDynamicBindingSet.Reset();

				FBindingSetItemArray DynamicBindingSetItems(1);
				DynamicBindingSetItems[0] = FBindingSetItem::CreateStructuredBuffer_SRV(0, m_pModelSdfDataBuffer.Get());
				ReturnIfFalse(pDevice->CreateBindingSet(
					FBindingSetDesc{ .BindingItems = DynamicBindingSetItems },
					m_pDynamicBindingLayout.Get(),
					IID_IBindingSet,
					PPV_ARG(m_pDynamicBindingSet.GetAddressOf())
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


		UINT32 dwChunkNumPerAxis = gdwGlobalSdfResolution / gdwVoxelNumPerChunk;
		FLOAT fVoxelSize = gfSceneGridSize / gdwGlobalSdfResolution;
		FLOAT fOffset = -gfSceneGridSize * 0.5f + fVoxelSize * 0.5f;

		// Clear Pass.
		BOOL bResOfGLobalSdfPassExecute = pCache->GetWorld()->Each<FSceneGrid>(
			[&](FEntity* pEntity, FSceneGrid* pGrid) -> BOOL
			{
				for (UINT32 ix = 0; ix < pGrid->Chunks.size(); ++ix)
				{
					auto& rChunk = pGrid->Chunks[ix];
					if (rChunk.bModelMoved && rChunk.pModelEntities.empty())
					{
						auto& rPassConstants = m_PassConstants.emplace_back();
						rPassConstants.VoxelOffset = {
							ix % dwChunkNumPerAxis,
							(ix / dwChunkNumPerAxis) % dwChunkNumPerAxis,
							ix / (dwChunkNumPerAxis * dwChunkNumPerAxis)
						};
						rPassConstants.VoxelOffset = (rPassConstants.VoxelOffset) * gdwVoxelNumPerChunk;

						ReturnIfFalse(pCmdList->SetComputeState(m_ClearPassComputeState));
						ReturnIfFalse(pCmdList->SetPushConstants(&rPassConstants, sizeof(Constant::GlobalSdfConstants)));

						FVector3I ThreadGroupNum = {
							Align(gdwVoxelNumPerChunk, static_cast<UINT32>(THREAD_GROUP_SIZE_X)) / THREAD_GROUP_SIZE_X,
							Align(gdwVoxelNumPerChunk, static_cast<UINT32>(THREAD_GROUP_SIZE_Y)) / THREAD_GROUP_SIZE_Y,
							Align(gdwVoxelNumPerChunk, static_cast<UINT32>(THREAD_GROUP_SIZE_Z)) / THREAD_GROUP_SIZE_Z
						};

						ReturnIfFalse(pCmdList->Dispatch(ThreadGroupNum.x, ThreadGroupNum.y, ThreadGroupNum.z));
						rChunk.bModelMoved = false;
					}
				}
				return true;
			}
		);
		ReturnIfFalse(bResOfGLobalSdfPassExecute);

		UINT32 dwMeshIndex = 0;
		bResOfGLobalSdfPassExecute = pCache->GetWorld()->Each<FSceneGrid>(
			[&](FEntity* pEntity, FSceneGrid* pGrid) -> BOOL
			{
				for (UINT32 ix = 0; ix < pGrid->Chunks.size(); ++ix)
				{
					auto& rChunk = pGrid->Chunks[ix];
					if (rChunk.bModelMoved)
					{
						UINT32 dwMeshIndexBegin = dwMeshIndex;
						for (const auto* cpModel : rChunk.pModelEntities)
						{
							dwMeshIndex += static_cast<UINT32>(cpModel->GetComponent<FDistanceField>()->MeshDistanceFields.size());
						}

						auto& rPassConstants = m_PassConstants.emplace_back();
						rPassConstants.fGIMaxDistance = gfSceneGridSize;
						rPassConstants.dwMeshSdfBegin = dwMeshIndexBegin;
						rPassConstants.dwMeshSdfEnd = dwMeshIndex;

						rPassConstants.VoxelOffset = {
							ix % dwChunkNumPerAxis,
							(ix / dwChunkNumPerAxis) % dwChunkNumPerAxis,
							ix / (dwChunkNumPerAxis * dwChunkNumPerAxis)
						};
						rPassConstants.VoxelOffset = (rPassConstants.VoxelOffset) * gdwVoxelNumPerChunk;

						rPassConstants.VoxelWorldMatrix = {
							fVoxelSize, 0.0f,		0.0f,		0.0f,
							0.0f,		fVoxelSize, 0.0f,		0.0f,
							0.0f,		0.0f,		fVoxelSize, 0.0f,
							fOffset,	fOffset,	fOffset,	1.0f
						};

						ReturnIfFalse(pCmdList->SetComputeState(m_ComputeState));
						ReturnIfFalse(pCmdList->SetPushConstants(&rPassConstants, sizeof(Constant::GlobalSdfConstants)));

						FVector3I ThreadGroupNum = {
							Align(gdwVoxelNumPerChunk, static_cast<UINT32>(THREAD_GROUP_SIZE_X)) / THREAD_GROUP_SIZE_X,
							Align(gdwVoxelNumPerChunk, static_cast<UINT32>(THREAD_GROUP_SIZE_Y)) / THREAD_GROUP_SIZE_Y,
							Align(gdwVoxelNumPerChunk, static_cast<UINT32>(THREAD_GROUP_SIZE_Z)) / THREAD_GROUP_SIZE_Z
						};

						ReturnIfFalse(pCmdList->Dispatch(ThreadGroupNum.x, ThreadGroupNum.y, ThreadGroupNum.z));

						rChunk.bModelMoved = false;
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
		m_cpMeshSdfs.clear(); m_cpMeshSdfs.shrink_to_fit();
		m_ModelSdfDatas.clear(); m_ModelSdfDatas.shrink_to_fit();
		m_PassConstants.clear(); m_PassConstants.shrink_to_fit();
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

