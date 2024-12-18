#include "dx12_converts.h"
#include <combaseapi.h>
#include <d3d12.h>
#include <memory>
#include <rpcndr.h>
#include <string>
#include <unordered_map>
#include <winnt.h>
#if RAY_TRACING
#include "dx12_ray_tracing.h"
#include "dx12_resource.h"
#include "dx12_pipeline.h"
#include "../../core/tools/check_cast.h"


#ifndef new_on_stack
#define new_on_stack(T) (T*)alloca(sizeof(T))
#endif

namespace fantasy
{
	namespace ray_tracing
	{
		DX12GeometryDesc DX12AccelStruct::convert_geometry_desc(const GeometryDesc& geometry_desc, D3D12_GPU_VIRTUAL_ADDRESS gpu_address)
		{
			DX12GeometryDesc ret;
			ret.Flags = convert_geometry_flags(geometry_desc.flags);
			if (geometry_desc.type == GeometryType::Triangle)
			{
				ret.type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

				auto& dst_triangles = ret.triangles;
				const auto& crSrcTriangles = geometry_desc.triangles;

				if (geometry_desc.triangles.index_buffer)
				{
					ID3D12Resource* d3d12_resource = reinterpret_cast<ID3D12Resource*>(crSrcTriangles.index_buffer->get_native_object());
					dst_triangles.IndexBuffer = d3d12_resource->GetGPUVirtualAddress() + crSrcTriangles.index_offset;
				}
				else
				{
					dst_triangles.IndexBuffer = gpu_address;
				}
				
				if (crSrcTriangles.vertex_buffer)
				{
					ID3D12Resource* d3d12_resource = reinterpret_cast<ID3D12Resource*>(crSrcTriangles.vertex_buffer->get_native_object());
					dst_triangles.VertexBuffer.StartAddress = d3d12_resource->GetGPUVirtualAddress() + crSrcTriangles.vertex_offset;
				}
				else
				{
					dst_triangles.VertexBuffer.StartAddress = gpu_address;
				}

				dst_triangles.VertexBuffer.StrideInBytes = crSrcTriangles.vertex_stride;
				dst_triangles.IndexFormat = get_dxgi_format_mapping(crSrcTriangles.index_format).srv_format;
				dst_triangles.VertexFormat = get_dxgi_format_mapping(crSrcTriangles.vertex_format).srv_format;
				dst_triangles.IndexCount = crSrcTriangles.index_count;
				dst_triangles.VertexCount = crSrcTriangles.vertex_count;

				dst_triangles.Transform3x4 = gpu_address;
			}
			else
			{
				ret.type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;

				if (geometry_desc.aabbs.buffer)
				{
					ID3D12Resource* d3d12_resource = reinterpret_cast<ID3D12Resource*>(geometry_desc.aabbs.buffer->get_native_object());
					ret.aabbs.AABBs.StartAddress = d3d12_resource->GetGPUVirtualAddress() + geometry_desc.aabbs.offset;
				}
				else
				{
					ret.aabbs.AABBs.StartAddress = gpu_address;
				}

				ret.aabbs.AABBs.StrideInBytes = geometry_desc.aabbs.stride;
				ret.aabbs.AABBCount = geometry_desc.aabbs.count;
			}
			return ret;
		}

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO DX12AccelStruct::get_accel_struct_prebuild_info()
		{
			DX12AccelStructBuildInputs dx12_accle_struct_bulid_inputs;
			dx12_accle_struct_bulid_inputs.Flags = convert_accel_struct_build_flags(_desc.flags);
			if (_desc.is_top_level)
			{
				dx12_accle_struct_bulid_inputs.type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
				dx12_accle_struct_bulid_inputs.InstanceAddress = D3D12_GPU_VIRTUAL_ADDRESS{0};
				dx12_accle_struct_bulid_inputs.dwDescNum = _desc.top_level_max_instance_num;
				dx12_accle_struct_bulid_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			}
			else
			{
				dx12_accle_struct_bulid_inputs.type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
				dx12_accle_struct_bulid_inputs.dwDescNum = static_cast<uint32_t>(_desc.bottom_level_geometry_descs.size());
				dx12_accle_struct_bulid_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY_OF_POINTERS;
				dx12_accle_struct_bulid_inputs.GeometryDescs.resize(_desc.bottom_level_geometry_descs.size());
				dx12_accle_struct_bulid_inputs.geometry_descs.resize(_desc.bottom_level_geometry_descs.size());
				for (uint32_t ix = 0; ix < static_cast<uint32_t>(dx12_accle_struct_bulid_inputs.GeometryDescs.size()); ++ix)
				{
					dx12_accle_struct_bulid_inputs.geometry_descs[ix] = dx12_accle_struct_bulid_inputs.GeometryDescs.data() + ix;
				}
				dx12_accle_struct_bulid_inputs.cpcpGeometryDesc = dx12_accle_struct_bulid_inputs.geometry_descs.data();

				for (uint32_t ix = 0; ix < static_cast<uint32_t>(_desc.bottom_level_geometry_descs.size()); ++ix)
				{
					auto& dst_geometry_desc = dx12_accle_struct_bulid_inputs.GeometryDescs[ix];
					const auto& geometry_desc = _desc.bottom_level_geometry_descs[ix];

					dst_geometry_desc = convert_geometry_desc(
						geometry_desc, 
						geometry_desc.use_transform ? D3D12_GPU_VIRTUAL_ADDRESS{16} : D3D12_GPU_VIRTUAL_ADDRESS{0}
					);
				}
			}

			auto d3d12_inputs = dx12_accle_struct_bulid_inputs.Convert();
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PreBuildInfo;
			_context->device5->GetRaytracingAccelerationStructurePrebuildInfo(&d3d12_inputs, &PreBuildInfo);
			return PreBuildInfo;
		}

		ShaderTable::ShaderTable(const DX12Context* context, PipelineInterface* pipeline) : 
			_context(context), _pipeline(pipeline)
		{
		}


		void ShaderTable::set_raygen_shader(const char* name, BindingSetInterface* binding_set)
		{
			DX12Pipeline* dx12_pipeline = check_cast<DX12Pipeline*>(_pipeline);

			const auto* export_name = dx12_pipeline->get_export(name);
			if (verify_export(export_name, binding_set))
			{
				_raygen_shader.shader_identifier = export_name->shader_identifier;
				_raygen_shader.binding_set = binding_set;
				_version++;
			}
		}

		int32_t ShaderTable::add_miss_shader(const char* name, BindingSetInterface* binding_set)
		{
			DX12Pipeline* dx12_pipeline = check_cast<DX12Pipeline*>(_pipeline);

			const auto* export_name = dx12_pipeline->get_export(name);
			if (verify_export(export_name, binding_set))
			{
				_miss_shaders.emplace_back(ShaderEntry{ 
					.binding_set = binding_set, 
					.shader_identifier = export_name->shader_identifier 
				});
				_version++;
				return static_cast<int32_t>(_miss_shaders.size() - 1);
			}

			return -1;
		}

		int32_t ShaderTable::add_hit_group(const char* name, BindingSetInterface* binding_set)
		{
			DX12Pipeline* dx12_pipeline = check_cast<DX12Pipeline*>(_pipeline);

			const auto* export_name = dx12_pipeline->get_export(name);
			if (verify_export(export_name, binding_set))
			{
				_hit_groups.emplace_back(ShaderEntry{ 
					.binding_set = binding_set, 
					.shader_identifier = export_name->shader_identifier 
				});
				_version++;
				return static_cast<int32_t>(_hit_groups.size() - 1);
			}

			return -1;
		}

		int32_t ShaderTable::add_callable_shader(const char* name, BindingSetInterface* binding_set)
		{
			DX12Pipeline* dx12_pipeline = check_cast<DX12Pipeline*>(_pipeline);

			const auto* export_name = dx12_pipeline->get_export(name);
			if (verify_export(export_name, binding_set))
			{
				_callable_shaders.emplace_back(ShaderEntry{ 
					.binding_set = binding_set, 
					.shader_identifier = export_name->shader_identifier 
				});
				_version++;
				return static_cast<int32_t>(_callable_shaders.size() - 1);
			}

			return -1;
		}

		void ShaderTable::clear_miss_shaders()
		{
			_miss_shaders.clear();
			_version++;
		}

		void ShaderTable::clear_hit_groups()
		{
			_hit_groups.clear();
			_version++;
		}

		void ShaderTable::clear_callable_shaders()
		{
			_callable_shaders.clear();
			_version++;
		}

		bool ShaderTable::verify_export(const DX12ExportTableEntry* export_name, BindingSetInterface* binding_set) const
		{
			if (!export_name)
			{
				LOG_ERROR("Couldn't find a DXR PSO export with a given name");
				return false;
			}

			if (export_name->binding_layout && !binding_set)
			{
				LOG_ERROR("A shader table entry does not provide required local bindings");
				return false;
			}

			if (!export_name->binding_layout && binding_set)
			{
				LOG_ERROR("A shader table entry provides local bindings, but none are required");
				return false;
			}

			if (binding_set && (check_cast<DX12BindingSet*>(binding_set)->get_layout() != export_name->binding_layout))
			{
				LOG_ERROR("A shader table entry provides local bindings that do not match the expected layout");
				return false;
			}

			return true;
		}

		DX12AccelStruct::DX12AccelStruct(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps, const AccelStructDesc& desc) :
			_context(context), 
			_descriptor_heaps(descriptor_heaps), 
			_desc(desc)
		{
		}

		bool DX12AccelStruct::initialize()
		{
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO as_prebuild_info = get_accel_struct_prebuild_info();
			ReturnIfFalse(as_prebuild_info.ResultDataMaxSizeInBytes > 0);

			BufferDesc desc = BufferDesc::create_accel_struct(as_prebuild_info.ResultDataMaxSizeInBytes, _desc.is_top_level);
			desc.is_virtual = _desc.is_virtual;
			DX12Buffer* dx12_buffer = new DX12Buffer(_context, _descriptor_heaps, desc);
			if (!dx12_buffer->initialize())
			{
				delete dx12_buffer;
				return false;
			}

			_data_buffer = std::unique_ptr<BufferInterface>(dx12_buffer);
			return true;
		}
		
		MemoryRequirements DX12AccelStruct::get_memory_requirements()
		{
			return _data_buffer->get_memory_requirements();
		}

		bool DX12AccelStruct::bind_memory(HeapInterface* heap, uint64_t offset)
		{
			return _data_buffer->bind_memory(heap, offset);
		}

		bool DX12Pipeline::initialize(
			std::vector<std::unique_ptr<DX12RootSignature>>&& shader_root_signatures,
			std::vector<std::unique_ptr<DX12RootSignature>>&& dx12_hit_group_root_signatures,
			std::unique_ptr<DX12RootSignature>&& dx12_global_root_signature
		)
		{
			ReturnIfFalse(
				shader_root_signatures.size() == _desc.shader_descs.size() &&
				dx12_hit_group_root_signatures.size() == _desc.hit_group_descs.size()
			);

			struct Library
			{
				const void* blob;
				size_t size = 0;
				std::vector<std::wstring> export_names;
				std::vector<D3D12_EXPORT_DESC> d3d12_export_descs;
			};

			std::unordered_map<const void*, Library> dxil_libraries;
			for (uint32_t ix = 0; ix < _desc.shader_descs.size(); ++ix)
			{
				const auto& shader = _desc.shader_descs[ix];

				ShaderByteCode byte_code = shader.shader->get_byte_code();
				ReturnIfFalse(byte_code.is_valid());

				std::string export_name = shader.shader->get_desc().entry;

				auto& library = dxil_libraries[byte_code.byte_code];
				library.blob = byte_code.byte_code;
				library.size = byte_code.size;
				library.export_names.emplace_back(export_name.begin(), export_name.end());

				if (shader.binding_layout && shader_root_signatures[ix])
				{
					_local_binding_roots[shader.binding_layout] = std::move(shader_root_signatures[ix]);

					DX12BindingLayout* dx12_binding_layout = check_cast<DX12BindingLayout*>(shader.binding_layout);
					_max_local_root_parameter_count = std::max(_max_local_root_parameter_count, static_cast<uint32_t>(dx12_binding_layout->d3d12_root_parameters.size()));
				}
			}

			std::vector<std::wstring> hit_group_export_names_wstring;	// 防止 wstring 析构.
			std::unordered_map<const Shader*, std::wstring> hit_group_export_names;
			hit_group_export_names.reserve(_desc.hit_group_descs.size());

			std::vector<D3D12_HIT_GROUP_DESC> d3d12_hit_group_descs;
			for (uint32_t ix = 0; ix < _desc.hit_group_descs.size(); ++ix)
			{
				const auto& hit_group = _desc.hit_group_descs[ix];

				for (auto* shader : { hit_group.closest_hit_shader, hit_group.any_hit_shader, hit_group.intersect_shader })
				{
					if (!shader) continue;

					auto& export_name = hit_group_export_names[shader];
					if (export_name.empty())
					{
						ShaderByteCode byte_code = shader->get_byte_code();
						ReturnIfFalse(byte_code.is_valid());

						std::string strExportName = shader->get_desc().entry;

						auto& library = dxil_libraries[byte_code.byte_code];
						library.blob = byte_code.byte_code;
						library.size = byte_code.size;
						library.export_names.emplace_back(strExportName.begin(), strExportName.end());

						export_name = library.export_names.back();
					}
				}

				if (hit_group.binding_layout && dx12_hit_group_root_signatures[ix])
				{
					_local_binding_roots[hit_group.binding_layout] = std::move(dx12_hit_group_root_signatures[ix]);

					DX12BindingLayout* dx12_binding_layout = check_cast<DX12BindingLayout*>(hit_group.binding_layout);
					_max_local_root_parameter_count = std::max(_max_local_root_parameter_count, static_cast<uint32_t>(dx12_binding_layout->d3d12_root_parameters.size()));
				}

				D3D12_HIT_GROUP_DESC d3d12_hit_group_desc = d3d12_hit_group_descs.emplace_back();
				if (hit_group.closest_hit_shader) 
					d3d12_hit_group_desc.ClosestHitShaderImport = hit_group_export_names[hit_group.closest_hit_shader].c_str();
				if (hit_group.any_hit_shader) 
					d3d12_hit_group_desc.AnyHitShaderImport = hit_group_export_names[hit_group.any_hit_shader].c_str();
				if (hit_group.intersect_shader) 
					d3d12_hit_group_desc.IntersectionShaderImport = hit_group_export_names[hit_group.intersect_shader].c_str();

				d3d12_hit_group_desc.Type = hit_group.is_procedural_primitive ? D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE : D3D12_HIT_GROUP_TYPE_TRIANGLES;

				hit_group_export_names_wstring.emplace_back(hit_group.export_name.begin(), hit_group.export_name.end());
				d3d12_hit_group_desc.HitGroupExport = hit_group_export_names_wstring.back().c_str();
			}

			std::vector<D3D12_DXIL_LIBRARY_DESC> d3d12_dxil_libraries;
			d3d12_dxil_libraries.reserve(dxil_libraries.size());
			for (auto& [blob, library] : dxil_libraries)
			{
				for (const auto& export_name : library.export_names)
				{
					D3D12_EXPORT_DESC d3d12_export_desc = {};
					d3d12_export_desc.ExportToRename = export_name.c_str();
					d3d12_export_desc.Name = export_name.c_str();
					d3d12_export_desc.Flags = D3D12_EXPORT_FLAG_NONE;
					library.d3d12_export_descs.push_back(d3d12_export_desc);
				}

				D3D12_DXIL_LIBRARY_DESC d3d12_library_desc{};
				d3d12_library_desc.DXILLibrary.pShaderBytecode = library.blob;
				d3d12_library_desc.DXILLibrary.BytecodeLength = library.size;
				d3d12_library_desc.NumExports = static_cast<uint32_t>(library.d3d12_export_descs.size());
				d3d12_library_desc.pExports = library.d3d12_export_descs.data();

				d3d12_dxil_libraries.push_back(d3d12_library_desc);
			}

			std::vector<D3D12_STATE_SUBOBJECT> d3d12_state_subobjects;

			D3D12_RAYTRACING_SHADER_CONFIG d3d12_shader_config{};
			d3d12_shader_config.MaxAttributeSizeInBytes = _desc.max_attribute_size;
			d3d12_shader_config.MaxPayloadSizeInBytes = _desc.max_payload_size;
			d3d12_state_subobjects.emplace_back(
				D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,
				&d3d12_shader_config
			);

			D3D12_RAYTRACING_PIPELINE_CONFIG d3d12_pipeline_config{};
			d3d12_pipeline_config.MaxTraceRecursionDepth = _desc.max_recursion_depth;
			d3d12_state_subobjects.emplace_back(
				D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG,
				&d3d12_pipeline_config
			);

			for (const auto& dxil_library : d3d12_dxil_libraries)
			{
				d3d12_state_subobjects.emplace_back(
					D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,
					&dxil_library
				);
			}

			for (const auto& hit_group_desc : d3d12_hit_group_descs)
			{
				d3d12_state_subobjects.emplace_back(
					D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,
					&hit_group_desc
				);
			}


			D3D12_GLOBAL_ROOT_SIGNATURE d3d12_global_root_signature;
			if (!_desc.global_binding_layouts.empty() && dx12_global_root_signature)
			{
				_global_root_signature = std::move(dx12_global_root_signature);
				d3d12_global_root_signature.pGlobalRootSignature = _global_root_signature->d3d12_root_signature.Get();
				d3d12_state_subobjects.emplace_back(
					D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE,
					&d3d12_global_root_signature
				);
			}

			d3d12_state_subobjects.reserve(d3d12_state_subobjects.size() + _local_binding_roots.size() * 2);

			size_t assocaiation_num = _desc.shader_descs.size() + _desc.hit_group_descs.size();
			std::vector<std::wstring> associate_export_names_wstring;
			std::vector<LPCWSTR> associate_export_names_cwstr;
			associate_export_names_wstring.reserve(assocaiation_num);
			associate_export_names_cwstr.reserve(assocaiation_num);

			for (const auto& [binding_layout, dx12_root_signature] : _local_binding_roots)
			{
				auto pD3D12LocalRootSig = new_on_stack(D3D12_LOCAL_ROOT_SIGNATURE);
				pD3D12LocalRootSig->pLocalRootSignature = dx12_root_signature->d3d12_root_signature.Get();

				d3d12_state_subobjects.emplace_back(
					D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE,
					pD3D12LocalRootSig
				);

				auto pD3D12Association = new_on_stack(D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
				pD3D12Association->NumExports = 0;
				pD3D12Association->pSubobjectToAssociate = &d3d12_state_subobjects.back();

				size_t export_index_offset = associate_export_names_cwstr.size();

				for (const auto& shader_desc : _desc.shader_descs)
				{
					if (shader_desc.binding_layout == binding_layout)
					{
						std::string strExportName = shader_desc.shader->get_desc().entry;
						associate_export_names_wstring.emplace_back(strExportName.begin(), strExportName.end());
						associate_export_names_cwstr.emplace_back(associate_export_names_wstring.back().c_str());
						pD3D12Association->NumExports++;
					}
				}

				for (const auto& hit_group : _desc.hit_group_descs)
				{
					if (hit_group.binding_layout == binding_layout)
					{
						associate_export_names_wstring.emplace_back(hit_group.export_name.begin(), hit_group.export_name.end());
						associate_export_names_cwstr.emplace_back(associate_export_names_wstring.back().c_str());
						pD3D12Association->NumExports++;
					}
				}

				pD3D12Association->pExports = &associate_export_names_cwstr[export_index_offset];

				d3d12_state_subobjects.emplace_back(
					D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
					pD3D12Association
				);
			}

			D3D12_STATE_OBJECT_DESC PipelineDesc;
			PipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
			PipelineDesc.NumSubobjects = static_cast<uint32_t>(d3d12_state_subobjects.size());
			PipelineDesc.pSubobjects = d3d12_state_subobjects.data();

			if (
				FAILED(_context->device5->CreateStateObject(&PipelineDesc, IID_PPV_ARGS(_d3d12_state_object.GetAddressOf()))) ||
				FAILED(_d3d12_state_object->QueryInterface(IID_PPV_ARGS(_d3d12_state_object_properties.GetAddressOf())))
			)
			{
				LOG_ERROR("Failed to initialize ray tracing pipeline.");
				return false;
			}

			for (const auto& shader_desc : _desc.shader_descs)
			{
				std::string export_name = shader_desc.shader->get_desc().entry;
				std::wstring export_name_wstring(export_name.begin(), export_name.end());
				const void* shader_identifier = _d3d12_state_object_properties->GetShaderIdentifier(export_name_wstring.c_str());

				ReturnIfFalse(shader_identifier != nullptr);

				export_names[export_name] = DX12ExportTableEntry{ 
					.binding_layout = shader_desc.binding_layout, 
					.shader_identifier = shader_identifier
				};
			}

			for (const auto& crHitGroup : _desc.hit_group_descs)
			{
				std::wstring export_name_wstring(crHitGroup.export_name.begin(), crHitGroup.export_name.end());
				const void* shader_identifier = _d3d12_state_object_properties->GetShaderIdentifier(export_name_wstring.c_str());

				ReturnIfFalse(shader_identifier != nullptr);

				export_names[crHitGroup.export_name] = DX12ExportTableEntry{ 
					.binding_layout = crHitGroup.binding_layout, 
					.shader_identifier = shader_identifier
				};
			}

			return true;
		}


		ShaderTableInterface* DX12Pipeline::create_shader_table()
		{
			ShaderTable* shader_table = new ShaderTable(_context, this);
			if (!shader_table->initalize())
			{
				delete shader_table;
				return nullptr;
			}
			return shader_table;
		}

		
		const DX12ExportTableEntry* DX12Pipeline::get_export(const char* name)
		{
			auto iter = export_names.find(name);
			if (iter != export_names.end())
			{
				return &iter->second;
			}
			return nullptr;
		}

		uint32_t DX12Pipeline::get_shaderTableEntrySize() const
		{
			uint32_t required_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(uint64_t) * _max_local_root_parameter_count;
			return Align(required_size, uint32_t(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
		}
	}


		
}


#endif
