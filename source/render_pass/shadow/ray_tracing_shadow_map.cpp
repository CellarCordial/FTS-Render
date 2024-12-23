

#include "ray_tracing_shadow_map.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/light.h"
#include "../../scene/scene.h"
#include <cstdint>
#include <memory>

namespace fantasy
{
	bool RayTracingShadowMapPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
        cache->get_world()->each<event::ModelLoaded>(
            [this](Entity* entity, event::ModelLoaded* event) -> bool
            {
                event->add_event([&]() { _update_vertex_buffer = true; return true; });
                return true;
            }
        );

		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(7);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::RayTracingShadowMapPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[5] = BindingLayoutItem::create_accel_struct(4);
			binding_layout_items[6] = BindingLayoutItem::create_texture_uav(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "shadow/shadow_map_raygen.slang";
			shader_compile_desc.entry_point = "ray_generation_shader";
			ShaderData ray_gen_data = shader_compile::compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = "shadow/shadow_map_miss.slang";
			shader_compile_desc.entry_point = "miss_shader";
			ShaderData miss_data = shader_compile::compile_shader(shader_compile_desc);

			ShaderDesc ray_gen_desc;
			ray_gen_desc.entry = "ray_generation_shader";
			ray_gen_desc.shader_type = ShaderType::RayGeneration;
			ReturnIfFalse(_ray_gen_shader = std::unique_ptr<Shader>(create_shader(ray_gen_desc, ray_gen_data.data(), ray_gen_data.size())));

			ShaderDesc miss_desc;
			miss_desc.shader_type = ShaderType::Miss;
			miss_desc.entry = "miss_shader";
			ReturnIfFalse(_miss_shader = std::unique_ptr<Shader>(create_shader(miss_desc, miss_data.data(), miss_data.size())));
		}
		// Texture.
		{
			ReturnIfFalse(_shadow_map_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::R8_UNORM,
					"shadow_map_texture"
				)
			)));
			cache->collect(_shadow_map_texture, ResourceType::Texture);
		}

		// Pipeline & Shader Table.
		{
            ray_tracing::PipelineDesc pipeline_desc;
            pipeline_desc.max_payload_size = sizeof(bool);
            pipeline_desc.global_binding_layouts.push_back(_binding_layout.get());
            pipeline_desc.shader_descs.push_back(ray_tracing::ShaderDesc{ .shader = _ray_gen_shader.get() });
            pipeline_desc.shader_descs.push_back(ray_tracing::ShaderDesc{ .shader = _miss_shader.get() });
            ReturnIfFalse(_pipeline = std::unique_ptr<ray_tracing::PipelineInterface>(device->create_ray_tracing_pipline(pipeline_desc)));
            ReturnIfFalse(_shader_table = std::unique_ptr<ray_tracing::ShaderTableInterface>(_pipeline->create_shader_table()));
            _shader_table->set_raygen_shader("ray_generation_shader");
            _shader_table->add_miss_shader("miss_shader");
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(7);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::RayTracingShadowMapPassConstant));
			binding_set_items[1] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("depth_texture")));
			binding_set_items[2] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require("blue_noise_texture")));
			binding_set_items[3] = BindingSetItem::create_texture_srv(2, check_cast<TextureInterface>(cache->require("world_space_normal_texture")));
			binding_set_items[4] = BindingSetItem::create_texture_srv(3, check_cast<TextureInterface>(cache->require("world_space_position_texture")));
			binding_set_items[5] = BindingSetItem::create_accel_struct(4, _top_level_accel_struct);
			binding_set_items[6] = BindingSetItem::create_texture_uav(0, _shadow_map_texture);
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout.get()
			)));
		}

		// Ray Tracing State.
		{
            _ray_tracing_state.binding_sets.push_back(_binding_set.get());
            _ray_tracing_state.shader_table = _shader_table.get();

            _dispatch_rays_arguments.width = CLIENT_WIDTH;
            _dispatch_rays_arguments.height = CLIENT_HEIGHT;
		}

		return true;
	}

	bool RayTracingShadowMapPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
        ReturnIfFalse(cmdlist->open());

        if (_update_vertex_buffer)
        {
            ray_tracing::GeometryTriangles geometry_triangles(
                check_cast<BufferInterface>(cache->require("geometry_index_buffer")),
                check_cast<BufferInterface>(cache->require("geometry_vertex_buffer")),
                sizeof(Vertex)
            );

            _ray_tracing_geometry_descs.emplace_back(geometry_triangles);

            DeviceInterface* device = cmdlist->get_deivce();

            ray_tracing::AccelStructDesc blas_desc;
            blas_desc.is_top_level = false;
            blas_desc.bottom_level_geometry_descs = _ray_tracing_geometry_descs;
            blas_desc.flags = ray_tracing::AccelStructBuildFlags::PreferFastTrace;
            _bottom_level_accel_struct = std::shared_ptr<ray_tracing::AccelStructInterface>(device->create_accel_struct(blas_desc));
            ReturnIfFalse(_bottom_level_accel_struct != nullptr);

            ray_tracing::AccelStructDesc tlas_desc;
            tlas_desc.is_top_level = true;
            tlas_desc.top_level_max_instance_num = 1;
            _top_level_accel_struct = std::shared_ptr<ray_tracing::AccelStructInterface>(device->create_accel_struct(tlas_desc));
            ReturnIfFalse(_top_level_accel_struct != nullptr);
            
            cmdlist->build_bottom_level_accel_struct(
                _bottom_level_accel_struct.get(), 
                _ray_tracing_geometry_descs.data(), 
                _ray_tracing_geometry_descs.size()
            );

            _ray_tracing_instance_descs.emplace_back(ray_tracing::InstanceDesc{ .bottom_level_accel_struct = _bottom_level_accel_struct.get() });

            cmdlist->build_top_level_accel_struct(
                _top_level_accel_struct.get(), 
                _ray_tracing_instance_descs.data(), 
                _ray_tracing_instance_descs.size()
            );
        }

        _pass_constant.frame_index = static_cast<uint32_t>(cache->frame_index);
        ReturnIfFalse(cache->get_world()->each<DirectionalLight>(
            [this](Entity* entity, DirectionalLight* light) -> bool
            {
                _pass_constant.sun_direction = light->direction;
                _pass_constant.sun_angular_radius = light->sun_angular_radius;
                return true;
            }
        ));

        ReturnIfFalse(cmdlist->set_ray_tracing_state(_ray_tracing_state));
        ReturnIfFalse(cmdlist->set_push_constants(&_pass_constant, sizeof(constant::RayTracingShadowMapPassConstant)));
        ReturnIfFalse(cmdlist->dispatch_rays(_dispatch_rays_arguments));

        ReturnIfFalse(cmdlist->close());
		return true;
	}
}

