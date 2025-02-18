#include "probe_update.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include <cstdint>
#include <memory>


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
 
	bool ProbeUpdatePass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
        ReturnIfFalse(cache->require_constants("ddgi_volume_data", reinterpret_cast<void**>(&_volume_data)));
        ReturnIfFalse(cache->require_constants("frame_count", reinterpret_cast<void**>(&_frame_count)));
 
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(4);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::ProbeUpdatePassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_texture_uav(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			ShaderDesc cs_desc;
			ShaderData cs_data;

			cs_compile_desc.shader_name = "ddgi/probe_depth_update_cs.hlsl";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			cs_data = compile_shader(cs_compile_desc);

			cs_desc.entry = "main";
			cs_desc.shader_type = ShaderType::Compute;
			ReturnIfFalse(_depth_update_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));
			
			cs_compile_desc.shader_name = "ddgi/probe_irradiance_update_cs.hlsl";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			cs_data = compile_shader(cs_compile_desc);

			cs_desc.entry = "main";
			cs_desc.shader_type = ShaderType::Compute;
			ReturnIfFalse(_depth_update_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));
		}

		// Pipeline.
		{
			ComputePipelineDesc depth_update_pipeline_desc;
			depth_update_pipeline_desc.compute_shader = _depth_update_cs;
			depth_update_pipeline_desc.binding_layouts.push_back(_binding_layout);
			ReturnIfFalse(_depth_update_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(depth_update_pipeline_desc)));

			
			ComputePipelineDesc irradiance_update_pipeline_desc;
			irradiance_update_pipeline_desc.compute_shader = _irradiance_update_cs;
			irradiance_update_pipeline_desc.binding_layouts.push_back(_binding_layout);
			ReturnIfFalse(_depth_update_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(irradiance_update_pipeline_desc)));
		}

		// Texture
		{
			ReturnIfFalse(_ddgi_volume_depth_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					_volume_data->volume_texture_resolution.x, 
					_volume_data->volume_texture_resolution.y, 
					Format::RG32_FLOAT,
					"ddgi_volume_depth_texture"
				)
			)));
			ReturnIfFalse(_ddgi_volume_irradiance_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					_volume_data->volume_texture_resolution.x, 
					_volume_data->volume_texture_resolution.y, 
					Format::RGBA16_FLOAT,
					"ddgi_volume_irradiance_texture"
				)
			)));

			cache->collect(_ddgi_volume_depth_texture, ResourceType::Texture);
			cache->collect(_ddgi_volume_irradiance_texture, ResourceType::Texture);
		}

		// Binding Set.
		{
			std::shared_ptr<TextureInterface> ddgi_radiance_texture = check_cast<TextureInterface>(cache->require("ddgi_radiance_texture"));
			
			BindingSetItemArray binding_set_items(3);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::ProbeUpdatePassConstant));
			binding_set_items[1] = BindingSetItem::create_texture_srv(0, ddgi_radiance_texture);
			binding_set_items[2] = BindingSetItem::create_texture_uav(0, check_cast<TextureInterface>(cache->require("ddgi_volume_depth_texture")));
            ReturnIfFalse(_depth_update_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
                BindingSetDesc{ .binding_items = binding_set_items },
                _binding_layout
            )));


			binding_set_items.resize(4);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::ProbeUpdatePassConstant));
			binding_set_items[1] = BindingSetItem::create_texture_srv(0, ddgi_radiance_texture);
			binding_set_items[2] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require("ddgi_direction_distance_texture")));
			binding_set_items[3] = BindingSetItem::create_texture_uav(0, check_cast<TextureInterface>(cache->require("ddgi_volume_irradiance_texture")));
			ReturnIfFalse(_irradiance_update_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
                BindingSetDesc{ .binding_items = binding_set_items },
                _binding_layout
            )));
		}

		// Compute state.
		{
			_compute_state.binding_sets.resize(1);
		}

		return true;
	}

	bool ProbeUpdatePass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		_pass_constant.ray_count_per_probe = _volume_data->ray_count;
		_pass_constant.first_frame = *_frame_count == 0 ? 1 : 0;
		// _pass_constant.history_alpha = ;
		// _pass_constant.history_gamma = ;
		// _pass_constant.depth_sharpness = ;

		uint2 thread_group_num = {
			static_cast<uint32_t>((align(_volume_data->volume_texture_resolution.x, static_cast<uint32_t>(THREAD_GROUP_SIZE_X)) / THREAD_GROUP_SIZE_X)),
			static_cast<uint32_t>((align(_volume_data->volume_texture_resolution.y, static_cast<uint32_t>(THREAD_GROUP_SIZE_Y)) / THREAD_GROUP_SIZE_Y)),
		};

		_compute_state.binding_sets[0] = _depth_update_binding_set.get();
		_compute_state.pipeline = _depth_update_pipeline.get();
		ReturnIfFalse(cmdlist->open());
		ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constant));
		ReturnIfFalse(cmdlist->close());
		
		_compute_state.binding_sets[0] = _irradiance_update_binding_set.get();
		_compute_state.pipeline = _irradiance_update_pipeline.get();
		ReturnIfFalse(cmdlist->open());
		ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constant));
		ReturnIfFalse(cmdlist->close());

        return true;
	}
}