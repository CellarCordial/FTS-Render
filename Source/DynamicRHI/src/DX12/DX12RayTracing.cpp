#include "DX12Converts.h"
#include <combaseapi.h>
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <winnt.h>
#if RAY_TRACING
#include "DX12RayTracing.h"
#include "DX12Resource.h"
#include "DX12Pipeline.h"

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

		FDX12ShaderTable::FDX12ShaderTable(const FDX12Context* cpContext, IPipeline* pPipeline) : 
			m_cpContext(cpContext), m_pPipeline(pPipeline)
		{
		}


		void FDX12ShaderTable::SetRayGenShader(const CHAR* strName, IBindingSet* pBindingSet)
		{
			FDX12Pipeline* pDX12Pipeline;
			m_pPipeline->QueryInterface(IID_IPipeline, PPV_ARG(&pDX12Pipeline));

			const auto* pExport = pDX12Pipeline->GetExport(strName);
			if (VerifyExport(pExport, pBindingSet))
			{
				m_RayGenShader.cpvShaderIdentifier = pExport->cpvShaderIdentifier;
				m_RayGenShader.pBindingSet = pBindingSet;
				m_dwVersion++;
			}
		}

		INT32 FDX12ShaderTable::AddMissShader(const CHAR* strName, IBindingSet* pBindingSet)
		{
			FDX12Pipeline* pDX12Pipeline;
			m_pPipeline->QueryInterface(IID_IPipeline, PPV_ARG(&pDX12Pipeline));

			const auto* pExport = pDX12Pipeline->GetExport(strName);
			if (VerifyExport(pExport, pBindingSet))
			{
				m_MissShaders.emplace_back(ShaderEntry{ 
					.pBindingSet = pBindingSet, 
					.cpvShaderIdentifier = pExport->cpvShaderIdentifier 
				});
				m_dwVersion++;
				return static_cast<INT32>(m_MissShaders.size() - 1);
			}

			return -1;
		}

		INT32 FDX12ShaderTable::AddHitGroup(const CHAR* strName, IBindingSet* pBindingSet)
		{
			FDX12Pipeline* pDX12Pipeline;
			m_pPipeline->QueryInterface(IID_IPipeline, PPV_ARG(&pDX12Pipeline));

			const auto* pExport = pDX12Pipeline->GetExport(strName);
			if (VerifyExport(pExport, pBindingSet))
			{
				m_HitGroups.emplace_back(ShaderEntry{ 
					.pBindingSet = pBindingSet, 
					.cpvShaderIdentifier = pExport->cpvShaderIdentifier 
				});
				m_dwVersion++;
				return static_cast<INT32>(m_HitGroups.size() - 1);
			}

			return -1;
		}

		INT32 FDX12ShaderTable::AddCallableShader(const CHAR* strName, IBindingSet* pBindingSet)
		{
			FDX12Pipeline* pDX12Pipeline;
			m_pPipeline->QueryInterface(IID_IPipeline, PPV_ARG(&pDX12Pipeline));

			const auto* pExport = pDX12Pipeline->GetExport(strName);
			if (VerifyExport(pExport, pBindingSet))
			{
				m_CallableShaders.emplace_back(ShaderEntry{ 
					.pBindingSet = pBindingSet, 
					.cpvShaderIdentifier = pExport->cpvShaderIdentifier 
				});
				m_dwVersion++;
				return static_cast<INT32>(m_CallableShaders.size() - 1);
			}

			return -1;
		}

		void FDX12ShaderTable::ClearMissShaders()
		{
			m_MissShaders.clear();
			m_dwVersion++;
		}

		void FDX12ShaderTable::ClearHitShaders()
		{
			m_HitGroups.clear();
			m_dwVersion++;
		}

		void FDX12ShaderTable::ClearCallableShaders()
		{
			m_CallableShaders.clear();
			m_dwVersion++;
		}

		bool FDX12ShaderTable::VerifyExport(const FDX12ExportTableEntry* pExport, IBindingSet* pBindingSet) const
		{
			if (!pExport)
			{
				LOG_ERROR("Couldn't find a DXR PSO export with a given name");
				return false;
			}

			if (pExport->pBindingLayout.Get() && !pBindingSet)
			{
				LOG_ERROR("A shader table entry does not provide required local bindings");
				return false;
			}

			if (!pExport->pBindingLayout.Get() && pBindingSet)
			{
				LOG_ERROR("A shader table entry provides local bindings, but none are required");
				return false;
			}

			if (pBindingSet && (CheckedCast<FDX12BindingSet*>(pBindingSet)->GetLayout() != pExport->pBindingLayout.Get()))
			{
				LOG_ERROR("A shader table entry provides local bindings that do not match the expected layout");
				return false;
			}

			return true;
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
			return pDX12Buffer->Initialize() && pDX12Buffer->QueryInterface(IID_IBuffer, PPV_ARG(m_pDataBuffer.GetAddressOf()));
		}
		

		BOOL FDX12Pipeline::Initialize(	
			const std::vector<IDX12RootSignature*>& crpDX12ShaderRootSigs,
			const std::vector<IDX12RootSignature*>& crpDX12HitGroupRootSigs,
			IDX12RootSignature* pDX12GlobalRootSig
		)
		{
			ReturnIfFalse(
				crpDX12ShaderRootSigs.size() == m_Desc.Shaders.size() &&
				crpDX12HitGroupRootSigs.size() == m_Desc.HitGroups.size()
			);

			struct Library
			{
				const void* cpvBlob;
				SIZE_T stBlobSize = 0;
				std::vector<std::wstring> ExportNames;
				std::vector<D3D12_EXPORT_DESC> D3D12ExportDescs;
			};

			std::unordered_map<const void*, Library> DxilLibrarys;
			for (UINT32 ix = 0; ix < m_Desc.Shaders.size(); ++ix)
			{
				const auto& crShader = m_Desc.Shaders[ix];

				const void* cpvBlob = nullptr;
				SIZE_T stBlobSize = 0;
				ReturnIfFalse(crShader.pShader->GetBytecode(&cpvBlob, &stBlobSize));

				std::string strExportName = crShader.pShader->GetDesc().strEntryName;

				auto& rLibrary = DxilLibrarys[cpvBlob];
				rLibrary.cpvBlob = cpvBlob;
				rLibrary.stBlobSize = stBlobSize;
				rLibrary.ExportNames.emplace_back(strExportName.begin(), strExportName.end());

				if (crShader.pBindingLayout && crpDX12ShaderRootSigs[ix])
				{
					m_LocalBindingRootMap[crShader.pBindingLayout] = crpDX12ShaderRootSigs[ix];

					FDX12BindingLayout* pDX12BindingLayout = CheckedCast<FDX12BindingLayout*>(crShader.pBindingLayout);
					m_dwMaxLocalRootParameter = std::max(m_dwMaxLocalRootParameter, static_cast<UINT32>(pDX12BindingLayout->m_RootParameters.size()));
				}
			}

			std::vector<std::wstring> HitGroupExportNames;	// 防止 wstring 析构.
			std::unordered_map<const IShader*, std::wstring> HitGroupShaderExportNames;
			HitGroupShaderExportNames.reserve(m_Desc.HitGroups.size());

			std::vector<D3D12_HIT_GROUP_DESC> D3D12HitGroupDescs;
			for (UINT32 ix = 0; ix < m_Desc.HitGroups.size(); ++ix)
			{
				const auto& crHitGroup = m_Desc.HitGroups[ix];

				for (const auto* cpShader : { crHitGroup.pClosestHitShader, crHitGroup.pAnyHitShader, crHitGroup.pIntersectShader })
				{
					if (!cpShader) continue;

					auto& rstrExportName = HitGroupShaderExportNames[cpShader];
					if (rstrExportName.empty())
					{
						const void* cpvBlob = nullptr;
						SIZE_T stBlobSize = 0;
						ReturnIfFalse(cpShader->GetBytecode(&cpvBlob, &stBlobSize));

						std::string strExportName = cpShader->GetDesc().strEntryName;

						auto& rLibrary = DxilLibrarys[cpvBlob];
						rLibrary.cpvBlob = cpvBlob;
						rLibrary.stBlobSize = stBlobSize;
						rLibrary.ExportNames.emplace_back(strExportName.begin(), strExportName.end());

						rstrExportName = rLibrary.ExportNames.back();
					}
				}

				if (crHitGroup.pBindingLayout && crpDX12HitGroupRootSigs[ix])
				{
					m_LocalBindingRootMap[crHitGroup.pBindingLayout] = crpDX12HitGroupRootSigs[ix];

					FDX12BindingLayout* pDX12BindingLayout = CheckedCast<FDX12BindingLayout*>(crHitGroup.pBindingLayout);
					m_dwMaxLocalRootParameter = std::max(m_dwMaxLocalRootParameter, static_cast<UINT32>(pDX12BindingLayout->m_RootParameters.size()));
				}

				auto& rD3D12HitGroupDesc = D3D12HitGroupDescs.emplace_back();
				if (crHitGroup.pClosestHitShader) rD3D12HitGroupDesc.ClosestHitShaderImport = HitGroupShaderExportNames[crHitGroup.pClosestHitShader].c_str();
				if (crHitGroup.pAnyHitShader) rD3D12HitGroupDesc.AnyHitShaderImport = HitGroupShaderExportNames[crHitGroup.pAnyHitShader].c_str();
				if (crHitGroup.pIntersectShader) rD3D12HitGroupDesc.IntersectionShaderImport = HitGroupShaderExportNames[crHitGroup.pIntersectShader].c_str();

				rD3D12HitGroupDesc.Type = crHitGroup.bIsProceduralPrimitive ? D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE : D3D12_HIT_GROUP_TYPE_TRIANGLES;

				HitGroupExportNames.emplace_back(crHitGroup.strExportName.begin(), crHitGroup.strExportName.end());
				rD3D12HitGroupDesc.HitGroupExport = HitGroupExportNames.back().c_str();
			}

			std::vector<D3D12_DXIL_LIBRARY_DESC> D3D12DxilLibraries;
			D3D12DxilLibraries.reserve(DxilLibrarys.size());
			for (auto& [rcpvBlob, rLibrary] : DxilLibrarys)
			{
				for (const auto& crExportName : rLibrary.ExportNames)
				{
					D3D12_EXPORT_DESC D3D12ExportDesc = {};
					D3D12ExportDesc.ExportToRename = crExportName.c_str();
					D3D12ExportDesc.Name = crExportName.c_str();
					D3D12ExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
					rLibrary.D3D12ExportDescs.push_back(D3D12ExportDesc);
				}

				D3D12_DXIL_LIBRARY_DESC D3D12LibraryDesc{};
				D3D12LibraryDesc.DXILLibrary.pShaderBytecode = rLibrary.cpvBlob;
				D3D12LibraryDesc.DXILLibrary.BytecodeLength = rLibrary.stBlobSize;
				D3D12LibraryDesc.NumExports = static_cast<UINT32>(rLibrary.D3D12ExportDescs.size());
				D3D12LibraryDesc.pExports = rLibrary.D3D12ExportDescs.data();

				D3D12DxilLibraries.push_back(D3D12LibraryDesc);
			}

			std::vector<D3D12_STATE_SUBOBJECT> D3D12StateSubobjects;

			D3D12_RAYTRACING_SHADER_CONFIG ShaderConfig{};
			ShaderConfig.MaxAttributeSizeInBytes = m_Desc.dwMaxAttributeSize;
			ShaderConfig.MaxPayloadSizeInBytes = m_Desc.dwMaxPayloadSize;
			D3D12StateSubobjects.emplace_back(
				D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,
				&ShaderConfig
			);

			D3D12_RAYTRACING_PIPELINE_CONFIG PipelineConfig{};
			PipelineConfig.MaxTraceRecursionDepth = m_Desc.dwMaxRecursionDepth;
			D3D12StateSubobjects.emplace_back(
				D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG,
				&PipelineConfig
			);

			for (const auto& crDxilLibrary : D3D12DxilLibraries)
			{
				D3D12StateSubobjects.emplace_back(
					D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,
					&crDxilLibrary
				);
			}

			for (const auto& crHitGroupDesc : D3D12HitGroupDescs)
			{
				D3D12StateSubobjects.emplace_back(
					D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,
					&crHitGroupDesc
				);
			}


			D3D12_GLOBAL_ROOT_SIGNATURE D3D12GlobalRootSig;
			if (!m_Desc.pGlobalBindingLayouts.Empty() && pDX12GlobalRootSig)
			{
				m_pGlobalRootSignature = pDX12GlobalRootSig;
				D3D12GlobalRootSig.pGlobalRootSignature = CheckedCast<FDX12RootSignature*>(pDX12GlobalRootSig)->m_pD3D12RootSignature.Get();
				D3D12StateSubobjects.emplace_back(
					D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE,
					&D3D12GlobalRootSig
				);
			}

			D3D12StateSubobjects.reserve(D3D12StateSubobjects.size() + m_LocalBindingRootMap.size() * 2);

			SIZE_T stNumAssociations = m_Desc.Shaders.size() + m_Desc.HitGroups.size();
			std::vector<std::wstring> wstrAssociateExports;
			std::vector<LPCWSTR> cwstrAssociateExportsCStr;
			wstrAssociateExports.reserve(stNumAssociations);
			cwstrAssociateExportsCStr.reserve(stNumAssociations);

			for (const auto& [crpBindingLayout, crpDX12RootSig] : m_LocalBindingRootMap)
			{
				auto pD3D12LocalRootSig = new_on_stack(D3D12_LOCAL_ROOT_SIGNATURE);
				pD3D12LocalRootSig->pLocalRootSignature = CheckedCast<FDX12RootSignature*>(crpDX12RootSig)->m_pD3D12RootSignature.Get();

				D3D12StateSubobjects.emplace_back(
					D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE,
					pD3D12LocalRootSig
				);

				auto pD3D12Association = new_on_stack(D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
				pD3D12Association->NumExports = 0;
				pD3D12Association->pSubobjectToAssociate = &D3D12StateSubobjects.back();

				SIZE_T stExportIndexOffset = cwstrAssociateExportsCStr.size();

				for (const auto& crShader : m_Desc.Shaders)
				{
					if (crShader.pBindingLayout == crpBindingLayout)
					{
						std::string strExportName = crShader.pShader->GetDesc().strEntryName;
						wstrAssociateExports.emplace_back(strExportName.begin(), strExportName.end());
						cwstrAssociateExportsCStr.emplace_back(wstrAssociateExports.back().c_str());
						pD3D12Association->NumExports++;
					}
				}

				for (const auto& crHitGroup : m_Desc.HitGroups)
				{
					if (crHitGroup.pBindingLayout == crpBindingLayout)
					{
						wstrAssociateExports.emplace_back(crHitGroup.strExportName.begin(), crHitGroup.strExportName.end());
						cwstrAssociateExportsCStr.emplace_back(wstrAssociateExports.back().c_str());
						pD3D12Association->NumExports++;
					}
				}

				pD3D12Association->pExports = &cwstrAssociateExportsCStr[stExportIndexOffset];

				D3D12StateSubobjects.emplace_back(
					D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
					pD3D12Association
				);
			}

			D3D12_STATE_OBJECT_DESC PipelineDesc;
			PipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
			PipelineDesc.NumSubobjects = static_cast<UINT32>(D3D12StateSubobjects.size());
			PipelineDesc.pSubobjects = D3D12StateSubobjects.data();

			if (
				FAILED(m_cpContext->pDevice5->CreateStateObject(&PipelineDesc, IID_PPV_ARGS(m_pD3D12StateObject.GetAddressOf()))) ||
				FAILED(m_pD3D12StateObject->QueryInterface(IID_PPV_ARGS(m_pD3D12StateObjectProperties.GetAddressOf())))
			)
			{
				LOG_ERROR("Failed to initialize ray tracing pipeline.");
				return false;
			}

			for (const auto& crShader : m_Desc.Shaders)
			{
				std::string strExportName = crShader.pShader->GetDesc().strEntryName;
				std::wstring wstrExportName(strExportName.begin(), strExportName.end());
				const void* cpvShaderIdentifier = m_pD3D12StateObjectProperties->GetShaderIdentifier(wstrExportName.c_str());

				ReturnIfFalse(cpvShaderIdentifier != nullptr);

				m_Exports[strExportName] = FDX12ExportTableEntry{ 
					.pBindingLayout = crShader.pBindingLayout, 
					.cpvShaderIdentifier = cpvShaderIdentifier
				};
			}

			for (const auto& crHitGroup : m_Desc.HitGroups)
			{
				std::wstring wstrExportName(crHitGroup.strExportName.begin(), crHitGroup.strExportName.end());
				const void* cpvShaderIdentifier = m_pD3D12StateObjectProperties->GetShaderIdentifier(wstrExportName.c_str());

				ReturnIfFalse(cpvShaderIdentifier != nullptr);

				m_Exports[crHitGroup.strExportName] = FDX12ExportTableEntry{ 
					.pBindingLayout = crHitGroup.pBindingLayout, 
					.cpvShaderIdentifier = cpvShaderIdentifier
				};
			}

			return true;
		}


		BOOL FDX12Pipeline::CreateShaderTable(CREFIID criid, void** ppvShaderTable)
		{
			FDX12ShaderTable* pDX12ShaderTable = new FDX12ShaderTable(m_cpContext, this);
			return pDX12ShaderTable->QueryInterface(criid, ppvShaderTable);
		}

		
		const FDX12ExportTableEntry* FDX12Pipeline::GetExport(const CHAR* strName)
		{
			auto Iter = m_Exports.find(strName);
			if (Iter != m_Exports.end())
			{
				return &Iter->second;
			}
			return nullptr;
		}

		UINT32 FDX12Pipeline::GetShaderTableEntrySize() const
		{
			UINT32 dwRequiredSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(UINT64) * m_dwMaxLocalRootParameter;
			return Align(dwRequiredSize, UINT32(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
		}
	}


		
}


#endif
