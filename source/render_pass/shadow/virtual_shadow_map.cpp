#include "virtual_shadow_map.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/camera.h"
#include "../../scene/light.h"


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
 
	bool VirtualShadowMapPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(3);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::VirtualShadowMapPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_uav(2);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "shadow/virtual_shadow_map_cs.slang";
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


		// Buffer.
		{
			ReturnIfFalse(_virtual_shadow_page_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_rwstructured(
					CLIENT_WIDTH * CLIENT_HEIGHT * sizeof(uint2),
                    true, 
					"virtual_shadow_page_buffer"
				)
			)));
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(3);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::VirtualShadowMapPassConstant));
			binding_set_items[1] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require("world_position_view_depth_texture")));
			binding_set_items[2] = BindingSetItem::create_structured_buffer_uav(2, _virtual_shadow_page_buffer);
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

		_pass_constant.virtual_shadow_page_size = _virtual_shadow_page_size;
		_pass_constant.virtual_shadow_resolution = _virtual_shadow_resolution;
 
		return true;
	}

	bool VirtualShadowMapPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		Entity* global_entity = cache->get_world()->get_global_entity();
		_pass_constant.camera_position = global_entity->get_component<Camera>()->position;
		_pass_constant.shadow_view_proj = global_entity->get_component<DirectionalLight>()->view_proj;
		
		ReturnIfFalse(cmdlist->open());

		uint2 thread_group_num = {
			static_cast<uint32_t>((align(CLIENT_WIDTH, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
			static_cast<uint32_t>((align(CLIENT_HEIGHT, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
		};

		ReturnIfFalse(cmdlist->set_compute_state(_compute_state));
		ReturnIfFalse(cmdlist->dispatch(thread_group_num.x, thread_group_num.y));

		ReturnIfFalse(cmdlist->close());
        return true;
	}
}






