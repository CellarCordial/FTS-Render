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
        _hierarchical_zbuffer_texture = check_cast<TextureInterface>(cache->require("hierarchical_zbuffer_texture"));

        uint32_t texture_mip_levels = _hierarchical_zbuffer_texture->get_desc().mip_levels;
        _pass_constants.resize(texture_mip_levels);

		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(3 + texture_mip_levels);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::HierarchicalZBufferPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_sampler(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(0);
            for (uint32_t ix = 0; ix < texture_mip_levels; ++ix)
            {
			    binding_layout_items[3 + ix] = BindingLayoutItem::create_texture_uav(ix);
            }
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "culling/hierarchical_zbuffer_update_cs.hlsl";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			cs_compile_desc.defines.push_back("REVERSE_Z_BUFFER=1");
			ShaderData cs_data = compile_shader(cs_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.entry = "main";
			cs_desc.shader_type = ShaderType::Compute;
			ReturnIfFalse(_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));
		}

		// Pipeline.
		{
			ComputePipelineDesc pipeline_desc;
			pipeline_desc.binding_layouts.push_back(_binding_layout);

			pipeline_desc.compute_shader = _cs;
			ReturnIfFalse(_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(3 + texture_mip_levels);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::HierarchicalZBufferPassConstant));
			binding_set_items[1] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
			binding_set_items[2] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("reverse_depth_texture")), TextureSubresourceSet{}, Format::R32_FLOAT);
            for (uint32_t ix = 0; ix < texture_mip_levels; ++ix)
            {
			    binding_set_items[3 + ix] = BindingSetItem::create_texture_uav(
                    ix, 
                    _hierarchical_zbuffer_texture,
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
			_compute_state.pipeline = _pipeline.get();
		}

        uint32_t* hzb_resolution = nullptr;
        ReturnIfFalse(cache->require_constants("hzb_resolution", reinterpret_cast<void**>(&hzb_resolution)));
        for (uint32_t ix = 0; ix < _pass_constants.size(); ++ix)
		{
			_pass_constants[ix].last_mip_level = ix;
			_pass_constants[ix].hzb_resolution = *hzb_resolution;
		}
 
		return true;
	}

	bool HierarchicalZBufferPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

        if (SceneSystem::loaded_submesh_count != 0)
		{
            cmdlist->clear_texture_float(
                _hierarchical_zbuffer_texture.get(), 
                TextureSubresourceSet{
                    .base_mip_level = 0,
                    .mip_level_count = _hierarchical_zbuffer_texture->get_desc().mip_levels,
                    .base_array_slice = 0,
                    .array_slice_count = 1
                }, 
                Color{ 0.0f }
            );

			uint2 thread_group_num = {
				static_cast<uint32_t>((align(_pass_constants[0].hzb_resolution, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
				static_cast<uint32_t>((align(_pass_constants[0].hzb_resolution, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
			};
			
			for (uint32_t ix = 0; ix < _pass_constants.size(); ++ix)
			{
				ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constants[ix]));
			}
		}
		ReturnIfFalse(cmdlist->close());
		return true;
	}
}

