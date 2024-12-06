#include "../include/SdfGenerate.h"
#include "../../../Core/include/ComRoot.h"
#include "../../../Shader/ShaderCompiler.h"
#include "../../../Scene/include/Scene.h"
#include "../../../Gui/include/GuiPanel.h"
#include "../../../Math/include/Bvh.h"
#include <memory>
#include <string>

namespace FTS
{
#define THREAD_GROUP_SIZE_Y 8
#define THREAD_GROUP_SIZE_Z 8
#define BVH_STACK_SIZE 32
#define UPPER_BOUND_ESTIMATE_PRESION 6
#define X_SLICE_SIZE 8

    BOOL FSdfGeneratePass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
    {
		pCache->GetWorld()->GetGlobalEntity()->GetComponent<Event::GenerateSdf>()->AddEvent(
			[this](FEntity* pEntity) 
			{ 
				ContinuePrecompute();
				m_pDistanceField = pEntity->GetComponent<FDistanceField>();

				ReturnIfFalse(m_pDistanceField != nullptr);

				if (!m_pDistanceField->CheckSdfFileExist())
				{
					const auto& crMeshDF = m_pDistanceField->MeshDistanceFields[0];
					std::string strSdfName = *pEntity->GetComponent<std::string>() + ".sdf";
					pBinaryOutput = std::make_unique<Serialization::BinaryOutput>(std::string(PROJ_DIR) + "Asset/SDF/" + strSdfName);
				}
				return true;
			}
		);


        // Binding Layout.
		{
			FBindingLayoutItemArray BindingLayoutItems(4);
            BindingLayoutItems[0] = FBindingLayoutItem::CreatePushConstants(0, sizeof(Constant::SdfGeneratePassConstants));
			BindingLayoutItems[1] = FBindingLayoutItem::CreateStructuredBuffer_SRV(0);
			BindingLayoutItems[2] = FBindingLayoutItem::CreateStructuredBuffer_SRV(1);
            BindingLayoutItems[3] = FBindingLayoutItem::CreateTexture_UAV(0);
			ReturnIfFalse(pDevice->CreateBindingLayout(
                FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems }, 
                IID_IBindingLayout, 
                PPV_ARG(m_pBindingLayout.GetAddressOf())
            ));
		}

        // Shader.
		{
			FShaderCompileDesc CSCompileDesc;
			CSCompileDesc.strShaderName = "SDF/SdfGenerate.hlsl";
			CSCompileDesc.strEntryPoint = "CS";
			CSCompileDesc.Target = EShaderTarget::Compute;
			CSCompileDesc.strDefines.push_back("GROUP_THREAD_NUM_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			CSCompileDesc.strDefines.push_back("GROUP_THREAD_NUM_Z=" + std::to_string(THREAD_GROUP_SIZE_Z));
			CSCompileDesc.strDefines.push_back("UPPER_BOUND_ESTIMATE_PRECISON=" + std::to_string(UPPER_BOUND_ESTIMATE_PRESION));
			CSCompileDesc.strDefines.push_back("BVH_STACK_SIZE=" + std::to_string(BVH_STACK_SIZE));
			FShaderData CSData = ShaderCompile::CompileShader(CSCompileDesc);

			FShaderDesc CSDesc;
			CSDesc.ShaderType = EShaderType::Compute;
			CSDesc.strEntryName = "CS";
			ReturnIfFalse(pDevice->CreateShader(CSDesc, CSData.Data(), CSData.Size(), IID_IShader, PPV_ARG(m_pCS.GetAddressOf())));
		}

        // Pipeline.
		{
			FComputePipelineDesc PipelineDesc;
			PipelineDesc.CS = m_pCS.Get();
			PipelineDesc.pBindingLayouts.PushBack(m_pBindingLayout.Get());
			ReturnIfFalse(pDevice->CreateComputePipeline(PipelineDesc, IID_IComputePipeline, PPV_ARG(m_pPipeline.GetAddressOf())));
		}

		// Compute State.
		{
			m_ComputeState.pBindingSets.Resize(1);
			m_ComputeState.pPipeline = m_pPipeline.Get();
		}

        return true;
    }

    BOOL FSdfGeneratePass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
    {
		ReturnIfFalse(pCmdList->Open());
		
		const auto& crMeshDF = m_pDistanceField->MeshDistanceFields[m_dwCurrMeshSdfIndex];

		if (!m_bResourceWrited)
        {
			IDevice* pDevice = pCmdList->GetDevice();

			// Texture.
			{
				ReturnIfFalse(pDevice->CreateTexture(
					FTextureDesc::CreateReadWrite(
						gdwSdfResolution,
						gdwSdfResolution,
						gdwSdfResolution,
						EFormat::R32_FLOAT,
						crMeshDF.strSdfTextureName
					),
					IID_ITexture,
					PPV_ARG(m_pSdfOutputTexture.GetAddressOf())
				));
				ReturnIfFalse(pCache->Collect(m_pSdfOutputTexture.Get()));
			}

			if (!m_pDistanceField->CheckSdfFileExist())
			{
				// Buffer.
				{
					ReturnIfFalse(pDevice->CreateBuffer(
						FBufferDesc::CreateStructured(
							crMeshDF.Bvh.GetNodes().size() * sizeof(FBvh::Node),
							sizeof(FBvh::Node),
							true
						),
						IID_IBuffer,
						PPV_ARG(m_pBvhNodeBuffer.GetAddressOf())
					));
					ReturnIfFalse(pDevice->CreateBuffer(
						FBufferDesc::CreateStructured(
							crMeshDF.Bvh.GetVertices().size() * sizeof(FBvh::Vertex),
							sizeof(FBvh::Vertex),
							true
						),
						IID_IBuffer,
						PPV_ARG(m_pBvhVertexBuffer.GetAddressOf())
					));
				}

				// Texture.
				{
					ReturnIfFalse(pDevice->CreateStagingTexture(
						FTextureDesc::CreateReadBack(
							gdwSdfResolution,
							gdwSdfResolution,
							gdwSdfResolution,
							EFormat::R32_FLOAT
						),
						ECpuAccessMode::Read,
						IID_IStagingTexture,
						PPV_ARG(m_pReadBackTexture.GetAddressOf())
					));
				}

				// Binding Set.
				{
					FBindingSetItemArray BindingSetItems(4);
					BindingSetItems[0] = FBindingSetItem::CreatePushConstants(0, sizeof(Constant::SdfGeneratePassConstants));
					BindingSetItems[1] = FBindingSetItem::CreateStructuredBuffer_SRV(0, m_pBvhNodeBuffer.Get());
					BindingSetItems[2] = FBindingSetItem::CreateStructuredBuffer_SRV(1, m_pBvhVertexBuffer.Get());
					BindingSetItems[3] = FBindingSetItem::CreateTexture_UAV(0, m_pSdfOutputTexture.Get());
					ReturnIfFalse(pDevice->CreateBindingSet(
						FBindingSetDesc{ .BindingItems = BindingSetItems },
						m_pBindingLayout.Get(),
						IID_IBindingSet,
						PPV_ARG(m_pBindingSet.GetAddressOf())
					));
				}

				// Compute State.
				{
					m_ComputeState.pBindingSets[0] = m_pBindingSet.Get();
				}


				m_PassConstants.dwTriangleNum = crMeshDF.Bvh.dwTriangleNum;
				m_PassConstants.SdfLower = crMeshDF.SdfBox.m_Lower;
				m_PassConstants.SdfUpper = crMeshDF.SdfBox.m_Upper;
				m_PassConstants.SdfExtent = m_PassConstants.SdfUpper - m_PassConstants.SdfLower;

				const auto& crNodes = crMeshDF.Bvh.GetNodes();
				const auto& crVertices = crMeshDF.Bvh.GetVertices();
				ReturnIfFalse(pCmdList->WriteBuffer(m_pBvhNodeBuffer.Get(), crNodes.data(), crNodes.size() * sizeof(FBvh::Node)));
				ReturnIfFalse(pCmdList->WriteBuffer(m_pBvhVertexBuffer.Get(), crVertices.data(), crVertices.size() * sizeof(FBvh::Vertex)));
			}

			m_bResourceWrited = true;
        }

		if (!m_pDistanceField->CheckSdfFileExist())
		{
			ReturnIfFalse(pCmdList->SetComputeState(m_ComputeState));

			m_PassConstants.dwXBegin = m_dwBeginX;
			m_PassConstants.dwXEnd = m_dwBeginX + X_SLICE_SIZE;
			ReturnIfFalse(pCmdList->SetPushConstants(&m_PassConstants, sizeof(Constant::SdfGeneratePassConstants)));
			ReturnIfFalse(pCmdList->Dispatch(
				1,
				static_cast<UINT32>(Align(gdwSdfResolution, static_cast<UINT32>(THREAD_GROUP_SIZE_Y)) / THREAD_GROUP_SIZE_Y),
				static_cast<UINT32>(Align(gdwSdfResolution, static_cast<UINT32>(THREAD_GROUP_SIZE_Z)) / THREAD_GROUP_SIZE_Z)
			));

			m_dwBeginX += X_SLICE_SIZE;
			if (m_dwBeginX == gdwSdfResolution)
			{
				ReturnIfFalse(pCmdList->CopyTexture(
					m_pReadBackTexture.Get(), 
					FTextureSlice{}, 
					m_pSdfOutputTexture.Get(), 
					FTextureSlice{}
				));

				if (m_dwCurrMeshSdfIndex + 1 == static_cast<UINT32>(m_pDistanceField->MeshDistanceFields.size()))
				{
					ReturnIfFalse(pCache->GetWorld()->GetGlobalEntity()->GetComponent<Event::UpdateGlobalSdf>()->Broadcast());
				}
			}
		}
		else
		{
			UINT32 dwPixelSize = GetFormatInfo(EFormat::R32_FLOAT).btBytesPerBlock;
			ReturnIfFalse(pCmdList->WriteTexture(
				m_pSdfOutputTexture.Get(),
				0,
				0,
				crMeshDF.SdfData.data(),
				gdwSdfResolution * dwPixelSize,
				gdwSdfResolution * gdwSdfResolution* dwPixelSize
			));

			if (m_dwCurrMeshSdfIndex + 1 == static_cast<UINT32>(m_pDistanceField->MeshDistanceFields.size()))
			{
				ReturnIfFalse(pCache->GetWorld()->GetGlobalEntity()->GetComponent<Event::UpdateGlobalSdf>()->Broadcast());
			}
		}

		ReturnIfFalse(pCmdList->Close());
		return true;
    }

	BOOL FSdfGeneratePass::FinishPass()
	{
		auto& rMeshDF = m_pDistanceField->MeshDistanceFields[m_dwCurrMeshSdfIndex];

		if (!m_pDistanceField->CheckSdfFileExist())
		{
			if (m_dwBeginX < gdwSdfResolution)
			{
				ContinuePrecompute();
				return true;
			}
			m_dwBeginX = 0;

			std::vector<FLOAT> SdfData(gdwSdfResolution * gdwSdfResolution * gdwSdfResolution);
			HANDLE FenceEvent = CreateEvent(nullptr, false, false, nullptr);

			UINT64 stRowPitch = 0;
			UINT64 stRowSize = sizeof(FLOAT) * gdwSdfResolution;
			UINT8* pMappedData = static_cast<UINT8*>(m_pReadBackTexture->Map(FTextureSlice{}, ECpuAccessMode::Read, FenceEvent, &stRowPitch));
			ReturnIfFalse(pMappedData && stRowPitch == stRowSize);

			UINT8* Dst = reinterpret_cast<UINT8*>(SdfData.data());
			for (UINT32 z = 0; z < gdwSdfResolution; ++z)
			{
				for (UINT32 y = 0; y < gdwSdfResolution; ++y)
				{
					UINT8* Src = pMappedData + stRowPitch * y;
					memcpy(Dst, Src, stRowSize);
					Dst += stRowSize;
				}
				pMappedData += stRowPitch * gdwSdfResolution;
			}

			if (m_dwCurrMeshSdfIndex == 0) (*pBinaryOutput)(gdwSdfResolution);

			(*pBinaryOutput)(
				rMeshDF.SdfBox.m_Lower.x,
				rMeshDF.SdfBox.m_Lower.y,
				rMeshDF.SdfBox.m_Lower.z,
				rMeshDF.SdfBox.m_Upper.x,
				rMeshDF.SdfBox.m_Upper.y,
				rMeshDF.SdfBox.m_Upper.z
			);
			pBinaryOutput->SaveBinaryData(SdfData.data(), SdfData.size() * sizeof(FLOAT));

			m_pReadBackTexture.Reset();
			m_pBvhNodeBuffer.Reset();
			m_pBvhVertexBuffer.Reset();
			rMeshDF.Bvh.Clear();

			if (++m_dwCurrMeshSdfIndex == static_cast<UINT32>(m_pDistanceField->MeshDistanceFields.size()))
			{
				std::string strSdfName = rMeshDF.strSdfTextureName.substr(0, rMeshDF.strSdfTextureName.find("SdfTexture")) + ".sdf";
				Gui::NotifyMessage(Gui::ENotifyType::Info, strSdfName + " bake finished.");
				m_pDistanceField = nullptr;
				pBinaryOutput.reset();
				m_dwCurrMeshSdfIndex = 0;
			}
			else
			{
				ContinuePrecompute();
			}
		}
		else
		{
			if (++m_dwCurrMeshSdfIndex == static_cast<UINT32>(m_pDistanceField->MeshDistanceFields.size()))
			{
				std::string strSdfName = rMeshDF.strSdfTextureName.substr(0, rMeshDF.strSdfTextureName.find("SdfTexture")) + ".sdf";
				Gui::NotifyMessage(Gui::ENotifyType::Info, strSdfName + " bake finished.");

				for (auto& rDF : m_pDistanceField->MeshDistanceFields) rDF.SdfData.clear();
				m_pDistanceField = nullptr;
				m_dwCurrMeshSdfIndex = 0;
			}
			else
			{
				ContinuePrecompute();
			}
		}

		m_pSdfOutputTexture.Reset();
		m_bResourceWrited = false;
		return true;
	}

}



