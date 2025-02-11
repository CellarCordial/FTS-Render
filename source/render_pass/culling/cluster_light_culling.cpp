
#include "cluster_light_culling.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/camera.h"
#include <cstdint>
#include <memory>


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
 
	bool ClusterLightCullingPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		cache->get_world()->get_global_entity()->get_component<event::AddPointLight>()->add_event(
			[this]() -> bool
			{
				_resource_writed = false;
				return true;
			}
		);
		
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(6);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::ClusterLightCullingPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_structured_buffer_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_structured_buffer_uav(0);
			binding_layout_items[5] = BindingLayoutItem::create_structured_buffer_uav(1);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "culling/cluster_light_culling_cs.slang";
			cs_compile_desc.entry_point = "compute_shader";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			ShaderData cs_data = shader_compile::compile_shader(cs_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.entry = "compute_shader";
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

		// Binding Set.
		{
			_binding_set_items.resize(6);
			_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::ClusterLightCullingPassConstant));
			_binding_set_items[1] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("world_position_view_depth_texture")));
		}

		// Compute state.
		{
			_compute_state.binding_sets.resize(1);
			_compute_state.pipeline = _pipeline.get();
		}
 
		return true;
	}

	bool ClusterLightCullingPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		if (!_resource_writed)
		{
			World* world = cache->get_world();

			ReturnIfFalse(world->each<PointLight>(
				[this](Entity* entity, PointLight* light) -> bool
				{
					_point_lights.emplace_back(*light);
					return true;
				}
			));
			ReturnIfFalse(world->each<SpotLight>(
				[this](Entity* entity, SpotLight* light) -> bool
				{
					_spot_lights.emplace_back(*light);
					return true;
				}
			));
			
			Camera* camera = world->get_global_entity()->get_component<Camera>();
			_pass_constant.view_proj = camera->get_view_proj();
			_pass_constant.half_fov_y = 0.5f * camera->get_fov_y();
			_pass_constant.near_z = camera->get_near_z();
			_pass_constant.point_light_count = static_cast<uint32_t>(_point_lights.size());
			_pass_constant.spot_light_count = static_cast<uint32_t>(_spot_lights.size());
			_pass_constant.divide_count = uint3(
				_tile_count.x,
				_tile_count.y,
				std::log(camera->get_far_z() / camera->get_near_z()) / 
				std::log(1.0f + 2.0f * _pass_constant.half_fov_y / _tile_count.y)
			);

			DeviceInterface* device = cmdlist->get_deivce();

			// Buffer
			{
				ReturnIfFalse(_point_light_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
					BufferDesc::create_read_write_structured_buffer(
						sizeof(PointLight) * _point_lights.size(), 
						sizeof(PointLight),
						"point_light_buffer"
					)
				)));
				ReturnIfFalse(_spot_light_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
					BufferDesc::create_read_write_structured_buffer(
						sizeof(SpotLight) * _spot_lights.size(), 
						sizeof(SpotLight),
						"spot_light_buffer"
					)
				)));

				ReturnIfFalse(cmdlist->write_buffer(_point_light_buffer.get(), _point_lights.data(), _point_lights.size()));
				ReturnIfFalse(cmdlist->write_buffer(_spot_light_buffer.get(), _spot_lights.data(), _spot_lights.size()));

				ReturnIfFalse(_light_index_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
					BufferDesc::create_read_write_structured_buffer(
						sizeof(uint32_t) * (_point_lights.size() + _spot_lights.size()) * max_cluster_light_num, 
						sizeof(uint32_t),
						"light_index_buffer"
					)
				)));
				ReturnIfFalse(_light_cluster_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
					BufferDesc::create_read_write_structured_buffer(
						sizeof(uint2) * _pass_constant.divide_count.x * _pass_constant.divide_count.y * _pass_constant.divide_count.z, 
						sizeof(uint2),
						"light_cluster_buffer"
					)
				)));
			}

			_binding_set.reset();
			_binding_set_items[2] = BindingSetItem::create_structured_buffer_srv(1, _point_light_buffer);
			_binding_set_items[3] = BindingSetItem::create_structured_buffer_srv(2, _spot_light_buffer);
			_binding_set_items[4] = BindingSetItem::create_structured_buffer_uav(0, _light_index_buffer);
			_binding_set_items[5] = BindingSetItem::create_structured_buffer_uav(1, _light_cluster_buffer);
            ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
                BindingSetDesc{ .binding_items = _binding_set_items },
                _binding_layout
            )));

			_compute_state.binding_sets[0] = _binding_set.get();
		}

		uint2 thread_group_num = {
			static_cast<uint32_t>((align(CLIENT_WIDTH, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
			static_cast<uint32_t>((align(CLIENT_HEIGHT, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
		};

		ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constant));

		ReturnIfFalse(cmdlist->close());
        return true;
	}

	bool ClusterLightCullingPass::finish_pass()
	{
		_point_lights.clear(); _point_lights.shrink_to_fit();
		_spot_lights.clear(); _spot_lights.shrink_to_fit();
		return true;
	}

}