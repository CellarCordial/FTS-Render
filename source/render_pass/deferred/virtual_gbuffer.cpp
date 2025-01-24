#include "virtual_gbuffer.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/virtual_mesh.h"
#include "../../scene/virtual_texture.h"
#include <cstdint>
#include <memory>

namespace fantasy
{
	bool VirtualGBufferPass::compile(DeviceInterface* device, RenderResourceCache* cache)
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
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::VirtualGBufferPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_structured_buffer_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_structured_buffer_srv(3);
			binding_layout_items[5] = BindingLayoutItem::create_structured_buffer_srv(4);
			binding_layout_items[6] = BindingLayoutItem::create_structured_buffer_uav(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "deferred/virtual_gbuffer_vs.slang";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = shader_compile::compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = "deferred/virtual_gbuffer_ps.slang";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Pixel;
			ShaderData ps_data = shader_compile::compile_shader(shader_compile_desc);

			ShaderDesc vs_desc;
			vs_desc.entry = "main";
			vs_desc.shader_type = ShaderType::Vertex;
			ReturnIfFalse(_vs = std::unique_ptr<Shader>(create_shader(vs_desc, vs_data.data(), vs_data.size())));

			ShaderDesc ps_desc;
			ps_desc.shader_type = ShaderType::Pixel;
			ps_desc.entry = "main";
			ReturnIfFalse(_ps = std::unique_ptr<Shader>(create_shader(ps_desc, ps_data.data(), ps_data.size())));
		}

		// Buffer
		{
			ReturnIfFalse(_vt_page_info_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_rwstructured(
					sizeof(VTPageInfo) * CLIENT_WIDTH * CLIENT_HEIGHT, 
					sizeof(VTPageInfo),
					"vt_page_info_buffer"
				)
			)));
			cache->collect(_vt_page_info_buffer, ResourceType::Buffer);
		}

		// Texture.
		{
			ReturnIfFalse(_world_position_view_depth_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA32_FLOAT,
					"world_position_view_depth_texture"
				)
			)));
			cache->collect(_world_position_view_depth_texture, ResourceType::Texture);

			ReturnIfFalse(_view_space_velocity_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGB32_FLOAT,
					"view_space_velocity_texture"
				)
			)));
			cache->collect(_view_space_velocity_texture, ResourceType::Texture);

			ReturnIfFalse(_tile_uv_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RG32_FLOAT,
					"tile_uv_texture"
				)
			)));
			cache->collect(_tile_uv_texture, ResourceType::Texture);

			ReturnIfFalse(_world_space_normal_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGB32_FLOAT,
					"world_space_normal_texture"
				)
			)));
			cache->collect(_world_space_normal_texture, ResourceType::Texture);

			ReturnIfFalse(_world_space_tangent_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGB32_FLOAT,
					"world_space_tangent_texture"
				)
			)));
			cache->collect(_world_space_tangent_texture, ResourceType::Texture);

			ReturnIfFalse(_base_color_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA16_FLOAT,
					"base_color_texture"
				)
			)));
			cache->collect(_base_color_texture, ResourceType::Texture);

			ReturnIfFalse(_pbr_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA8_UNORM,
					"pbr_texture"
				)
			)));
			cache->collect(_pbr_texture, ResourceType::Texture);

			ReturnIfFalse(_emmisive_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA16_FLOAT,
					"emmisive_texture"
				)
			)));
			cache->collect(_emmisive_texture, ResourceType::Texture);
		}

		// Frame Buffer.
		{
			FrameBufferDesc frame_buffer_desc;
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_world_position_view_depth_texture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_view_space_velocity_texture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_tile_uv_texture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_world_space_normal_texture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_world_space_tangent_texture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_base_color_texture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_pbr_texture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_emmisive_texture));
			ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(frame_buffer_desc)));
		}
 
		// Pipeline.
		{
			GraphicsPipelineDesc pipeline_desc;
			pipeline_desc.vertex_shader = _vs.get();
			pipeline_desc.pixel_shader = _ps.get();
			pipeline_desc.input_layout = _input_layout.get();
			pipeline_desc.binding_layouts.push_back(_binding_layout.get());
			ReturnIfFalse(_pipeline = std::unique_ptr<GraphicsPipelineInterface>(
				device->create_graphics_pipeline(pipeline_desc, _frame_buffer.get())
			));
		}

		// Binding Set.
		{
			_binding_set_items.resize(7);
			_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::VirtualGBufferPassConstant));
			_binding_set_items[6] = BindingSetItem::create_structured_buffer_uav(0, _vt_page_info_buffer);
		}

		// Graphics state.
		{
			_graphics_state.pipeline = _pipeline.get();
			_graphics_state.frame_buffer = _frame_buffer.get();
			_graphics_state.binding_sets.resize(1);
			_graphics_state.viewport_state = ViewportState::create_default_viewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

		return true;
	}

	bool VirtualGBufferPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		if (!_resource_writed)
		{
			bool res = cache->get_world()->each<VirtualMesh, Mesh, Material>(
                [&](Entity* entity, VirtualMesh* virtual_mesh, Mesh* mesh, Material* material) -> bool
                {
                    for (const auto& submesh : virtual_mesh->_submeshes)
                    {
                        for (const auto& group : submesh.cluster_groups)
                        {
                            for (auto ix : group.cluster_indices)
                            {
                                const auto& cluster = submesh.clusters[ix];
								_cluster_vertices.insert(_cluster_vertices.end(), cluster.vertices.begin(), cluster.vertices.end());

								for(uint32_t ix = 0; ix < cluster.indices.size() / 3; ++ix)
								{
									uint32_t i0 = cluster.indices[ix * 3];
									uint32_t i1 = cluster.indices[ix * 3 + 1];
									uint32_t i2 = cluster.indices[ix * 3 + 2];

									uint32_t max_index = sizeof(uint8_t);
									ReturnIfFalse(i0 < max_index && i1 < max_index && i2 < max_index);

									_cluster_triangles.push_back(uint32_t(i0 | (i1 << 8) | (i2 << 16)));
								}
                            }
                        }
                    }  
					for (const auto& submesh : mesh->submeshes)
					{
						const auto& submaterial = material->submaterials[submesh.material_index];
						_geometry_constants.emplace_back(
							GeometryConstantGpu{
								.world_matrix = submesh.world_matrix,
								.inv_trans_world = inverse(transpose(submesh.world_matrix)),
								.base_color = submaterial.base_color_factor,
								.emissive = submaterial.emissive_factor,
								.roughness = submaterial.roughness_factor,
								.metallic = submaterial.metallic_factor,
								.occlusion = submaterial.occlusion_factor,
								.texture_resolution = uint2(submaterial.images[0].width, submaterial.images[0].height)
							}
						);
					}
                    return true;
                }
            );
			ReturnIfFalse(res);

			DeviceInterface* device = cmdlist->get_deivce();

			ReturnIfFalse(_cluster_vertex_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
					BufferDesc::create_structured(
						sizeof(Vertex) * _cluster_vertices.size(), 
						sizeof(Vertex),
						false,
						"cluster_vertex_buffer"
					)
				)
			));
			ReturnIfFalse(_cluster_triangle_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
					BufferDesc::create_structured(
						sizeof(uint32_t) * _cluster_triangles.size(), 
						sizeof(uint32_t),
						false,
						"cluster_triangle_buffer"
					)
				)
			));
			ReturnIfFalse(_geometry_constant_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
					BufferDesc::create_structured(
						sizeof(GeometryConstantGpu) * _geometry_constants.size(), 
						sizeof(GeometryConstantGpu),
						false,
						"geometry_constant_buffer"
					)
				)
			));

			ReturnIfFalse(cmdlist->write_buffer(_cluster_vertex_buffer.get(), _cluster_vertices.data(), sizeof(Vertex) * _cluster_vertices.size()));
			ReturnIfFalse(cmdlist->write_buffer(_cluster_triangle_buffer.get(), _cluster_triangles.data(), sizeof(Vertex) * _cluster_vertices.size()));
			ReturnIfFalse(cmdlist->write_buffer(_geometry_constant_buffer.get(), _geometry_constants.data(), sizeof(Vertex) * _cluster_vertices.size()));

			_binding_set.reset();
			_binding_set_items[1] = BindingSetItem::create_structured_buffer_srv(0, _geometry_constant_buffer);
			_binding_set_items[2] = BindingSetItem::create_structured_buffer_srv(1, check_cast<BufferInterface>(cache->require("visible_cluster_id_buffer")));
			_binding_set_items[3] = BindingSetItem::create_structured_buffer_srv(2, check_cast<BufferInterface>(cache->require("cluster_buffer")));
			_binding_set_items[4] = BindingSetItem::create_structured_buffer_srv(3, _cluster_vertex_buffer);
			_binding_set_items[5] = BindingSetItem::create_structured_buffer_srv(4, _cluster_triangle_buffer);
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = _binding_set_items },
				_binding_layout.get()
			)));

			_graphics_state.binding_sets[0] = _binding_set.get();
			_graphics_state.indirect_buffer = check_cast<BufferInterface>(cache->require("draw_indexed_indirect_arguments_buffer")).get();

			_resource_writed = true;
		}

		ReturnIfFalse(cmdlist->set_graphics_state(_graphics_state));
		ReturnIfFalse(cmdlist->set_push_constants(&_pass_constant, sizeof(constant::VirtualGBufferPassConstant)));
		ReturnIfFalse(cmdlist->draw_indirect());

		ReturnIfFalse(cmdlist->close());
		return true;
	}

	bool VirtualGBufferPass::finish_pass()
	{
		_cluster_vertices.clear(); _cluster_vertices.shrink_to_fit();
		_cluster_triangles.clear(); _cluster_triangles.shrink_to_fit();
		_geometry_constants.clear(); _geometry_constants.shrink_to_fit();
		return true;
	}

}

