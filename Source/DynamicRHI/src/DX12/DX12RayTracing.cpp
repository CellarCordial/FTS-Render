#include "DX12Converts.h"
#include <d3d12.h>
#if RAY_TRACING
#include "DX12RayTracing.h"
#include "DX12Resource.h"

namespace FTS
{
	namespace RayTracing
	{
		struct FDX12GeometryDesc
		{
            D3D12_RAYTRACING_GEOMETRY_TYPE Type;
            D3D12_RAYTRACING_GEOMETRY_FLAGS Flags;
            union
            {
                D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC Triangles;
                D3D12_RAYTRACING_GEOMETRY_AABBS_DESC AABBs;
			};
		};

		struct FDX12AccelStructBuildInputs
		{
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE Type;
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS Flags;
			UINT32 dwNumDescs;
			D3D12_ELEMENTS_LAYOUT DescsLayout;

			union 
			{
				D3D12_GPU_VIRTUAL_ADDRESS InstanceAddress;
				const FDX12GeometryDesc* const* cpcpGeometryDesc;
			};

			std::vector<FDX12GeometryDesc> GeometryDescs;
			std::vector<FDX12GeometryDesc*> pGeometryDescs;

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Convert() const
			{
				return D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS{
					.Type = Type,
					.Flags = Flags,
					.NumDescs = dwNumDescs,
					.DescsLayout = DescsLayout,
					.InstanceDescs = InstanceAddress
				};
			}
		};

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO FDX12AccelStruct::GetAccelStructPrebuildInfo()
		{
			FDX12AccelStructBuildInputs BuildInputs;
			BuildInputs.Flags = ConvertAccelStructureBuildFlags(m_Desc.Flags);
			if (m_Desc.bIsTopLevel)
			{
				BuildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
				BuildInputs.InstanceAddress = D3D12_GPU_VIRTUAL_ADDRESS{0};
				BuildInputs.dwNumDescs = m_Desc.stTopLevelMaxInstantNum;
				BuildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			}
			else
			{
				BuildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
				BuildInputs.dwNumDescs = static_cast<UINT32>(m_Desc.BottomLevelGeometryDescs.size());
				BuildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY_OF_POINTERS;
				BuildInputs.GeometryDescs.resize(m_Desc.BottomLevelGeometryDescs.size());
				BuildInputs.pGeometryDescs.resize(m_Desc.BottomLevelGeometryDescs.size());
				for (UINT32 ix = 0; ix < static_cast<UINT32>(BuildInputs.GeometryDescs.size()); ++ix)
				{
					BuildInputs.pGeometryDescs[ix] = BuildInputs.GeometryDescs.data() + ix;
				}
				BuildInputs.cpcpGeometryDesc = BuildInputs.pGeometryDescs.data();

				for (UINT32 ix = 0; ix < static_cast<UINT32>(m_Desc.BottomLevelGeometryDescs.size()); ++ix)
				{
					auto& rDstGeometryDesc = BuildInputs.GeometryDescs[ix];
					const auto& crSrcGeometryDesc = m_Desc.BottomLevelGeometryDescs[ix];

					rDstGeometryDesc.Flags = ConvertGeometryFlags(crSrcGeometryDesc.Flags);
					if (crSrcGeometryDesc.Type == EGeometryType::Triangle)
					{
						rDstGeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

						auto& rDstTriangles = rDstGeometryDesc.Triangles;
						const auto& crSrcTriangles = crSrcGeometryDesc.Triangles;

						if (crSrcGeometryDesc.Triangles.pIndexBuffer)
						{
							FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(crSrcTriangles.pIndexBuffer);
							rDstTriangles.IndexBuffer = pDX12Buffer->m_GpuAddress + crSrcTriangles.stIndexOffset;
						}
						else
						{
							rDstTriangles.IndexBuffer = D3D12_GPU_VIRTUAL_ADDRESS{0};
						}
						
						if (crSrcTriangles.pVertexBuffer)
						{
							FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(crSrcTriangles.pVertexBuffer);
							rDstTriangles.VertexBuffer.StartAddress = pDX12Buffer->m_GpuAddress + crSrcTriangles.stVertexOffset;
						}
						else
						{
							rDstTriangles.VertexBuffer.StartAddress = D3D12_GPU_VIRTUAL_ADDRESS{0};
						}

						rDstTriangles.VertexBuffer.StrideInBytes = crSrcTriangles.dwVertexStride;
						rDstTriangles.IndexFormat = GetDxgiFormatMapping(crSrcTriangles.IndexFormat).SRVFormat;
						rDstTriangles.VertexFormat = GetDxgiFormatMapping(crSrcTriangles.VertexFormat).SRVFormat;
						rDstTriangles.IndexCount = crSrcTriangles.dwIndexCount;
						rDstTriangles.VertexCount = crSrcTriangles.dwVertexCount;

						rDstTriangles.Transform3x4 = crSrcGeometryDesc.bUseTransform ? D3D12_GPU_VIRTUAL_ADDRESS{16} : D3D12_GPU_VIRTUAL_ADDRESS{0};
					}
					else
					{
						rDstGeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;

						if (crSrcGeometryDesc.AABBs.pBuffer)
						{
							FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(crSrcGeometryDesc.AABBs.pBuffer);
							rDstGeometryDesc.AABBs.AABBs.StartAddress = pDX12Buffer->m_GpuAddress + crSrcGeometryDesc.AABBs.stOffset;
						}
						else
						{
							rDstGeometryDesc.AABBs.AABBs.StartAddress = D3D12_GPU_VIRTUAL_ADDRESS{0};
						}

						rDstGeometryDesc.AABBs.AABBs.StrideInBytes = crSrcGeometryDesc.AABBs.dwStride;
						rDstGeometryDesc.AABBs.AABBCount = crSrcGeometryDesc.AABBs.dwCount;
					}
				}
			}

			auto Inputs = BuildInputs.Convert();
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PreBuildInfo;
			m_cpContext->pDevice5->GetRaytracingAccelerationStructurePrebuildInfo(&Inputs, &PreBuildInfo);
			return PreBuildInfo;
		}


		void FDX12ShaderTable::SetRayGenerationShader(const CHAR* strName, IBindingSet* pBindingSet)
		{
		}

		INT32 FDX12ShaderTable::AddMissShader(const CHAR* strName, IBindingSet* pBindingSet)
		{
		}

		INT32 FDX12ShaderTable::AddHitGroup(const CHAR* strName, IBindingSet* pBindingSet)
		{

		}

		INT32 FDX12ShaderTable::AddCallableShader(const CHAR* strName, IBindingSet* pBindingSet)
		{

		}

		void FDX12ShaderTable::ClearMissShaders()
		{

		}

		void FDX12ShaderTable::ClearHitShaders()
		{

		}

		void FDX12ShaderTable::ClearCallableShaders()
		{

		}

		IPipeline* FDX12ShaderTable::GetPipeline() const
		{

		}

		FDX12AccelStruct::FDX12AccelStruct(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps, const FAccelStructDesc& crDesc) :
			m_cpContext(cpContext), m_pDescriptorHeaps(pDescriptorHeaps), m_Desc(crDesc)
		{
		}

		BOOL FDX12AccelStruct::Initialize()
		{
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = GetAccelStructPrebuildInfo();
			ReturnIfFalse(ASPreBuildInfo.ResultDataMaxSizeInBytes > 0);

			FBufferDesc Desc = FBufferDesc::CreateAccelStruct(ASPreBuildInfo.ResultDataMaxSizeInBytes, m_Desc.bIsTopLevel);
			Desc.bIsVirtual = m_Desc.bIsVirtual;
			FDX12Buffer* pDX12Buffer = new FDX12Buffer(m_cpContext, m_pDescriptorHeaps, Desc);
			return pDX12Buffer->Initialize() && pDX12Buffer->QueryInterface(IID_IBuffer, PPV_ARG(m_pDataBuffer.GetAddressOf())))
		}

		UINT64 FDX12AccelStruct::GetDeviceAddress() const
		{
			return CheckedCast<FDX12Buffer*>(m_pDataBuffer)->m_GpuAddress;
		}

	}

}


#endif
