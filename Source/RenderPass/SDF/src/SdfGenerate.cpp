#include "../include/SdfGenerate.h"
#include "../../../Core/include/ComRoot.h"
#include "../../../Shader/ShaderCompiler.h"
#include "../../../Gui/include/GuiPass.h"
#include <fstream>
#include <memory>
#include <string>

namespace FTS
{
#define THREAD_GROUP_SIZE_Y 8
#define THREAD_GROUP_SIZE_Z 8
#define SDF_RESOLUTION 128
#define BVH_STACK_SIZE 32
#define UPPER_BOUND_ESTIMATE_PRESION 6
#define X_SLICE_SIZE 32

    BOOL FSdfGeneratePass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
    {
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

        return true;
    }

    BOOL FSdfGeneratePass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
    {
        if (!m_bResourceWrited)
        {
			IDevice* pDevice;
			ReturnIfFalse(pCmdList->GetDevice(&pDevice));
			// Buffer.
			{
				ReturnIfFalse(BuildBvh());
				ReturnIfFalse(pDevice->CreateBuffer(
					FBufferDesc::CreateStructured(
						m_Bvh.GetNodes().size() * sizeof(FBvh::Node),
						sizeof(FBvh::Node)
					),
					IID_IBuffer,
					PPV_ARG(m_pBvhNodeBuffer.GetAddressOf())
				));
				ReturnIfFalse(pDevice->CreateBuffer(
					FBufferDesc::CreateStructured(
						m_Bvh.GetVertices().size() * sizeof(FBvh::Vertex),
						sizeof(FBvh::Vertex)
					),
					IID_IBuffer,
					PPV_ARG(m_pBvhVertexBuffer.GetAddressOf())
				));
			}

			// Texture.
			{
				ReturnIfFalse(pDevice->CreateTexture(
					FTextureDesc::CreateReadWrite(
						SDF_RESOLUTION,
						SDF_RESOLUTION,
						SDF_RESOLUTION,
						EFormat::R32_FLOAT
					),
					IID_ITexture,
					PPV_ARG(m_pSdfOutputTexture.GetAddressOf())
				));
				ReturnIfFalse(pDevice->CreateStagingTexture(
					FTextureDesc::CreateReadBack(
						SDF_RESOLUTION,
						SDF_RESOLUTION,
						SDF_RESOLUTION,
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
				m_ComputeState.pBindingSets.PushBack(m_pBindingSet.Get());
				m_ComputeState.pPipeline = m_pPipeline.Get();
			}

			UINT64 stTriangleCount = 0;
			for (const auto& crSubmesh : m_cpMesh->SubMeshes) stTriangleCount += crSubmesh.Indices.size() / 3;
			m_PassConstants.dwTriangleNum = static_cast<UINT32>(stTriangleCount);
			m_PassConstants.SdfExtent = m_PassConstants.SdfUpper - m_PassConstants.SdfLower;

			m_bResourceWrited = true;
        }

		ReturnIfFalse(pCmdList->Open());

		const auto& crNodes = m_Bvh.GetNodes();
		const auto& crVertices = m_Bvh.GetVertices();
		ReturnIfFalse(pCmdList->WriteBuffer(m_pBvhNodeBuffer.Get(), crNodes.data(), crNodes.size()));
		ReturnIfFalse(pCmdList->WriteBuffer(m_pBvhVertexBuffer.Get(), crVertices.data(), crVertices.size()));

		ReturnIfFalse(pCmdList->SetComputeState(m_ComputeState));

		for (UINT32 ix = 0; ix < SDF_RESOLUTION / X_SLICE_SIZE; ++ix)
		{
			m_PassConstants.dwXBegin = X_SLICE_SIZE * ix;
			m_PassConstants.dwXEnd = m_PassConstants.dwXBegin + X_SLICE_SIZE;
			ReturnIfFalse(pCmdList->SetPushConstants(&m_PassConstants, sizeof(Constant::SdfGeneratePassConstants)));
			ReturnIfFalse(pCmdList->Dispatch(
				1,
				static_cast<UINT32>(Align(SDF_RESOLUTION, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y),
				static_cast<UINT32>(Align(SDF_RESOLUTION, THREAD_GROUP_SIZE_Z) / THREAD_GROUP_SIZE_Z)
			));
		}

		ReturnIfFalse(pCmdList->CopyTexture(m_pReadBackTexture.Get(), FTextureSlice{}, m_pSdfOutputTexture.Get(), FTextureSlice{}));

		ReturnIfFalse(pCmdList->Close());

		if (Type == ERenderPassType::Exclude)
        {
			m_Bvh.Clear();
			m_pBvhNodeBuffer.Reset();
			m_pBvhVertexBuffer.Reset();

			std::vector<FLOAT> SdfData(SDF_RESOLUTION * SDF_RESOLUTION * SDF_RESOLUTION);
			HANDLE FenceEvent = CreateEvent(nullptr, false, false, nullptr);

			UINT64 stRowPitch = 0;
			UINT64 stRowSize = sizeof(FLOAT) * SDF_RESOLUTION;
			UINT8* pMappedData = static_cast<UINT8*>(m_pReadBackTexture->Map(FTextureSlice{}, ECpuAccessMode::Read, FenceEvent, &stRowPitch));
			ReturnIfFalse(pMappedData && stRowPitch == stRowSize);

			UINT8* Dst = reinterpret_cast<UINT8*>(SdfData.data());
			for (UINT32 z = 0; z < SDF_RESOLUTION; ++z)
			{
				for (UINT32 y = 0; y < SDF_RESOLUTION; ++y)
				{
					UINT8* Src = pMappedData + stRowPitch * y;
					memcpy(Dst, Src, stRowSize);
					Dst += stRowSize;
				}
				pMappedData += stRowPitch * SDF_RESOLUTION;
			}

			std::string strProjDir = PROJ_DIR;
			std::ofstream fout(strProjDir + "D:/Document/Code/FTSRender/Asset/Sdf/Model");

			UINT64 stIndex = 0;
			for (UINT32 z = 0; z < SDF_RESOLUTION; ++z)
				for (UINT32 y = 0; y < SDF_RESOLUTION; ++y)
					for (UINT32 x = 0; x < SDF_RESOLUTION; ++x)
						fout << SdfData[stIndex++] << " ";

			m_pSdfOutputTexture.Reset();
			m_pReadBackTexture.Reset();
		}

        return true;
    }

    BOOL FSdfGeneratePass::BuildBvh()
    {
		ReturnIfFalse(m_cpMesh != nullptr);

		UINT64 stIndicesNum = 0;
        for (const auto& crMesh : m_cpMesh->SubMeshes)
        {
			stIndicesNum += crMesh.Indices.size();
        }

        UINT64 ix = 0;
        std::vector<FBvh::Vertex> BvhVertices(stIndicesNum);
        for (const auto& crMesh : m_cpMesh->SubMeshes)
        {
            for (auto VertexId : crMesh.Indices)
            {
                BvhVertices[ix++] = { 
					crMesh.Vertices[VertexId].Position, 
					crMesh.Vertices[VertexId].Normal 
				};
            }
        }
        ReturnIfFalse(ix == stIndicesNum);

        m_Bvh.Build(BvhVertices, stIndicesNum / 3);

        return true;
    }

}