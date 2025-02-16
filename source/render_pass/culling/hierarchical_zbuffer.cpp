#include "hierarchical_zbuffer.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/scene.h"
#include <memory>

namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16u 
#define THREAD_GROUP_SIZE_Y 16u

	bool HierarchicalZBufferPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
        std::shared_ptr<TextureInterface> hierarchical_zbuffer_texture =
             check_cast<TextureInterface>(cache->require("hierarchical_zbuffer_texture"));

        uint32_t texture_mip_levels = hierarchical_zbuffer_texture->get_desc().mip_levels;
        _pass_constants.resize(texture_mip_levels);

		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(2 + texture_mip_levels);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::HierarchicalZBufferPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_sampler(0);
            for (uint32_t ix = 0; ix < texture_mip_levels; ++ix)
            {
			    binding_layout_items[2 + ix] = BindingLayoutItem::create_texture_uav(ix);
            }
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "culling/hierarchical_zbuffer_cs.slang";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.resize(3);
			cs_compile_desc.defines[0] = "THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X);
			cs_compile_desc.defines[1] = "THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y);

			cs_compile_desc.defines[2] = "COPY_DEPTH=" + std::to_string(0);
			ShaderData calc_mip_cs_data = compile_shader(cs_compile_desc);
			cs_compile_desc.defines[2] = "COPY_DEPTH=" + std::to_string(1);
			ShaderData copy_depth_cs_data = compile_shader(cs_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.entry = "main";
			cs_desc.shader_type = ShaderType::Compute;
			ReturnIfFalse(_calc_mip_cs = std::unique_ptr<Shader>(create_shader(cs_desc, calc_mip_cs_data.data(), calc_mip_cs_data.size())));
			ReturnIfFalse(_copy_depth_cs = std::unique_ptr<Shader>(create_shader(cs_desc, copy_depth_cs_data.data(), copy_depth_cs_data.size())));
		}

		// Pipeline.
		{
			ComputePipelineDesc pipeline_desc;
			pipeline_desc.binding_layouts.push_back(_binding_layout);

			pipeline_desc.compute_shader = _calc_mip_cs;
			ReturnIfFalse(_calc_mip_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));
			pipeline_desc.compute_shader = _copy_depth_cs;
			ReturnIfFalse(_copy_depth_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(2 + texture_mip_levels);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::HierarchicalZBufferPassConstant));
			binding_set_items[1] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
            for (uint32_t ix = 0; ix < texture_mip_levels; ++ix)
            {
			    binding_set_items[2 + ix] = BindingSetItem::create_texture_uav(
                    ix, 
                    hierarchical_zbuffer_texture,
                    TextureSubresourceSet{
                        .base_mip_level = ix,
                        .mip_level_count = 1,
                        .base_array_slice = 0,
                        .array_slice_count = 1  
                    }
                );
            }
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout
			)));
		}

		// Compute state.
		{
			_compute_state.binding_sets.push_back(_binding_set.get());
		}

        uint32_t* ptr = &_pass_constants[0].hzb_resolution;
        ReturnIfFalse(cache->require_constants("hzb_resolution", reinterpret_cast<void**>(&ptr)));
        for (auto& constant : _pass_constants) constant.hzb_resolution = _pass_constants[0].hzb_resolution;
 
		return true;
	}

	bool HierarchicalZBufferPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());
        if (SceneSystem::loaded_submesh_count != 0)
		{
			uint2 thread_group_num = {
				static_cast<uint32_t>((align(_pass_constants[0].hzb_resolution, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
				static_cast<uint32_t>((align(_pass_constants[0].hzb_resolution, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
			};
			
			// _pass_constants 中变化的只有 last_mip_level, 而这 copy_depth pass 并不关心, 所以直接使用 _pass_constants[0] 即可.
			_compute_state.pipeline = _copy_depth_pipeline.get();
			ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constants[0]));
	
			_compute_state.pipeline = _calc_mip_pipeline.get();
			for (uint32_t ix = 0; ix < _pass_constants.size(); ++ix)
			{
				ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constants[ix]));
			}
		}
		ReturnIfFalse(cmdlist->close());
		return true;
	}
}

