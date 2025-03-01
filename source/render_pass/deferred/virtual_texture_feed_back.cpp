
#include "virtual_texture_feed_back.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/scene.h"
#include "../../scene/light.h"


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
 
	bool VirtualTextureFeedBackPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		cache->get_world()->get_global_entity()->get_component<event::AddModel>()->add_event(
			[this]() -> bool
			{
				_resource_writed = false;	
				return true;
			}
		);

		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(6);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::VirtualTextureFeedBackPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_texture_uav(0);
			binding_layout_items[5] = BindingLayoutItem::create_structured_buffer_uav(1);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "deferred/virtual_texture_feed_back_cs.hlsl";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			cs_compile_desc.defines.push_back("VT_FEED_BACK_SCALE_FACTOR=" + std::to_string(VT_FEED_BACK_SCALE_FACTOR));
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


		// Buffer.
		{
			uint32_t feed_back_buffer_element_num = 
				(CLIENT_WIDTH / VT_FEED_BACK_SCALE_FACTOR) * (CLIENT_HEIGHT / VT_FEED_BACK_SCALE_FACTOR);

			ReturnIfFalse(_vt_feed_back_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_read_write_structured_buffer(
					sizeof(uint3) * feed_back_buffer_element_num, 
					sizeof(uint3),
					"vt_feed_back_buffer"
				)
			)));
			cache->collect(_vt_feed_back_buffer, ResourceType::Buffer);	

			ReturnIfFalse(_vt_feed_back_read_back_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_read_back_buffer(
					sizeof(uint3) * feed_back_buffer_element_num, 
					"vt_feed_back_read_back_buffer"
				)
			)));
			cache->collect(_vt_feed_back_read_back_buffer, ResourceType::Buffer);
		}

		// Texture.
		{
			ReturnIfFalse(_vt_page_uv_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RG32_UINT,
					"vt_page_uv_texture"
				)
			)));
			cache->collect(_vt_page_uv_texture, ResourceType::Texture);
		}
        
		// Binding Set.
		{
			_binding_set_items.resize(6);
			_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::VirtualTextureFeedBackPassConstant));
			_binding_set_items[2] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require("geometry_uv_miplevel_id_texture")));
			_binding_set_items[3] = BindingSetItem::create_texture_srv(2, check_cast<TextureInterface>(cache->require("world_position_view_depth_texture")));
			_binding_set_items[4] = BindingSetItem::create_texture_uav(0, _vt_page_uv_texture);
			_binding_set_items[5] = BindingSetItem::create_structured_buffer_uav(1, _vt_feed_back_buffer);
		}

		// Compute state.
		{
			_compute_state.binding_sets.resize(1);
			_compute_state.pipeline = _pipeline.get();
		}
 
		return true;
	}

	bool VirtualTextureFeedBackPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

        if (SceneSystem::loaded_submesh_count != 0)
		{
			if (!_resource_writed)
			{
				_binding_set_items[1] = BindingSetItem::create_structured_buffer_srv(0, check_cast<BufferInterface>(cache->require("geometry_constant_buffer")));
				ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(cmdlist->get_deivce()->create_binding_set(
					BindingSetDesc{ .binding_items = _binding_set_items },
					_binding_layout
				)));

				_compute_state.binding_sets[0] = _binding_set.get();
				_resource_writed = true;
			}

			_pass_constant.shadow_view_proj = 
				cache->get_world()->get_global_entity()->get_component<DirectionalLight>()->get_view_proj();

			uint2 thread_group_num = {
				static_cast<uint32_t>((align(CLIENT_WIDTH, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
				static_cast<uint32_t>((align(CLIENT_HEIGHT, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
			};

			cmdlist->clear_buffer_uint(_vt_feed_back_buffer.get(), BufferRange{ 0, _vt_feed_back_buffer->get_desc().byte_size }, INVALID_SIZE_32);
			cmdlist->clear_texture_uint(_vt_page_uv_texture.get(), TextureSubresourceSet{}, INVALID_SIZE_32);
			
			ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constant));
			
			
			uint32_t feed_back_buffer_element_num = 
				(CLIENT_WIDTH / VT_FEED_BACK_SCALE_FACTOR) * (CLIENT_HEIGHT / VT_FEED_BACK_SCALE_FACTOR);
	
			cmdlist->copy_buffer(
				_vt_feed_back_read_back_buffer.get(), 
				0, 
				_vt_feed_back_buffer.get(), 
				0, 
				_vt_feed_back_buffer->get_desc().byte_size
			);
		}

		ReturnIfFalse(cmdlist->close());
        return true;
	}
}