
#include "gbuffer.h"
#include "../../shader/shader_compiler.h"
#include "../../scene/camera.h"
#include "../../scene/scene.h"
#include "../../core/tools/check_cast.h"
#include <cstdint>
#include <memory>

namespace fantasy
{
	bool GBufferPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
        cache->get_world()->each<event::UpdateGBuffer>(
            [this](Entity* entity, event::UpdateGBuffer* event) -> bool 
            {
                _update_gbuffer = true;
                return true;
            } 
        );

		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(8);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::GBufferPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_push_constants(0, sizeof(constant::GeometryConstant));
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[5] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[6] = BindingLayoutItem::create_texture_srv(4);
			binding_layout_items[7] = BindingLayoutItem::create_sampler(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Input Layout.
		{
			VertexAttributeDescArray vertex_attribute_desc(4);
			vertex_attribute_desc[0].name = "POSITION";
			vertex_attribute_desc[0].format = Format::RGB32_FLOAT;
			vertex_attribute_desc[0].offset = offsetof(Vertex, position);
			vertex_attribute_desc[0].element_stride = sizeof(Vertex);
			vertex_attribute_desc[1].name = "NORMAL";
			vertex_attribute_desc[1].format = Format::RGB32_FLOAT;
			vertex_attribute_desc[1].offset = offsetof(Vertex, normal);
			vertex_attribute_desc[1].element_stride = sizeof(Vertex);
			vertex_attribute_desc[2].name = "TANGENT";
			vertex_attribute_desc[2].format = Format::RGB32_FLOAT;
			vertex_attribute_desc[2].offset = offsetof(Vertex, tangent);
			vertex_attribute_desc[2].element_stride = sizeof(Vertex);
			vertex_attribute_desc[3].name = "TEXCOORD";
			vertex_attribute_desc[3].format = Format::RG32_FLOAT;
			vertex_attribute_desc[3].offset = offsetof(Vertex, uv);
			vertex_attribute_desc[3].element_stride = sizeof(Vertex);
			ReturnIfFalse(_input_layout = std::unique_ptr<InputLayoutInterface>(device->create_input_layout(
				vertex_attribute_desc.data(),
				vertex_attribute_desc.size(),
				nullptr
			)));
		}


		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "deferred/gbuffer_vs.slang";
			shader_compile_desc.entry_point = "vertex_shader";
			shader_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = shader_compile::compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = "deferred/gbuffer_ps.slang";
			shader_compile_desc.entry_point = "pixel_shader";
			shader_compile_desc.target = ShaderTarget::Pixel;
			ShaderData ps_data = shader_compile::compile_shader(shader_compile_desc);

			ShaderDesc vs_desc;
			vs_desc.entry = "vertex_shader";
			vs_desc.shader_type = ShaderType::Vertex;
			ReturnIfFalse(_vs = std::unique_ptr<Shader>(create_shader(vs_desc, vs_data.data(), vs_data.size())));

			ShaderDesc ps_desc;
			ps_desc.shader_type = ShaderType::Pixel;
			ps_desc.entry = "pixel_shader";
			ReturnIfFalse(_ps = std::unique_ptr<Shader>(create_shader(ps_desc, ps_data.data(), ps_data.size())));
		}





        // Texture
        {
            ReturnIfFalse(_world_space_position_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA32_FLOAT,
					"world_space_position_depth_texture"
				)
			)));
            ReturnIfFalse(_world_space_normal_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGB32_FLOAT,
					"normal_texture"
				)
			)));
            ReturnIfFalse(_base_color_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA32_FLOAT,
					"base_color_texture"
				)
			)));
            ReturnIfFalse(_pbr_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA32_FLOAT,
					"pbr_texture"
				)
			)));
            ReturnIfFalse(_emissive_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA32_FLOAT,
					"emissive_texture"
				)
			)));
            ReturnIfFalse(_view_space_velocity_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGB32_FLOAT,
					"view_space_velocity_texture"
				)
			)));
            ReturnIfFalse(_depth_texture = std::shared_ptr<TextureInterface>(device->create_texture(
                TextureDesc::create_depth(
                    CLIENT_WIDTH, 
                    CLIENT_HEIGHT, 
                    Format::R32_FLOAT,
                    "depth_texture"
                )
            )))
            
			cache->collect(_world_space_position_texture, ResourceType::Texture);
			cache->collect(_world_space_normal_texture, ResourceType::Texture);
			cache->collect(_base_color_texture, ResourceType::Texture);
			cache->collect(_pbr_texture, ResourceType::Texture);
			cache->collect(_emissive_texture, ResourceType::Texture);
			cache->collect(_view_space_velocity_texture, ResourceType::Texture);
			cache->collect(_depth_texture, ResourceType::Texture);
        }  
 
		// Frame Buffer.
		{
			FrameBufferDesc frame_buffer_desc;
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_world_space_position_texture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_world_space_normal_texture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_base_color_texture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_pbr_texture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_emissive_texture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_view_space_velocity_texture));
			frame_buffer_desc.depth_stencil_attachment = FrameBufferAttachment::create_attachment(_depth_texture);
			ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(frame_buffer_desc)));
		}
 
		// Pipeline.
		{
			GraphicsPipelineDesc pipeline_desc;
			pipeline_desc.vertex_shader = _vs.get();
			pipeline_desc.pixel_shader = _ps.get();
			pipeline_desc.input_layout = _input_layout.get();
			pipeline_desc.binding_layouts.push_back(_binding_layout.get());
			pipeline_desc.render_state.depth_stencil_state.enable_depth_test = true;
			pipeline_desc.render_state.depth_stencil_state.enable_depth_write = false;
			pipeline_desc.render_state.depth_stencil_state.depth_func = ComparisonFunc::LessOrEqual;
			ReturnIfFalse(_pipeline = std::unique_ptr<GraphicsPipelineInterface>(
				device->create_graphics_pipeline(pipeline_desc, _frame_buffer.get())
			));
		}
		return true;
	}

	bool GBufferPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

        ReturnIfFalse(update(cmdlist, cache));

        _graphics_state.pipeline = _pipeline.get();
        _graphics_state.frame_buffer = _frame_buffer.get();
        _graphics_state.index_buffer_binding = IndexBufferBinding{ .buffer = _index_buffer };
        _graphics_state.vertex_buffer_bindings.push_back(VertexBufferBinding{ .buffer = _vertex_buffer });
        _graphics_state.viewport_state = ViewportState::create_default_viewport(CLIENT_WIDTH, CLIENT_HEIGHT);

        for (uint32_t ix = 0; ix < _draw_arguments.size(); ++ix)
        {
			_graphics_state.binding_sets.push_back(_binding_sets[ix].get());

            ReturnIfFalse(cmdlist->set_graphics_state(_graphics_state));
            ReturnIfFalse(cmdlist->set_push_constants(&_pass_constant, sizeof(constant::GBufferPassConstant)));
            ReturnIfFalse(cmdlist->set_push_constants(&_geometry_constants[ix], sizeof(constant::GeometryConstant)));
            ReturnIfFalse(cmdlist->draw_indexed(_draw_arguments[ix]));
        }

		ReturnIfFalse(cmdlist->close());
		return true;
	}

    bool GBufferPass::finish_pass()
    {
        _indices.clear(); _indices.shrink_to_fit();
        _vertices.clear(); _vertices.shrink_to_fit();
        _geometry_constants.clear(); _geometry_constants.shrink_to_fit();
        return true;
    }


    bool GBufferPass::update(CommandListInterface* cmdlist, RenderResourceCache* cache)
    {
        DeviceInterface* device = cmdlist->get_deivce();
        World* world = cache->get_world();

        ReturnIfFalse(world->each<Camera>(
            [this](Entity* entity, Camera* camera) -> bool
            {
                _pass_constant.view_matrix = camera->view_matrix;
                _pass_constant.view_proj = camera->get_view_proj();
                _pass_constant.prev_view_matrix = camera->prev_view_matrix;
                return true;
            }
        ))

        if (_update_gbuffer)
        {
            _binding_sets.clear();
            _index_buffer.reset();
            _vertex_buffer.reset();
            _draw_arguments.clear();

            bool res = world->each<Mesh, Material>(
                [&](Entity* entity, Mesh* mesh, Material* material) -> bool
                {
                    uint64_t old_size = _draw_arguments.size();
                    uint64_t new_size = old_size + mesh->submeshes.size();

                    _binding_sets.resize(new_size);
					_draw_arguments.resize(new_size);
                    _geometry_constants.resize(new_size);

					_draw_arguments[old_size].index_count = static_cast<uint32_t>(mesh->submeshes[0].indices.size());
					if (old_size > 0)
					{
						_draw_arguments[old_size].start_index_location = _indices.size();
						_draw_arguments[old_size].start_vertex_location = _vertices.size();
					}

					for (uint64_t ix = old_size; ix < _draw_arguments.size(); ++ix)
					{
						const auto& submesh = mesh->submeshes[ix];
						_vertices.insert(_vertices.end(), submesh.vertices.begin(), submesh.vertices.end());
						_indices.insert(_indices.end(), submesh.indices.begin(), submesh.indices.end());

						if (ix != 0)
						{
							_draw_arguments[ix].index_count = submesh.indices.size();
							_draw_arguments[ix].start_index_location = _draw_arguments[ix - 1].start_index_location + mesh->submeshes[ix - 1].indices.size();
							_draw_arguments[ix].start_vertex_location = _draw_arguments[ix - 1].start_vertex_location + mesh->submeshes[ix - 1].vertices.size();
						}

                        const auto& submaterial = material->submaterials[submesh.material_index];

                        auto& geometry_constant = _geometry_constants[ix];

                        geometry_constant.world_matrix = submesh.world_matrix;
                        geometry_constant.inv_trans_world = inverse(transpose(submesh.world_matrix));
                        
                        geometry_constant.diffuse = submaterial.diffuse_factor;
                        geometry_constant.emissive = submaterial.emissive_factor;    
                        geometry_constant.roughness = submaterial.roughness_factor;
                        geometry_constant.metallic = submaterial.metallic_factor;
                        geometry_constant.occlusion = submaterial.occlusion_factor;

            			BindingSetItemArray binding_set_items(Material::TextureType_Num);
                        for (uint32_t jx = 0; jx < Material::TextureType_Num; ++jx)
                        {
                            const auto& image = submaterial.images[jx];
                            uint32_t texture_index = ix * Material::TextureType_Num + jx;

                            std::shared_ptr<TextureInterface> material_texture;
                            
                            if (image.is_valid())
                            {
                                ReturnIfFalse(material_texture = std::shared_ptr<TextureInterface>(device->create_texture(
                                    TextureDesc::create_shader_resource(
                                        image.width,
                                        image.height,
                                        image.format
                                    )
                                )));
                                ReturnIfFalse(cmdlist->write_texture(
                                    material_texture.get(), 
                                    0, 
                                    0, 
                                    image.data.get(), 
                                    image.size / image.height
                                ));
                            }
                            else
                            {
                                material_texture = check_cast<TextureInterface>(cache->require("BlackTexture"));
                            }

			                binding_set_items[jx] = BindingSetItem::create_texture_srv(0, material_texture);
                        }
                        ReturnIfFalse(_binding_sets[ix] = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
                            BindingSetDesc{ .binding_items = binding_set_items },
                            _binding_layout.get()
                        )));
					}
                    return true;
                }
            );
            ReturnIfFalse(res);

            ReturnIfFalse(_vertex_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_vertex(
					sizeof(Vertex) * _vertices.size(), 
					"GeometryVertexBuffer"
				)
			)));
            ReturnIfFalse(_index_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
                BufferDesc::create_index(
                    sizeof(uint32_t) * _indices.size(),
                    "GeometryIndexBuffer"
                )
            )))
            ReturnIfFalse(cmdlist->write_buffer(
                _vertex_buffer.get(), 
                _vertices.data(), 
                _vertices.size() * sizeof(Vertex)
            ));
            ReturnIfFalse(cmdlist->write_buffer(
                _index_buffer.get(), 
                _indices.data(), 
                _indices.size() * sizeof(uint32_t)
            ));
        }

        return true;
    }
}