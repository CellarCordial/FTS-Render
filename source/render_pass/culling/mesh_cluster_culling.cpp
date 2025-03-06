
#include "mesh_cluster_culling.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/camera.h"
#include "../../scene/scene.h"
#include <cstdint>


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16u
#define THREAD_GROUP_SIZE_Y 16u
 
	bool MeshClusterCullingPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
        cache->get_world()->get_global_entity()->get_component<event::AddModel>()->add_event(
            [this]() -> bool
            {
                _mesh_update = true;
				return true;
            }
        );

		ReturnIfFalse(cache->collect_constants("cluster_group_count", &_cluster_group_count));
        
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(8);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::MeshClusterCullingPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_uav(0);
			binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_uav(1);
			binding_layout_items[3] = BindingLayoutItem::create_structured_buffer_srv(0);
			binding_layout_items[4] = BindingLayoutItem::create_structured_buffer_srv(1);
			binding_layout_items[5] = BindingLayoutItem::create_structured_buffer_srv(2);
			binding_layout_items[6] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[7] = BindingLayoutItem::create_sampler(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "culling/mesh_cluster_culling_cs.hlsl";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));

#if NANITE
			cs_compile_desc.defines.push_back("NANITE=1");
#endif

			ShaderData cs_data = compile_shader(cs_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.entry = "main";
			cs_desc.shader_type = ShaderType::Compute;
			ReturnIfFalse(_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));
		}

		// Pipeline.
		{
			ComputePipelineDesc pipeline_desc;
			pipeline_desc.compute_shader = _cs;
			pipeline_desc.binding_layouts.push_back(_binding_layout);
			ReturnIfFalse(_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));
		}

		// Buffer.
		{
			ReturnIfFalse(_virtual_gbuffer_draw_indirect_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_read_write_structured_buffer(
					sizeof(DrawIndexedIndirectArguments), 
					sizeof(DrawIndexedIndirectArguments),
					"virtual_gbuffer_draw_indirect_buffer"
				)
			)));
            cache->collect(_virtual_gbuffer_draw_indirect_buffer, ResourceType::Buffer);
		}

        uint32_t hzb_mip_levels = search_most_significant_bit(_hzb_resolution) + 1;

		// Texture.
		{
            TextureDesc desc;
            desc.name = "hierarchical_zbuffer_texture";
            desc.width = _hzb_resolution;
            desc.height = _hzb_resolution;
            desc.format = Format::R32_FLOAT;
            desc.mip_levels = hzb_mip_levels;
            desc.allow_unordered_access = true;
			ReturnIfFalse(_hierarchical_zbuffer_texture = std::shared_ptr<TextureInterface>(device->create_texture(desc)));
			cache->collect(_hierarchical_zbuffer_texture, ResourceType::Texture);
		}
 
		// Binding Set.
		{
			_binding_set_items.resize(8);
			_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::MeshClusterCullingPassConstant));
			_binding_set_items[1] = BindingSetItem::create_structured_buffer_uav(0, _virtual_gbuffer_draw_indirect_buffer);

			_binding_set_items[6] = BindingSetItem::create_texture_srv(
                3, 
                _hierarchical_zbuffer_texture,
                TextureSubresourceSet{ 
                    .base_mip_level = 0,
                    .mip_level_count = hzb_mip_levels,
                    .base_array_slice = 0,
                    .array_slice_count = 1
                }
            );
			_binding_set_items[7] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));

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

        if (SceneSystem::loaded_submesh_count != 0)
        {
            World* world = cache->get_world();
            
            Camera* camera = world->get_global_entity()->get_component<Camera>();
            _pass_constant.camera_fov_y = camera->get_fov_y();
            _pass_constant.view_matrix = camera->view_matrix;
            _pass_constant.reverse_z_proj_matrix = camera->reverse_z_proj_matrix;
            _pass_constant.near_plane = camera->get_near_z();
            _pass_constant.far_plane = camera->get_far_z();

            if (_mesh_update)
            {
                DeviceInterface* device = cmdlist->get_deivce();
                _pass_constant.group_count = 0;
    
                uint32_t cluster_count = 0;
                uint32_t vertex_offset = 0;
                uint32_t triangle_offset = 0;
    
                bool res = world->each<VirtualMesh, Mesh, Material>(
                    [&](Entity* entity, VirtualMesh* virtual_mesh, Mesh* mesh, Material* material) -> bool
                    {
                        for (const auto& submesh : virtual_mesh->_submeshes)
                        {
#if NANITE
                            _pass_constant.group_count += static_cast<uint32_t>(submesh.cluster_groups.size());
                            for (const auto& group : submesh.cluster_groups)
                            {
                                _mesh_cluster_groups.emplace_back(convert_mesh_cluster_group(group, cluster_count, submesh.mip_levels - 1));

                                for (auto ix : group.cluster_indices)
                                {
                                    const auto& cluster = submesh.clusters[ix];
                                    _mesh_clusters.emplace_back(convert_mesh_cluster(cluster, vertex_offset, triangle_offset));

                                    vertex_offset += static_cast<uint32_t>(cluster.vertices.size());
                                    triangle_offset += static_cast<uint32_t>(cluster.indices.size() / 3);
                                }
                                
                                cluster_count += static_cast<uint32_t>(group.cluster_indices.size());
                            }
#else
                            _pass_constant.group_count++;
                            _mesh_cluster_groups.emplace_back(convert_mesh_cluster_group(submesh.cluster_groups[0], cluster_count, 0));
                            for (const auto& cluster : submesh.clusters)
                            {
                                _mesh_clusters.emplace_back(convert_mesh_cluster(cluster, vertex_offset, triangle_offset));
                                vertex_offset += static_cast<uint32_t>(cluster.vertices.size());
                                triangle_offset += MeshCluster::cluster_tirangle_num;
                            }
                            cluster_count += submesh.cluster_groups[0].cluster_count;
#endif
                        }  
                        
						for (const auto& submesh : mesh->submeshes)
						{
							const auto& submaterial = material->submaterials[submesh.material_index];
							_geometry_constants.emplace_back(
								GeometryConstantGpu{
									.world_matrix = submesh.world_matrix,
									.inv_trans_world = inverse(transpose(submesh.world_matrix)),
									.base_color = float4(submaterial.base_color_factor),
									.occlusion = submaterial.occlusion_factor,
									.roughness = submaterial.roughness_factor,
									.metallic = submaterial.metallic_factor,
									.emissive = float4(submaterial.emissive_factor),
									.texture_resolution = material->image_resolution
								}
							);
						}
                        return true;
                    }
                );
                ReturnIfFalse(res);

                _cluster_group_count = static_cast<uint32_t>(_mesh_cluster_groups.size());
                
                ReturnIfFalse(_mesh_cluster_group_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
                    BufferDesc::create_structured_buffer(
                        sizeof(MeshClusterGroupGpu) * _mesh_cluster_groups.size(), 
                        sizeof(MeshClusterGroupGpu),
                        "mesh_cluster_group_buffer"
                    )
                )));
                cache->collect(_mesh_cluster_group_buffer, ResourceType::Buffer);
                
                ReturnIfFalse(_mesh_cluster_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
                    BufferDesc::create_structured_buffer(
                        sizeof(MeshClusterGpu) * _mesh_clusters.size(), 
                        sizeof(MeshClusterGpu),
                        "mesh_cluster_buffer"
                    )
                )));
                cache->collect(_mesh_cluster_buffer, ResourceType::Buffer);
    
                ReturnIfFalse(_visible_cluster_id_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
                    BufferDesc::create_read_write_structured_buffer(
                        sizeof(uint32_t) * cluster_count, 
                        sizeof(uint32_t),
                        "visible_cluster_id_buffer"
                    )
                )));
                cache->collect(_visible_cluster_id_buffer, ResourceType::Buffer);
				
				ReturnIfFalse(_geometry_constant_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
						BufferDesc::create_structured_buffer(
							sizeof(GeometryConstantGpu) * _geometry_constants.size(), 
							sizeof(GeometryConstantGpu),
							"geometry_constant_buffer"
						)
					)
				));
				cache->collect(_geometry_constant_buffer, ResourceType::Buffer);
                
    
				ReturnIfFalse(cmdlist->write_buffer(
                    _geometry_constant_buffer.get(), 
                    _geometry_constants.data(), 
                    sizeof(GeometryConstantGpu) * 
                    _geometry_constants.size()
                ));
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
                _binding_set_items[5] = BindingSetItem::create_structured_buffer_srv(2, _geometry_constant_buffer);
                ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
                    BindingSetDesc{ .binding_items = _binding_set_items },
                    _binding_layout
                )));
                _compute_state.binding_sets[0] = _binding_set.get();
    
                _mesh_update = false;
            }

            cmdlist->clear_buffer_uint(
                _virtual_gbuffer_draw_indirect_buffer.get(), 
                BufferRange(0, sizeof(DrawIndexedIndirectArguments)), 
                0
            );
    
            uint32_t thread_group_num = 
                static_cast<uint32_t>((align(_pass_constant.group_count, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X));
    
            ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num, 1, 1, &_pass_constant));
        }
        
		ReturnIfFalse(cmdlist->close());

        return true;
	}

    bool MeshClusterCullingPass::finish_pass(RenderResourceCache* cache)
    {
        _mesh_cluster_groups.clear(); _mesh_cluster_groups.shrink_to_fit();
        _mesh_clusters.clear(); _mesh_clusters.shrink_to_fit();
        return true;
    }
}