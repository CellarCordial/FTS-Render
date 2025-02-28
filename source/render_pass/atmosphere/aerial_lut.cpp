#include "aerial_lut.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/light.h"
#include "../../scene/camera.h"
#include <memory>
#include <string>
#include <vector>

namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
#define AERIAL_LUT_RES_X 200
#define AERIAL_LUT_RES_Y 150
#define AERIAL_LUT_RES_Z 32

	bool AerialLUTPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		// BindingLayout.
		{
			BindingLayoutItemArray binding_layout_items(8);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::AerialLUTPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_constant_buffer(1);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[5] = BindingLayoutItem::create_sampler(0);
			binding_layout_items[6] = BindingLayoutItem::create_sampler(1);
			binding_layout_items[7] = BindingLayoutItem::create_texture_uav(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "atmosphere/aerial_lut_cs.hlsl";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
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

		// Texture.
		{
			ReturnIfFalse(_aerial_lut_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					AERIAL_LUT_RES_X,
					AERIAL_LUT_RES_Y,
					AERIAL_LUT_RES_Z,
					Format::RGBA32_FLOAT,
					"aerial_lut_texture"
				)
			)));
			cache->collect(_aerial_lut_texture, ResourceType::Texture);
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(8);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::AerialLUTPassConstant));
			binding_set_items[1] = BindingSetItem::create_constant_buffer(1, check_cast<BufferInterface>(cache->require("atmosphere_properties_buffer")));
			binding_set_items[2] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("multi_scattering_texture")));
			binding_set_items[3] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require("transmittance_texture")));
			binding_set_items[4] = BindingSetItem::create_texture_srv(2, check_cast<TextureInterface>(cache->require("shadow_map_texture")), TextureSubresourceSet{}, Format::R32_FLOAT);
			binding_set_items[5] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
			binding_set_items[6] = BindingSetItem::create_sampler(1, check_cast<SamplerInterface>(cache->require("point_clamp_sampler")));
			binding_set_items[7] = BindingSetItem::create_texture_uav(0, _aerial_lut_texture);
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout
			)));
		}

		// Compute state.
		{
			_compute_state.binding_sets.push_back(_binding_set.get());
			_compute_state.pipeline = _pipeline.get();
		}

		return true;
	}

	bool AerialLUTPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		// Update constant.
		{
			float* world_scale;
			ReturnIfFalse(cache->require_constants("world_scale", reinterpret_cast<void**>(&world_scale)));
			_pass_constant.world_scale = *world_scale;

			ReturnIfFalse(cache->get_world()->each<Camera>(
				[this](Entity* entity, Camera* _camera) -> bool
				{
					
					return true;
				}
			));
			Entity* global_entity = cache->get_world()->get_global_entity();

			Camera* camera = global_entity->get_component<Camera>();
			_pass_constant.camera_height = _pass_constant.world_scale * camera->position.y;
			_pass_constant.camera_position = camera->position;

			auto frustum = camera->get_frustum_directions();
			_pass_constant.frustum_a = frustum.A;
			_pass_constant.frustum_b = frustum.B;
			_pass_constant.frustum_c = frustum.C;
			_pass_constant.frustum_d = frustum.D;
			
			DirectionalLight* light = global_entity->get_component<DirectionalLight>();
			_pass_constant.sun_direction = light->direction;
			_pass_constant.sun_theta = std::asin(-light->direction.y);
			_pass_constant.shadow_view_proj = light->get_view_proj();

		}

		uint2 thread_group_num = {
			static_cast<uint32_t>(align(AERIAL_LUT_RES_X, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X),
			static_cast<uint32_t>(align(AERIAL_LUT_RES_Y, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y),
		};

		ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constant));

		ReturnIfFalse(cmdlist->close());
		return true;
	}
}
