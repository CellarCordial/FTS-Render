#ifndef RHI_DX12_RAY_TRACING_H
#define RHI_DX12_RAY_TRACING_H

#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <wrl/client.h>

#include "dx12_descriptor.h"

namespace fantasy
{
	namespace ray_tracing
	{
		struct DX12ShaderTableState
		{
			uint32_t committed_version = 0;
			ID3D12DescriptorHeap* d3d12_descriptor_heap_srv = nullptr;
			ID3D12DescriptorHeap* d3d12_descriptor_heap_samplers = nullptr;
			D3D12_DISPATCH_RAYS_DESC d3d12_dispatch_rays_desc = {};
		};

		struct DX12ExportTableEntry
		{
			BindingLayoutInterface* binding_layout;
			const void* shader_identifier = nullptr;
		};

		struct DX12GeometryDesc
		{
            D3D12_RAYTRACING_GEOMETRY_TYPE type;
            D3D12_RAYTRACING_GEOMETRY_FLAGS Flags;
            union
            {
                D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC triangles;
                D3D12_RAYTRACING_GEOMETRY_AABBS_DESC aabbs;
			};
		};

		struct DX12AccelStructBuildInputs
		{
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE type;
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS Flags;
			uint32_t dwDescNum;
			D3D12_ELEMENTS_LAYOUT DescsLayout;

			union 
			{
				D3D12_GPU_VIRTUAL_ADDRESS InstanceAddress;
				const DX12GeometryDesc* const* cpcpGeometryDesc;
			};

			std::vector<DX12GeometryDesc> GeometryDescs;
			std::vector<DX12GeometryDesc*> geometry_descs;

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Convert() const
			{
				return D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS{
					.Type = type,
					.Flags = Flags,
					.NumDescs = dwDescNum,
					.DescsLayout = DescsLayout,
					.InstanceDescs = InstanceAddress
				};
			}
		};

		class DX12AccelStruct : public AccelStructInterface
		{
		public:
			DX12AccelStruct(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps, const AccelStructDesc& desc);

			bool initialize();

			// AccelStructInterface.
			const AccelStructDesc& get_desc() const override { return _desc; }
			BufferInterface* get_buffer() const override { return _data_buffer.get(); }
			MemoryRequirements get_memory_requirements() override;
			bool bind_memory(HeapInterface* heap, uint64_t offset = 0) override;


			void create_srv(size_t descriptor) const;
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO get_accel_struct_prebuild_info();
			static DX12GeometryDesc convert_geometry_desc(const GeometryDesc& geometry_desc, D3D12_GPU_VIRTUAL_ADDRESS gpu_address);

		public:
			std::vector<D3D12_RAYTRACING_INSTANCE_DESC> d3d12_ray_tracing_instance_descs;
			std::vector<std::shared_ptr<AccelStructInterface>> bottom_level_accel_structs;

			AccelStructDesc _desc;

		private:
			const DX12Context* _context;
        	DX12DescriptorHeaps* _descriptor_heaps;

			std::unique_ptr<BufferInterface> _data_buffer;
		};

		class DX12ShaderTable : public ShaderTableInterface
		{
		public:
			DX12ShaderTable(const DX12Context* context, PipelineInterface* pipeline);

			bool initalize() { return true; }

			// ShaderTableInterface.
			void set_raygen_shader(const char* name, BindingSetInterface* binding_set = nullptr) override;
			int32_t add_miss_shader(const char* name, BindingSetInterface* binding_set = nullptr) override;
			int32_t add_hit_group(const char* name, BindingSetInterface* binding_set = nullptr) override;
			int32_t add_callable_shader(const char* name, BindingSetInterface* binding_set = nullptr) override;
			void clear_miss_shaders() override;
			void clear_hit_groups() override;
			void clear_callable_shaders() override;
			PipelineInterface* get_pipeline() const override { return _pipeline; }

			
			struct ShaderEntry
			{
				BindingSetInterface* binding_set;
				const void* shader_identifier = nullptr;
			};

			uint32_t get_entry_count() const;
			uint32_t _version = 0;
			ShaderEntry _raygen_shader;
			std::vector<ShaderEntry> _hit_groups;
			std::vector<ShaderEntry> _miss_shaders;
			std::vector<ShaderEntry> _callable_shaders;

		private:
			bool verify_export(const DX12ExportTableEntry* export_name, BindingSetInterface* binding_set) const;

		private:
			const DX12Context* _context;

			PipelineInterface* _pipeline;
		};


		class DX12Pipeline : public PipelineInterface
		{
		public:
			DX12Pipeline(const DX12Context* context) : _context(context) {}

			bool initialize(
				std::vector<std::unique_ptr<DX12RootSignature>>&& shader_root_signatures,
				std::vector<std::unique_ptr<DX12RootSignature>>&& dx12_hit_group_root_signatures,
				std::unique_ptr<DX12RootSignature>&& dx12_global_root_signature
			);

			// PipelineInterface.
			const PipelineDesc& get_desc() const override { return _desc; }
			ShaderTableInterface* create_shader_table() override;
        	void* get_native_object() override { return _d3d12_state_object.Get(); }

			const DX12ExportTableEntry* get_export(const char* name);
			uint32_t get_shaderTableEntrySize() const;
		
			std::unique_ptr<DX12RootSignature> _global_root_signature;

		private:
			const DX12Context* _context;

			PipelineDesc _desc;
			uint32_t _max_local_root_parameter_count = 0;
			std::unordered_map<BindingLayoutInterface*, std::unique_ptr<DX12RootSignature>> _local_binding_roots;
			std::unordered_map<std::string, DX12ExportTableEntry> export_names;
			Microsoft::WRL::ComPtr<ID3D12StateObject> _d3d12_state_object;
			Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> _d3d12_state_object_properties;
		};
	}

}

#endif