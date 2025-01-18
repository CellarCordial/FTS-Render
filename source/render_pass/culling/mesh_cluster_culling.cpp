
#include "mesh_cluster_culling.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/camera.h"
#include <cstdint>


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16u
#define THREAD_GROUP_SIZE_Y 16u
 
	bool MeshClusterCullingPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
        cache->get_world()->get_global_entity()->get_component<event::ModelLoaded>()->add_event(
            [this]() -> bool
            {
                _resource_writed = false;
				return true;
            }
        );
        
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(7);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::MeshClusterCullingPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_uav(0);
			binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_uav(1);
			binding_layout_items[3] = BindingLayoutItem::create_structured_buffer_srv(0);
			binding_layout_items[4] = BindingLayoutItem::create_structured_buffer_srv(1);
			binding_layout_items[5] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[6] = BindingLayoutItem::create_sampler(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "culling/mesh_cluster_culling_cs.slang";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			ShaderData cs_data = shader_compile::compile_shader(cs_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.entry = "main";
			cs_desc.shader_type = ShaderType::Compute;
			ReturnIfFalse(_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));
		}

		// Pipeline.
		{
			ComputePipelineDesc pipeline_desc;
			pipeline_desc.compute_shader = _cs.get();
			pipeline_desc.binding_layouts.push_back(_binding_layout.get());
			ReturnIfFalse(_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));
		}

		// Buffer.
		{
			ReturnIfFalse(_virtual_gbuffer_indirect_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_structured(
					sizeof(DrawIndexedIndirectArguments), 
					sizeof(DrawIndexedIndirectArguments),
                    true, 
					"virtual_gbuffer_indirect_buffer"
				)
			)));
            cache->collect(_virtual_gbuffer_indirect_buffer, ResourceType::Buffer);
		}

        uint32_t hzb_mip_levels = search_most_significant_bit(_hzb_resolution) + 1;
		// Texture.
		{
            TextureDesc desc = TextureDesc::create_shader_resource(
                _hzb_resolution,
                _hzb_resolution,
                Format::R32_FLOAT,
                "hierarchical_zbuffer_texture"
            );
            desc.mip_levels = hzb_mip_levels;
			ReturnIfFalse(
                _hierarchical_zbuffer_texture = std::shared_ptr<TextureInterface>(device->create_texture(desc))
            );
			cache->collect(_hierarchical_zbuffer_texture, ResourceType::Texture);
		}
 
		// Binding Set.
		{
			_binding_set_items.resize(7);
			_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::MeshClusterCullingPassConstant));
			_binding_set_items[1] = BindingSetItem::create_structured_buffer_uav(0, _virtual_gbuffer_indirect_buffer);

			_binding_set_items[5] = BindingSetItem::create_texture_srv(
                2, 
                _hierarchical_zbuffer_texture,
                TextureSubresourceSet{ 
                    .base_mip_level = hzb_mip_levels - 1,
                    .mip_level_count = 1,
                    .base_array_slice = 0,
                    .array_slice_count = 1
                }
            );
			_binding_set_items[6] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));

		}

		// Compute state.
		{
			_compute_state.binding_sets.resize(1);
			_compute_state.pipeline = _pipeline.get();
		}

        _pass_constant.hzb_resolution = _hzb_resolution;
        ReturnIfFalse(cache->collect_constants("hzb_resolution", &_hzb_resolution));
 
		return true;
	}

	bool MeshClusterCullingPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

        World* world = cache->get_world();

        Camera* camera = world->get_global_entity()->get_component<Camera>();
        _pass_constant.camera_fov_y = camera->get_fov_y();
        _pass_constant.view_matrix = camera->view_matrix;
        _pass_constant.proj_matrix = camera->proj_matrix;

        if (!_resource_writed)
        {
            DeviceInterface* device = cmdlist->get_deivce();
            _pass_constant.group_count = 0;

            uint32_t vertex_offset = 0;
            uint32_t triangle_offset = 0;
            uint32_t cluster_index_offset = 0;

            ReturnIfFalse(world->each<VirtualMesh>(
                [&](Entity* entity, VirtualMesh* virtual_mesh) -> bool
                {
                    for (const auto& submesh : virtual_mesh->_submeshes)
                    {
                        _pass_constant.group_count += static_cast<uint32_t>(submesh.cluster_groups.size());

                        for (const auto& group : submesh.cluster_groups)
                        {
                            _mesh_cluster_groups.emplace_back(convert_mesh_cluster_group(group, cluster_index_offset));

                            for (auto ix : group.cluster_indices)
                            {
                                const auto& cluster = submesh.clusters[ix];
                                _mesh_clusters.emplace_back(convert_mesh_cluster(cluster, vertex_offset, triangle_offset));

                                vertex_offset += static_cast<uint32_t>(cluster.vertices.size());
                                triangle_offset += static_cast<uint32_t>(cluster.indices.size() / 3);
                            }
                            
                            cluster_index_offset += static_cast<uint32_t>(group.cluster_indices.size());
                        }

                    }  
                    return true;
                }
            ));
            
            ReturnIfFalse(_mesh_cluster_group_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
                BufferDesc::create_structured(
                    sizeof(MeshClusterGroupGpu) * _mesh_cluster_groups.size(), 
                    sizeof(MeshClusterGroupGpu),
                    true,
                    "mesh_cluster_group_buffer"
                )
            )));
            
            ReturnIfFalse(_mesh_cluster_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
                BufferDesc::create_structured(
                    sizeof(MeshClusterGpu) * _mesh_clusters.size(), 
                    sizeof(MeshClusterGpu),
                    true,
                    "mesh_cluster_buffer"
                )
            )));
            cache->collect(_mesh_cluster_buffer, ResourceType::Buffer);

            ReturnIfFalse(_visible_cluster_id_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
                BufferDesc::create_structured(
                    sizeof(uint32_t) * cluster_index_offset, 
                    sizeof(uint32_t),
                    true,
                    "visible_cluster_id_buffer"
                )
            )));
            cache->collect(_visible_cluster_id_buffer, ResourceType::Buffer);

            ReturnIfFalse(cmdlist->write_buffer(
                _mesh_cluster_group_buffer.get(), 
                _mesh_cluster_groups.data(), 
                sizeof(MeshClusterGroupGpu) * _mesh_cluster_groups.size()
            ));
            ReturnIfFalse(cmdlist->write_buffer(
                _mesh_cluster_buffer.get(), 
                _mesh_clusters.data(), 
                sizeof(MeshClusterGpu) * _mesh_clusters.size()
            ));

            _binding_set.reset();

            _binding_set_items[2] = BindingSetItem::create_structured_buffer_uav(1, _visible_cluster_id_buffer);
			_binding_set_items[3] = BindingSetItem::create_structured_buffer_srv(0, _mesh_cluster_group_buffer);
			_binding_set_items[4] = BindingSetItem::create_structured_buffer_srv(1, _mesh_cluster_buffer);
            ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
                BindingSetDesc{ .binding_items = _binding_set_items },
                _binding_layout.get()
            )));
            _compute_state.binding_sets[0] = _binding_set.get();

            _resource_writed = true;
        }

		uint32_t thread_group_num = 
            static_cast<uint32_t>((align(_pass_constant.group_count, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X));

		ReturnIfFalse(cmdlist->set_compute_state(_compute_state));
        ReturnIfFalse(cmdlist->set_push_constants(&_pass_constant, sizeof(constant::MeshClusterCullingPassConstant)));
		ReturnIfFalse(cmdlist->dispatch(thread_group_num));

		ReturnIfFalse(cmdlist->close());

        return true;
	}

    bool MeshClusterCullingPass::finish_pass()
    {
        _mesh_cluster_groups.clear(); _mesh_cluster_groups.shrink_to_fit();
        _mesh_clusters.clear(); _mesh_clusters.shrink_to_fit();
        return true;
    }

}