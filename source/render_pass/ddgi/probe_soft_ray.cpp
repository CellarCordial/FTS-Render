#include "probe_soft_ray.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/distance_field.h"
#include <cstdint>


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
 
	bool ProbeSoftRayPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
        ReturnIfFalse(cache->collect_constants("ddgi_volume_data", &_ddgi_volume_data));

		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(12);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::ProbeSoftRayPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[5] = BindingLayoutItem::create_texture_srv(4);
			binding_layout_items[6] = BindingLayoutItem::create_structured_buffer_srv(5);
			binding_layout_items[7] = BindingLayoutItem::create_structured_buffer_srv(6);
			binding_layout_items[8] = BindingLayoutItem::create_texture_uav(0);
			binding_layout_items[9] = BindingLayoutItem::create_texture_uav(1);
			binding_layout_items[10] = BindingLayoutItem::create_sampler(0);
			binding_layout_items[11] = BindingLayoutItem::create_sampler(1);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "ddgi/probe_soft_ray_cs.slang";
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
            uint32_t probe_count = _ddgi_volume_data.probe_count.x * 
                                   _ddgi_volume_data.probe_count.y * 
                                   _ddgi_volume_data.probe_count.z;
            
			ReturnIfFalse(_ddgi_radiance_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					_ddgi_volume_data.ray_count,
					probe_count,
					Format::RGBA32_FLOAT,
					"ddgi_radiance_texture"
				)
			)));

			ReturnIfFalse(_ddgi_direction_distance_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					_ddgi_volume_data.ray_count,
					probe_count,
					Format::RGBA32_FLOAT,
					"ddgi_direction_distance_texture"
				)
			)));

			cache->collect(_ddgi_radiance_texture, ResourceType::Texture);
			cache->collect(_ddgi_direction_distance_texture, ResourceType::Texture);
		}
 
		// Binding Set.
		{
			BindingSetItemArray binding_set_items(12);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::ProbeSoftRayPassConstant));
			binding_set_items[1] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("sky_lut_texture")));
			binding_set_items[2] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require("global_sdf_texture")));
			binding_set_items[3] = BindingSetItem::create_texture_srv(2, check_cast<TextureInterface>(cache->require("surface_normal_atlas_texture")));
			binding_set_items[4] = BindingSetItem::create_texture_srv(3, check_cast<TextureInterface>(cache->require("surface_depth_atlas_texture")));
			binding_set_items[5] = BindingSetItem::create_texture_srv(4, check_cast<TextureInterface>(cache->require("surface_light_cache_atlas_texture")));
			binding_set_items[6] = BindingSetItem::create_structured_buffer_srv(0, check_cast<BufferInterface>(cache->require("sdf_chunk_data_buffer")));
			binding_set_items[7] = BindingSetItem::create_structured_buffer_srv(1, check_cast<BufferInterface>(cache->require("mesh_surface_data_buffer")));
			binding_set_items[8] = BindingSetItem::create_texture_uav(0, _ddgi_radiance_texture);
			binding_set_items[9] = BindingSetItem::create_texture_uav(1, _ddgi_direction_distance_texture);
			binding_set_items[10] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
			binding_set_items[11] = BindingSetItem::create_sampler(1, check_cast<SamplerInterface>(cache->require("linear_wrap_sampler")));
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

        _pass_constant.volume_data = _ddgi_volume_data;
        _pass_constant.sdf_voxel_size = SDF_SCENE_GRID_SIZE / GLOBAL_SDF_RESOLUTION;
        _pass_constant.sdf_chunk_size = VOXEL_NUM_PER_CHUNK * _pass_constant.sdf_voxel_size;

		return true;
	}

	bool ProbeSoftRayPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

        uint32_t probe_count = _ddgi_volume_data.probe_count.x * 
                               _ddgi_volume_data.probe_count.y * 
                               _ddgi_volume_data.probe_count.z;
		
        uint2 thread_group_num = {
			static_cast<uint32_t>((align(probe_count, static_cast<uint32_t>(THREAD_GROUP_SIZE_X)) / THREAD_GROUP_SIZE_X)),
			static_cast<uint32_t>((align(_ddgi_volume_data.ray_count, static_cast<uint32_t>(THREAD_GROUP_SIZE_Y)) / THREAD_GROUP_SIZE_Y)),
		};

		ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constant));

		ReturnIfFalse(cmdlist->close());
        return true;
	}
}




