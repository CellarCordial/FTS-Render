
#include "gbuffer.h"
#include "../../shader/shader_compiler.h"
#include "../../scene/camera.h"
#include "../../core/tools/check_cast.h"
#include <cstdint>
#include <memory>

namespace fantasy
{
	bool GBufferPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		cache->get_world()->get_global_entity()->get_component<event::AddModel>()->add_event(
            [this]() -> bool
            {
				_update_gbuffer = true;
				return true;
            }
        );

		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(9);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::GBufferPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[5] = BindingLayoutItem::create_texture_srv(4);
			binding_layout_items[6] = BindingLayoutItem::create_texture_srv(5);
			binding_layout_items[7] = BindingLayoutItem::create_structured_buffer_srv(6);
			binding_layout_items[8] = BindingLayoutItem::create_sampler(0);
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
				vertex_attribute_desc.size()
			)));
		}


		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "deferred/gbuffer_vs.hlsl";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = "deferred/gbuffer_ps.hlsl";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Pixel;
			ShaderData ps_data = compile_shader(shader_compile_desc);

			ShaderDesc vs_desc;
			vs_desc.entry = "main";
			vs_desc.shader_type = ShaderType::Vertex;
			ReturnIfFalse(_vs = std::unique_ptr<Shader>(create_shader(vs_desc, vs_data.data(), vs_data.size())));

			ShaderDesc ps_desc;
			ps_desc.shader_type = ShaderType::Pixel;
			ps_desc.entry = "main";
			ReturnIfFalse(_ps = std::unique_ptr<Shader>(create_shader(ps_desc, ps_data.data(), ps_data.size())));
		}


        // Texture
        {
			std::string path = std::string(PROJ_DIR) + "asset/image/black.png";
			_black_image = Image::load_image_from_file(path.c_str());
			ReturnIfFalse(_black_image.is_valid());
			ReturnIfFalse(_black_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_shader_resource_texture(
					_black_image.width,
					_black_image.height,
					_black_image.format,
					"black_texture"
				)
			)));
			cache->collect(_black_texture, ResourceType::Texture);

            ReturnIfFalse(_world_position_view_depth_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target_texture(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA32_FLOAT,
					"world_position_view_depth_texture"
				)
			)));
            ReturnIfFalse(_world_space_normal_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target_texture(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA32_FLOAT,
					"world_space_normal_texture"
				)
			)));
            ReturnIfFalse(_base_color_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target_texture(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA32_FLOAT,
					"base_color_texture"
				)
			)));
            ReturnIfFalse(_pbr_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target_texture(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA32_FLOAT,
					"pbr_texture"
				)
			)));
            ReturnIfFalse(_emissive_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target_texture(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA32_FLOAT,
					"emissive_texture"
				)
			)));
            ReturnIfFalse(_view_space_velocity_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target_texture(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RGBA32_FLOAT,
					"view_space_velocity_texture"
				)
			)));
            ReturnIfFalse(_depth_texture = std::shared_ptr<TextureInterface>(device->create_texture(
                TextureDesc::create_depth_stencil_texture(
                    CLIENT_WIDTH, 
                    CLIENT_HEIGHT, 
                    Format::D32,
                    "depth_texture"
                )
            )));
            
			cache->collect(_world_position_view_depth_texture, ResourceType::Texture);
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
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_world_position_view_depth_texture));
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
			pipeline_desc.vertex_shader = _vs;
			pipeline_desc.pixel_shader = _ps;
			pipeline_desc.input_layout = _input_layout;
			pipeline_desc.binding_layouts.push_back(_binding_layout);
			pipeline_desc.render_state.depth_stencil_state.enable_depth_test = true;
			pipeline_desc.render_state.depth_stencil_state.enable_depth_write = true;
			pipeline_desc.render_state.depth_stencil_state.depth_func = ComparisonFunc::LessOrEqual;
			ReturnIfFalse(_pipeline = std::unique_ptr<GraphicsPipelineInterface>(
				device->create_graphics_pipeline(pipeline_desc, _frame_buffer.get())
			));
		}

		_graphics_state.binding_sets.resize(1);
		_graphics_state.vertex_buffer_bindings.resize(1);
        _graphics_state.pipeline = _pipeline.get();
        _graphics_state.frame_buffer = _frame_buffer.get();
        _graphics_state.viewport_state = ViewportState::create_default_viewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		ReturnIfFalse(_anisotropic_warp_sampler = check_cast<SamplerInterface>(cache->require("anisotropic_wrap_sampler")));

		return true;
	}

	bool GBufferPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

        ReturnIfFalse(update(cmdlist, cache));


		clear_color_attachment(cmdlist, _frame_buffer.get());
		clear_depth_stencil_attachment(cmdlist, _frame_buffer.get());

        for (uint32_t ix = 0; ix < _draw_arguments.size(); ++ix)
        {
			_pass_constant.geometry_constant_index = ix;
			_graphics_state.binding_sets[0] = _binding_sets[ix].get();
            ReturnIfFalse(cmdlist->draw_indexed(_graphics_state, _draw_arguments[ix], &_pass_constant));
        }

		ReturnIfFalse(cmdlist->close());
		return true;
	}

    bool GBufferPass::finish_pass(RenderResourceCache* cache)
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
                return true;
            }
        ))

		if (!_resource_writed)
		{
			_resource_writed = true;

			ReturnIfFalse(cmdlist->write_texture(
				_black_texture.get(), 
				0, 
				0, 
				_black_image.data.get(), 
				_black_image.size / _black_image.height
			));
		}

        if (_update_gbuffer)
        {
			_update_gbuffer = false;

            _binding_sets.clear();
            _index_buffer.reset();
            _vertex_buffer.reset();
            _draw_arguments.clear();
			_geometry_constants.clear();

			std::vector<BindingSetItemArray> binding_set_item_arrays;

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
						uint64_t submesh_index = ix - old_size;
						const auto& submesh = mesh->submeshes[submesh_index];
						_vertices.insert(_vertices.end(), submesh.vertices.begin(), submesh.vertices.end());
						_indices.insert(_indices.end(), submesh.indices.begin(), submesh.indices.end());

						if (submesh_index != 0)
						{
							_draw_arguments[ix].index_count = submesh.indices.size();
							_draw_arguments[ix].start_index_location = _draw_arguments[ix - 1].start_index_location + mesh->submeshes[submesh_index - 1].indices.size();
							_draw_arguments[ix].start_vertex_location = _draw_arguments[ix - 1].start_vertex_location + mesh->submeshes[submesh_index - 1].vertices.size();
						}

						auto& geometry_constant = _geometry_constants[ix];

						geometry_constant.world_matrix = submesh.world_matrix;
						geometry_constant.inv_trans_world = inverse(transpose(submesh.world_matrix));

						auto& binding_set_item_array = binding_set_item_arrays.emplace_back();
						binding_set_item_array.resize(Material::TextureType_Num + 3);
						binding_set_item_array[0] = BindingSetItem::create_push_constants(0, sizeof(constant::GBufferPassConstant));

						if (submesh.material_index != INVALID_SIZE_32)
						{
							const auto& submaterial = material->submaterials[submesh.material_index];

							geometry_constant.diffuse = Vector4(submaterial.base_color_factor);
							geometry_constant.emissive = Vector4(submaterial.emissive_factor);    
							geometry_constant.roughness = submaterial.roughness_factor;
							geometry_constant.metallic = submaterial.metallic_factor;
							geometry_constant.occlusion = submaterial.occlusion_factor;

							for (uint32_t jx = 0; jx < Material::TextureType_Num; ++jx)
							{
								const auto& image = submaterial.images[jx];
								uint32_t texture_index = ix * Material::TextureType_Num + jx;

								std::shared_ptr<TextureInterface> material_texture;
								
								if (image.is_valid())
								{
									ReturnIfFalse(material_texture = std::shared_ptr<TextureInterface>(device->create_texture(
										TextureDesc::create_shader_resource_texture(
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
									material_texture = _black_texture;
								}

								binding_set_item_array[jx + 1] = BindingSetItem::create_texture_srv(jx, material_texture);
							}
						}
						else 
						{
							for (uint32_t jx = 0; jx < Material::TextureType_Num; ++jx)
							{
								binding_set_item_array[jx + 1] = BindingSetItem::create_texture_srv(jx, _black_texture);
							}
						}
					}
                    return true;
                }
            );
            ReturnIfFalse(res);

            ReturnIfFalse(_vertex_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_vertex_buffer(
					sizeof(Vertex) * _vertices.size(), 
					"geometry_vertex_buffer"
				)
			)));
			cache->collect(_vertex_buffer, ResourceType::Buffer);

            ReturnIfFalse(_index_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
                BufferDesc::create_index_buffer(
                    sizeof(uint32_t) * _indices.size(),
                    "geometry_index_buffer"
                )
            )));
			cache->collect(_index_buffer, ResourceType::Buffer);

			ReturnIfFalse(_geometry_constant_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_structured_buffer(
					_geometry_constants.size() * sizeof(constant::GeometryConstant), 
					sizeof(constant::GeometryConstant),
					"geometry_constant_buffer"
				)
			)));
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
			ReturnIfFalse(cmdlist->write_buffer(
                _geometry_constant_buffer.get(), 
                _geometry_constants.data(), 
                _geometry_constants.size() * sizeof(constant::GeometryConstant)
            ));

			for (uint32_t ix = 0; ix < binding_set_item_arrays.size(); ++ix)
			{
				auto& binding_set_item_array = binding_set_item_arrays[ix];
				binding_set_item_array[Material::TextureType_Num + 1] = 
					BindingSetItem::create_structured_buffer_srv(6, _geometry_constant_buffer);
				binding_set_item_array[Material::TextureType_Num + 2] = 
					BindingSetItem::create_sampler(0, _anisotropic_warp_sampler);
				ReturnIfFalse(_binding_sets[ix] = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
					BindingSetDesc{ .binding_items = binding_set_item_array },
					_binding_layout
				)));

			}

			_graphics_state.index_buffer_binding = IndexBufferBinding{ .buffer = _index_buffer };
        	_graphics_state.vertex_buffer_bindings[0] = VertexBufferBinding{ .buffer = _vertex_buffer };
        }
        return true;
    }
}
