#ifndef RHI_DX12_RAY_TRACING_H
#define RHI_DX12_RAY_TRACING_H

#include "DX12Forward.h"
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <wrl/client.h>
#if RAY_TRACING
#include "../../../Core/include/ComRoot.h"
#include "../../../Core/include/ComCli.h"
#include "../../include/RayTracing.h"
#include "DX12Descriptor.h"

namespace FTS
{
	namespace RayTracing
	{
		struct FDX12ShaderTableState
		{
			UINT32 dwCommittedVersion = 0;
			ID3D12DescriptorHeap* pDescriptorHeapSRV = nullptr;
			ID3D12DescriptorHeap* pDescriptorHeapSamplers = nullptr;
			D3D12_DISPATCH_RAYS_DESC DispatchRaysDesc = {};
		};

		struct FDX12ExportTableEntry
		{
			TComPtr<IBindingLayout> pBindingLayout;
			const void* cpvShaderIdentifier = nullptr;
		};

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
			UINT32 dwDescNum;
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
					.NumDescs = dwDescNum,
					.DescsLayout = DescsLayout,
					.InstanceDescs = InstanceAddress
				};
			}
		};

		class FDX12AccelStruct :
			public TComObjectRoot<FComMultiThreadModel>,
			public IAccelStruct
		{
		public:
			BEGIN_INTERFACE_MAP(FDX12AccelStruct)
				INTERFACE_ENTRY(IID_IAccelStruct, IAccelStruct)
			END_INTERFACE_MAP

			FDX12AccelStruct(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps, const FAccelStructDesc& crDesc);

			BOOL Initialize();

			// IAccelStruct.
			const FAccelStructDesc& GetDesc() const override { return m_Desc; }

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO GetAccelStructPrebuildInfo();

			static void FillGeometryDesc(
				FDX12GeometryDesc& rDstGeometryDesc, 
				const FGeometryDesc& crSrcGeometryDesc, 
				D3D12_GPU_VIRTUAL_ADDRESS GpuAddress
			);


		public:
			FAccelStructDesc m_Desc;
			TComPtr<IBuffer> m_pDataBuffer;

			std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_DxrInstanceDescs;
			std::vector<TComPtr<IAccelStruct>> m_pBottomLevelAccelStructs;

		private:
			const FDX12Context* m_cpContext;
        	FDX12DescriptorHeaps* m_pDescriptorHeaps;

		};

		class FDX12ShaderTable :
			public TComObjectRoot<FComMultiThreadModel>,
			public IShaderTable
		{
		public:
			BEGIN_INTERFACE_MAP(FDX12ShaderTable)
				INTERFACE_ENTRY(IID_IShaderTable, IShaderTable)
			END_INTERFACE_MAP

			FDX12ShaderTable(const FDX12Context* cpContext, IPipeline* pPipeline);

			// IShaderTable.
			void SetRayGenShader(const CHAR* strName, IBindingSet* pBindingSet = nullptr) override;
			INT32 AddMissShader(const CHAR* strName, IBindingSet* pBindingSet = nullptr) override;
			INT32 AddHitGroup(const CHAR* strName, IBindingSet* pBindingSet = nullptr) override;
			INT32 AddCallableShader(const CHAR* strName, IBindingSet* pBindingSet = nullptr) override;
			void ClearMissShaders() override;
			void ClearHitShaders() override;
			void ClearCallableShaders() override;
			IPipeline* GetPipeline() const override { return m_pPipeline.Get(); }

		private:
			BOOL VerifyExport(const FDX12ExportTableEntry* pExport, IBindingSet* pBindingSet) const;

		private:
			const FDX12Context* m_cpContext;
			
			struct ShaderEntry
			{
				TComPtr<IBindingSet> pBindingSet;
				const void* cpvShaderIdentifier = nullptr;
			};

			UINT32 m_dwVersion = 0;

			TComPtr<IPipeline> m_pPipeline;
			ShaderEntry m_RayGenShader;
			std::vector<ShaderEntry> m_HitGroups;
			std::vector<ShaderEntry> m_MissShaders;
			std::vector<ShaderEntry> m_CallableShaders;
		};


		class FDX12Pipeline :
			public TComObjectRoot<FComMultiThreadModel>,
			public IPipeline
		{
		public:
			BEGIN_INTERFACE_MAP(FDX12Pipeline)
				INTERFACE_ENTRY(IID_IPipeline, IPipeline)
			END_INTERFACE_MAP

			FDX12Pipeline(const FDX12Context* cpContext) : m_cpContext(cpContext) {}

			BOOL Initialize(
				const std::vector<IDX12RootSignature*>& crpDX12ShaderRootSigs,
				const std::vector<IDX12RootSignature*>& crpDX12HitGroupRootSigs,
				IDX12RootSignature* pDX12GlobalRootSig = nullptr
			);

			// IPipeline.
			const FPipelineDesc& GetDesc() const override { return m_Desc; }
			BOOL CreateShaderTable(CREFIID criid, void** ppvShaderTable) override;

			const FDX12ExportTableEntry* GetExport(const CHAR* strName);
			UINT32 GetShaderTableEntrySize() const;
		
		private:
			const FDX12Context* m_cpContext;

			FPipelineDesc m_Desc;
			UINT32 m_dwMaxLocalRootParameter = 0;
			std::unordered_map<IBindingLayout*, IDX12RootSignature*> m_LocalBindingRootMap;
			std::unordered_map<std::string, FDX12ExportTableEntry> m_Exports;
			TComPtr<IDX12RootSignature> m_pGlobalRootSignature;
			Microsoft::WRL::ComPtr<ID3D12StateObject> m_pD3D12StateObject;
			Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> m_pD3D12StateObjectProperties;
		};
	}

}

#endif

#endif