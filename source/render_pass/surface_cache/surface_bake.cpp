
#include "surface_bake.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/surface_cache.h"
#include "../../scene/geometry.h"
#include <cstdint>
#include <memory>
#include <string>

namespace fantasy
{
	bool SurfaceBakePass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
        cache->get_world()->get_global_entity()->get_component<event::GenerateSurfaceCache>()->add_event(
            [this](Entity* model_entity) -> bool
            {
                _model_entity = model_entity;
                return true;
            }
        );
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(6);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::SurfaceBakePassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[5] = BindingLayoutItem::create_sampler(0);
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
			shader_compile_desc.shader_name = "surface_cache/surface_bake_vs.slang";
			shader_compile_desc.entry_point = "vertex_shader";
			shader_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = shader_compile::compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = "surface_cache/surface_bake_ps.slang";
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

		// Pipeline.
		{
			_pipeline_desc.vertex_shader = _vs.get();
			_pipeline_desc.pixel_shader = _ps.get();
			_pipeline_desc.input_layout = _input_layout.get();
			_pipeline_desc.binding_layouts.push_back(_binding_layout.get());
		}

		// Binding Set.
		{
			_binding_set_items.resize(5);
			_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::SurfaceBakePassConstant));
			_binding_set_items[4] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
		}

		return true;
	}

	bool SurfaceBakePass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
        DeviceInterface* device = cmdlist->get_deivce();
        if (_model_entity)
        {
            const Mesh* mesh = _model_entity->get_component<Mesh>();
            const std::string& model_name = *_model_entity->get_component<std::string>();
            const SurfaceCache* surface_cache = _model_entity->get_component<SurfaceCache>();
            
            ReturnIfFalse(mesh->submeshes.size() == surface_cache->mesh_surface_caches.size());

            for (uint32_t ix = 0; ix < mesh->submeshes.size(); ++ix)
            {
                const auto& submesh = mesh->submeshes[ix];
                const auto& submesh_surface_cache = surface_cache->mesh_surface_caches[ix];

                // Buffer
                {
                    uint64_t vertices_size = sizeof(Vertex) * submesh.vertices.size();
                    uint64_t indices_size = sizeof(uint32_t) * submesh.indices.size();
                    ReturnIfFalse(device->create_buffer(BufferDesc::create_vertex(vertices_size)));
                    ReturnIfFalse(device->create_buffer(BufferDesc::create_index(indices_size)));
                    ReturnIfFalse(cmdlist->write_buffer(_vertex_buffer.get(), submesh.vertices.data(), vertices_size));
                    ReturnIfFalse(cmdlist->write_buffer(_index_buffer.get(), submesh.indices.data(), indices_size));
                }

                std::shared_ptr<TextureInterface> surface_base_color_texture;
                std::shared_ptr<TextureInterface> surface_normal_texture;
                std::shared_ptr<TextureInterface> surface_pbr_texture;
                std::shared_ptr<TextureInterface> surface_emissive_texture;
                std::shared_ptr<TextureInterface> surface_light_cache_texture;
                std::shared_ptr<TextureInterface> surface_depth_texture;

                // Texture
                {

                    ReturnIfFalse(surface_base_color_texture = std::shared_ptr<TextureInterface>(device->create_texture(
                        TextureDesc::create_render_target(
                            SURFACE_RESOLUTION, 
                            SURFACE_RESOLUTION, 
                            Format::RGBA32_FLOAT,
                            submesh_surface_cache.surfaces[SurfaceCache::MeshSurfaceCache::SurfaceType_BaseColor].surface_texture_name.c_str()
                        )
                    )));
                    ReturnIfFalse(surface_normal_texture = std::shared_ptr<TextureInterface>(device->create_texture(
                        TextureDesc::create_render_target(
                            SURFACE_RESOLUTION, 
                            SURFACE_RESOLUTION, 
                            Format::RGB32_FLOAT,
                            submesh_surface_cache.surfaces[SurfaceCache::MeshSurfaceCache::SurfaceType_Normal].surface_texture_name.c_str()
                        )
                    )));
                    ReturnIfFalse(surface_pbr_texture = std::shared_ptr<TextureInterface>(device->create_texture(
                        TextureDesc::create_render_target(
                            SURFACE_RESOLUTION, 
                            SURFACE_RESOLUTION, 
                            Format::RGB32_FLOAT,
                            submesh_surface_cache.surfaces[SurfaceCache::MeshSurfaceCache::SurfaceType_PBR].surface_texture_name.c_str()
                        )
                    )));
                    ReturnIfFalse(surface_emissive_texture = std::shared_ptr<TextureInterface>(device->create_texture(
                        TextureDesc::create_render_target(
                            SURFACE_RESOLUTION, 
                            SURFACE_RESOLUTION, 
                            Format::RGBA32_FLOAT,
                            submesh_surface_cache.surfaces[SurfaceCache::MeshSurfaceCache::SurfaceType_Emissve].surface_texture_name.c_str()
                        )
                    )));
                    ReturnIfFalse(surface_depth_texture = std::shared_ptr<TextureInterface>(device->create_texture(
                        TextureDesc::create_depth(
                            SURFACE_RESOLUTION, 
                            SURFACE_RESOLUTION, 
                            Format::D32,
                            submesh_surface_cache.surfaces[SurfaceCache::MeshSurfaceCache::SurfaceType_Depth].surface_texture_name.c_str()
                        )
                    )));
                    ReturnIfFalse(surface_light_cache_texture = std::shared_ptr<TextureInterface>(device->create_texture(
                        TextureDesc::create_render_target(
                            SURFACE_RESOLUTION, 
                            SURFACE_RESOLUTION, 
                            Format::RGBA32_FLOAT,
                            submesh_surface_cache.light_cache.c_str()
                        )
                    )));

                    cache->collect(surface_base_color_texture, ResourceType::Texture);
                    cache->collect(surface_normal_texture, ResourceType::Texture);
                    cache->collect(surface_pbr_texture, ResourceType::Texture);
                    cache->collect(surface_emissive_texture, ResourceType::Texture);
                    cache->collect(surface_depth_texture, ResourceType::Texture);
                    cache->collect(surface_light_cache_texture, ResourceType::Texture);
                    
                }

                // Frame Buffer.
                {
                	FrameBufferDesc frame_buffer_desc;
                	frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(surface_base_color_texture));
                	frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(surface_normal_texture));
                	frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(surface_pbr_texture));
                	frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(surface_emissive_texture));
                	frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(surface_light_cache_texture));
                    frame_buffer_desc.depth_stencil_attachment = FrameBufferAttachment::create_attachment(surface_depth_texture);
                	ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(frame_buffer_desc)));
                }

                ReturnIfFalse(_pipeline = std::unique_ptr<GraphicsPipelineInterface>(
                	device->create_graphics_pipeline(_pipeline_desc, _frame_buffer.get())
                ));

                _binding_set_items[1] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require(get_geometry_texture_name(ix, Material::TextureType_BaseColor, 0, model_name).c_str())));
                _binding_set_items[2] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require(get_geometry_texture_name(ix, Material::TextureType_Normal, 0, model_name).c_str())));
                _binding_set_items[3] = BindingSetItem::create_texture_srv(2, check_cast<TextureInterface>(cache->require(get_geometry_texture_name(ix, Material::TextureType_PBR, 0, model_name).c_str())));
                _binding_set_items[4] = BindingSetItem::create_texture_srv(3, check_cast<TextureInterface>(cache->require(get_geometry_texture_name(ix, Material::TextureType_Emissive, 0, model_name).c_str())));
                ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
                	BindingSetDesc{ .binding_items = _binding_set_items },
                	_binding_layout.get()
                )));

                _graphics_state.pipeline = _pipeline.get();
                _graphics_state.frame_buffer = _frame_buffer.get();
                _graphics_state.binding_sets.push_back(_binding_set.get());
                _graphics_state.index_buffer_binding = IndexBufferBinding{ .buffer = _index_buffer };
                _graphics_state.vertex_buffer_bindings.push_back(VertexBufferBinding{ .buffer = _vertex_buffer });

                float mesh_extent = 2.0f * submesh.bounding_sphere.radius;
                float viewport_size = 3 * mesh_extent;
                _graphics_state.viewport_state.viewports.push_back(Viewport{ 0.0f, viewport_size, 0.0f, viewport_size, 0.0f, 1.0f });
                _graphics_state.viewport_state.rects.push_back(Rect{ 0, static_cast<uint32_t>(viewport_size), 0, static_cast<uint32_t>(viewport_size) });

                _pass_constant.view_proj = mul(
                    look_at_left_hand(float3(0.0f), float3(0.0, 0.0f, 1.0f), float3(0.0f, 1.0f, 0.0f)),
                    orthographic_left_hand(viewport_size, viewport_size, 0.1f, 100.0f)
                );

                float z_bias = 0.2f;
                _pass_constant.world_matrix[0] = mul(translate(float3(-mesh_extent, mesh_extent, z_bias)), rotate(float3(0.0f,  0.0f,   0.0f)));    // 前
                _pass_constant.world_matrix[1] = mul(translate(float3(0.0f,         mesh_extent, z_bias)), rotate(float3(0.0f,  90.0f,  0.0f)));   // 左
                _pass_constant.world_matrix[2] = mul(translate(float3(mesh_extent,  mesh_extent, z_bias)), rotate(float3(0.0f,  180.0f, 0.0f)));  // 后
                _pass_constant.world_matrix[3] = mul(translate(float3(-mesh_extent, 0.0f,        z_bias)), rotate(float3(0.0f,  270.0f, 0.0f)));  // 右
                _pass_constant.world_matrix[4] = mul(translate(float3(0.0f,         0.0f,        z_bias)), rotate(float3(90.0f, 0.0f,   0.0f)));   // 上
                _pass_constant.world_matrix[5] = mul(translate(float3(mesh_extent,  0.0f,        z_bias)), rotate(float3(90.0f, 0.0f,   -0.0f)));  // 下

                ReturnIfFalse(cmdlist->open());

                ReturnIfFalse(cmdlist->set_graphics_state(_graphics_state));
                ReturnIfFalse(cmdlist->set_push_constants(&_pass_constant, sizeof(constant::SurfaceBakePassConstant)));
                ReturnIfFalse(cmdlist->draw_indexed(_draw_arguments));

                ReturnIfFalse(cmdlist->close());
            }

            return true;
        }
		return false;
	}

    bool SurfaceBakePass::finish_pass()
    {
        _model_entity = nullptr;

        _frame_buffer.reset();
        _pipeline.reset();
        _binding_set.reset();
        _vertex_buffer.reset();
        _index_buffer.reset();
        return true;
    }

}


