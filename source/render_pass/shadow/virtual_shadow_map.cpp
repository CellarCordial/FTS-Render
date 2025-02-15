
#include "virtual_shadow_map.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/camera.h"
#include <memory>

namespace fantasy
{
	bool VirtualShadowMapPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(6);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::VirtualShadowMapPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_structured_buffer_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_structured_buffer_srv(3);
			binding_layout_items[5] = BindingLayoutItem::create_structured_buffer_srv(4);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = ".slang";
			shader_compile_desc.entry_point = "vertex_shader";
			shader_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = ".slang";
			shader_compile_desc.entry_point = "pixel_shader";
			shader_compile_desc.target = ShaderTarget::Pixel;
			ShaderData ps_data = compile_shader(shader_compile_desc);

			ShaderDesc vs_desc;
			vs_desc.entry = "vertex_shader";
			vs_desc.shader_type = ShaderType::Vertex;
			ReturnIfFalse(_vs = std::unique_ptr<Shader>(create_shader(vs_desc, vs_data.data(), vs_data.size())));

			ShaderDesc ps_desc;
			ps_desc.shader_type = ShaderType::Pixel;
			ps_desc.entry = "pixel_shader";
			ReturnIfFalse(_ps = std::unique_ptr<Shader>(create_shader(ps_desc, ps_data.data(), ps_data.size())));
		}

		// Texture.
		{
			ReturnIfFalse(_physical_shadow_map_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					PHYSICAL_SHADOW_RESOLUTION,
					PHYSICAL_SHADOW_RESOLUTION,
					Format::R32_FLOAT,
					"physical_shadow_map_texture"
				)
			)));
			cache->collect(_physical_shadow_map_texture, ResourceType::Texture);
		}
 
		// Frame Buffer.
		{
			FrameBufferDesc frame_buffer_desc;
			ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(frame_buffer_desc)));
		}
 
		// Pipeline.
		{
			GraphicsPipelineDesc pipeline_desc;
			pipeline_desc.vertex_shader = _vs;
			pipeline_desc.pixel_shader = _ps;
			pipeline_desc.binding_layouts.push_back(_binding_layout);
			ReturnIfFalse(_pipeline = std::unique_ptr<GraphicsPipelineInterface>(
				device->create_graphics_pipeline(pipeline_desc, _frame_buffer.get())
			));
		}

		// Binding Set.
		{
			_binding_set_items.resize(6);
			_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::VirtualShadowMapPassConstant));
			_binding_set_items[2] = BindingSetItem::create_structured_buffer_srv(1, check_cast<BufferInterface>(cache->require("virtual_shadow_visible_cluster_id_buffer")));
		}

		// Graphics state.
		{
			_graphics_state.pipeline = _pipeline.get();
			_graphics_state.frame_buffer = _frame_buffer.get();
			_graphics_state.viewport_state = ViewportState::create_default_viewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

		return true;
	}

	bool VirtualShadowMapPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		if (!_resource_writed)
		{
            DeviceInterface* device = cmdlist->get_deivce();
			_binding_set_items[1] = BindingSetItem::create_structured_buffer_srv(0, check_cast<BufferInterface>(cache->require("geometry_constant_buffer")));
			_binding_set_items[3] = BindingSetItem::create_structured_buffer_srv(2, check_cast<BufferInterface>(cache->require("cluster_buffer")));
			_binding_set_items[4] = BindingSetItem::create_structured_buffer_srv(3, check_cast<BufferInterface>(cache->require("cluster_vertex_buffer")));
			_binding_set_items[5] = BindingSetItem::create_structured_buffer_srv(4, check_cast<BufferInterface>(cache->require("cluster_triangle_buffer")));
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = _binding_set_items },
				_binding_layout
			)));
			_graphics_state.binding_sets.push_back(_binding_set.get());
			_resource_writed = true;
		}

		Camera* camera = cache->get_world()->get_global_entity()->get_component<Camera>();
		_pass_constant.view_matrix = camera->view_matrix;
		_pass_constant.view_proj = camera->get_view_proj();

		ReturnIfFalse(cmdlist->open());
		ReturnIfFalse(cmdlist->draw_indexed_indirect(_graphics_state, 0, 1, &_pass_constant));
		ReturnIfFalse(cmdlist->close());
		return true;
	}
}
