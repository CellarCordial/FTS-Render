#include "aerial_shadow.h"

#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/camera.h"
#include "../../scene/light.h"

namespace fantasy
{	
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
#define AERIAL_LUT_RES_X 200
#define AERIAL_LUT_RES_Y 150
#define AERIAL_LUT_RES_Z 32

	bool AerialShadowPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{	
		_final_texture = check_cast<TextureInterface>(cache->require("final_texture"));
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(15);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::AerialShadowConstant));
			binding_layout_items[1] = BindingLayoutItem::create_constant_buffer(1);
			binding_layout_items[2] = BindingLayoutItem::create_texture_uav(0);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[5] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[6] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[7] = BindingLayoutItem::create_texture_srv(4);
			binding_layout_items[8] = BindingLayoutItem::create_structured_buffer_srv(5);
			binding_layout_items[9] = BindingLayoutItem::create_texture_srv(6);
			binding_layout_items[10] = BindingLayoutItem::create_texture_srv(7);
			binding_layout_items[11] = BindingLayoutItem::create_texture_srv(8);
			binding_layout_items[12] = BindingLayoutItem::create_sampler(0);
			binding_layout_items[13] = BindingLayoutItem::create_sampler(1);
			binding_layout_items[14] = BindingLayoutItem::create_sampler(2);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "atmosphere/aerial_shadow_cs.hlsl";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Compute;
			shader_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			shader_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			ShaderData cs_data = compile_shader(shader_compile_desc);

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
			ReturnIfFalse(_pipeline = std::unique_ptr<ComputePipelineInterface>(
				device->create_compute_pipeline(pipeline_desc)
			));
		}

        
		// Texture.
		{
			std::string image_path = std::string(PROJ_DIR) + "asset/image/BlueNoise.png";
			_blue_noise_image = Image::load_image_from_file(image_path.c_str());
			ReturnIfFalse(_blue_noise_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_shader_resource_texture(
					_blue_noise_image.width,
					_blue_noise_image.height,
					_blue_noise_image.format,
					"blue_noise_texture"
				)
			)));
            cache->collect(_blue_noise_texture, ResourceType::Texture);
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(15);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::AerialShadowConstant));
			binding_set_items[1] = BindingSetItem::create_constant_buffer(1, check_cast<BufferInterface>(cache->require("atmosphere_properties_buffer")));
			binding_set_items[2] = BindingSetItem::create_texture_uav(0, _final_texture);
			binding_set_items[3] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("world_position_view_depth_texture")));
			binding_set_items[4] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require("world_space_normal_texture")));
			binding_set_items[5] = BindingSetItem::create_texture_srv(2, check_cast<TextureInterface>(cache->require("base_color_texture")));
			binding_set_items[6] = BindingSetItem::create_texture_srv(3, check_cast<TextureInterface>(cache->require("aerial_lut_texture")));
			binding_set_items[7] = BindingSetItem::create_texture_srv(4, check_cast<TextureInterface>(cache->require("transmittance_texture")));
			binding_set_items[8] = BindingSetItem::create_structured_buffer_srv(5, check_cast<BufferInterface>(cache->require("vt_shadow_indirect_buffer")));
			binding_set_items[9] = BindingSetItem::create_texture_srv(6, check_cast<TextureInterface>(cache->require("vt_physical_shadow_texture")));
			binding_set_items[10] = BindingSetItem::create_texture_srv(7, _blue_noise_texture);
			binding_set_items[11] = BindingSetItem::create_texture_srv(8, check_cast<TextureInterface>(cache->require("geometry_uv_mip_id_texture")));
			binding_set_items[12] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
			binding_set_items[13] = BindingSetItem::create_sampler(1, check_cast<SamplerInterface>(cache->require("point_clamp_sampler")));
			binding_set_items[14] = BindingSetItem::create_sampler(2, check_cast<SamplerInterface>(cache->require("point_wrap_sampler")));
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout
			)));
		}

		// Graphics state.
		{
			_compute_state.pipeline = _pipeline.get();
			_compute_state.binding_sets.push_back(_binding_set.get());
		}
		return true;
	}

	bool AerialShadowPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
        float* world_scale;
        ReturnIfFalse(cache->require_constants("world_scale", reinterpret_cast<void**>(&world_scale)));
		_pass_constant.world_scale = *world_scale;

		_pass_constant.jitter_factor = {
			_jitter_radius / AERIAL_LUT_RES_X,
			_jitter_radius / AERIAL_LUT_RES_Y
		};
		_pass_constant.blue_noise_uv_factor = {
			(1.0f * CLIENT_WIDTH) / _blue_noise_image.width,
			(1.0f * CLIENT_HEIGHT) / _blue_noise_image.height
		};

		Entity* global_entity = cache->get_world()->get_global_entity();

		_pass_constant.camera_position = global_entity->get_component<Camera>()->position;

		DirectionalLight* light = global_entity->get_component<DirectionalLight>();
		_pass_constant.sun_dir = light->direction;
		_pass_constant.sun_theta = std::asin(-light->direction.y);
		_pass_constant.sun_intensity = float3(light->intensity * light->color);
		_pass_constant.shadow_view_proj = light->get_view_proj();


		ReturnIfFalse(cmdlist->open());
		uint2 thread_group_num = {
			static_cast<uint32_t>((align(CLIENT_WIDTH, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
			static_cast<uint32_t>((align(CLIENT_HEIGHT, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
		};
		ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constant));
		ReturnIfFalse(cmdlist->close());
		return true;
	}
}