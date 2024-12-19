
#include "brdf_lut.h"
#include "../../shader/shader_compiler.h"
#include <memory>

namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16u
#define THREAD_GROUP_SIZE_Y 16u
 
	bool BrdfLUTPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(1);
			binding_layout_items[0] = BindingLayoutItem::create_texture_uav(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "deferred/brdf_lut.slang";
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
			pipeline_desc.compute_shader = _cs.get();
			pipeline_desc.binding_layouts.push_back(_binding_layout.get());
			ReturnIfFalse(_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));
		}

		// Texture.
		{
			ReturnIfFalse(_brdf_lut_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write(
					_brdf_lut_resolution.x,
					_brdf_lut_resolution.y,
					Format::RGBA32_FLOAT,
					"BrdfLUTTexture"
				)
			)));
			cache->collect(_brdf_lut_texture, ResourceType::Texture);
		}
 
		// Binding Set.
		{
			BindingSetItemArray binding_set_items(1);
			binding_set_items[0] = BindingSetItem::create_texture_uav(0, _brdf_lut_texture);
            ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
                BindingSetDesc{ .binding_items = binding_set_items },
                _binding_layout.get()
            )));
		}

		// Compute state.
		{
			_compute_state.binding_sets.push_back(_binding_set.get());
			_compute_state.pipeline = _pipeline.get();
		}

		return true;
	}

	bool BrdfLUTPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
        ReturnIfFalse(cmdlist->open());

        Vector2I thread_group_num = {
            static_cast<uint32_t>((Align(_brdf_lut_resolution.x, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
            static_cast<uint32_t>((Align(_brdf_lut_resolution.y, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
        };

        ReturnIfFalse(cmdlist->set_compute_state(_compute_state));
        ReturnIfFalse(cmdlist->dispatch(thread_group_num.x, thread_group_num.y));

        ReturnIfFalse(cmdlist->close());
        return true;
	}
}
