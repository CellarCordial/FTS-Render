// #include "virtual_shadow_map.h"
// #include "../../shader/shader_compiler.h"
// #include "../../core/tools/check_cast.h"
// #include "../../gui/gui_panel.h"


// namespace fantasy
// {
// #define THREAD_GROUP_SIZE_X 16u
// #define THREAD_GROUP_SIZE_Y 16u
 
// 	bool Pass::compile(DeviceInterface* device, RenderResourceCache* cache)
// 	{
// 		// Binding Layout.
// 		{
// 			BindingLayoutItemArray binding_layout_items(N);
// 			binding_layout_items[Index] = BindingLayoutItem::create_push_constants(Slot, sizeof(constant));
// 			binding_layout_items[Index] = BindingLayoutItem::create_constant_buffer(Slot);
// 			binding_layout_items[Index] = BindingLayoutItem::create_structured_buffer_srv(Slot);
// 			binding_layout_items[Index] = BindingLayoutItem::create_structured_buffer_uav(Slot);
// 			binding_layout_items[Index] = BindingLayoutItem::create_raw_buffer_srv(Slot);
// 			binding_layout_items[Index] = BindingLayoutItem::create_raw_buffer_uav(Slot);
// 			binding_layout_items[Index] = BindingLayoutItem::create_typed_buffer_srv(Slot);
// 			binding_layout_items[Index] = BindingLayoutItem::create_typed_buffer_uav(Slot);
// 			binding_layout_items[Index] = BindingLayoutItem::create_texture_srv(Slot);
// 			binding_layout_items[Index] = BindingLayoutItem::create_texture_uav(Slot);
// 			binding_layout_items[Index] = BindingLayoutItem::create_sampler(Slot);
// 			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
// 				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
// 			)));
// 		}

// 		// Shader.
// 		{
// 			ShaderCompileDesc cs_compile_desc;
// 			cs_compile_desc.shader_name = ".slang";
// 			cs_compile_desc.entry_point = "compute_shader";
// 			cs_compile_desc.target = ShaderTarget::Compute;
// 			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
// 			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
// 			ShaderData cs_data = shader_compile::compile_shader(cs_compile_desc);

// 			ShaderDesc cs_desc;
// 			cs_desc.entry = "compute_shader";
// 			cs_desc.shader_type = ShaderType::Compute;
// 			ReturnIfFalse(_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));
// 		}

// 		// Pipeline.
// 		{
// 			ComputePipelineDesc pipeline_desc;
// 			pipeline_desc.compute_shader = _cs.get();
// 			pipeline_desc.binding_layouts.push_back(_binding_layout.get());
// 			ReturnIfFalse(_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));
// 		}


// 		// Buffer.
// 		{
// 			ReturnIfFalse(_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
// 				BufferDesc::create_(
// 					byte_size, 
// 					"Buffer"
// 				)
// 			)));
// 		}

// 		// Texture.
// 		{
// 			ReturnIfFalse(_texture = std::shared_ptr<TextureInterface>(device->create_texture(
// 				TextureDesc::create_(
// 					CLIENT_WIDTH,
// 					CLIENT_HEIGHT,
// 					Format::RGBA32_FLOAT,
// 					"Texture"
// 				)
// 			)));
// 			cache->collect(_texture, ResourceType::Texture);
// 		}
 
// 		// Binding Set.
// 		{
// 			BindingSetItemArray binding_set_items(N);
// 			binding_set_items[Index] = BindingSetItem::create_push_constants(Slot, sizeof(constant));
// 			binding_set_items[Index] = BindingSetItem::create_constant_buffer(Slot, _buffer);
// 			binding_set_items[Index] = BindingSetItem::create_structured_buffer_srv(Slot, _buffer);
// 			binding_set_items[Index] = BindingSetItem::create_structured_buffer_uav(Slot, _buffer);
// 			binding_set_items[Index] = BindingSetItem::create_raw_buffer_srv(Slot, _buffer);
// 			binding_set_items[Index] = BindingSetItem::create_raw_buffer_uav(Slot, _buffer);
// 			binding_set_items[Index] = BindingSetItem::create_typed_buffer_srv(Slot, _buffer);
// 			binding_set_items[Index] = BindingSetItem::create_typed_buffer_uav(Slot, _buffer);
// 			binding_set_items[Index] = BindingSetItem::create_texture_srv(Slot, _texture);
// 			binding_set_items[Index] = BindingSetItem::create_texture_uav(Slot, _texture);
// 			binding_set_items[Index] = BindingSetItem::create_sampler(Slot, sampler);
//             ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
//                 BindingSetDesc{ .binding_items = binding_set_items },
//                 _binding_layout.get()
//             )));
// 		}

// 		// Compute state.
// 		{
// 			_compute_state.binding_sets.push_back(_binding_set.get());
// 			_compute_state.pipeline = _pipeline.get();
// 		}
 
// 		return true;
// 	}

// 	bool Pass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
// 	{
// 		ReturnIfFalse(cmdlist->open());

// 		uint2 thread_group_num = {
// 			static_cast<uint32_t>((align(_texture_resolution.x, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
// 			static_cast<uint32_t>((align(_texture_resolution.y, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
// 		};

// 		ReturnIfFalse(cmdlist->set_compute_state(_compute_state));
// 		ReturnIfFalse(cmdlist->dispatch(thread_group_num.x, thread_group_num.y));

// 		ReturnIfFalse(cmdlist->close());
// 	}
// }






