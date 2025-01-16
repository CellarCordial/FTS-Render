#include "virtual_gbuffer.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"

namespace fantasy
{
	bool VirtualGBufferPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(6);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::VirtualGBufferPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_structured_buffer_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_structured_buffer_srv(3);
			binding_layout_items[5] = BindingLayoutItem::create_structured_buffer_uav(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "deferred/virtual_gbuffer_vs.slang";
			shader_compile_desc.entry_point = "vertex_shader";
			shader_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = shader_compile::compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = "deferred/virtual_gbuffer_ps.slang";
			shader_compile_desc.entry_point = "pixel_shader";
			shader_compile_desc.target = ShaderTarget::Pixel;
			ShaderData ps_data = shader_compile::compile_shader(shader_compile_desc);

			ShaderDesc vs_desc;
			vs_desc.entry = "vertex_shader";
			vs_desc.shader_type = ShaderType::Vertex;
			ReturnIfFalse(_vs = std::unique_ptr<Shader>(create_shader(vs_desc, vs_data.data(), vs_data.size())));

			ShaderDesc ps_desc;
			ps_desc.shader_type = ShaderType::Pixel;
			ps_desc.entry = "pixel_shader";
			ReturnIfFalse(_ps = std::unique_ptr<Shader>(create_shader(ps_desc, ps_data.data(), ps_data.size())));
		}

		// Buffer.
		{
			ReturnIfFalse(_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_(
					byte_size, 
					"Buffer"
				)
			)));
		}

		// Texture.
		{
			ReturnIfFalse(_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_(
					width,
					height,
					Format::RGBA32_FLOAT,
					"Texture"
				)
			)));
			cache->collect(_texture, ResourceType::Texture);
		}
 
		// Frame Buffer.
		{
			FrameBufferDesc frame_buffer_desc;
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(pRenderTargetTexture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(pRenderTargetTexture));
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(pRenderTargetTexture));
			frame_buffer_desc.depth_stencil_attachment = FrameBufferAttachment::create_attachment(pDepthStencilTexture);
			ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(frame_buffer_desc)));
		}
 
		// Pipeline.
		{
			GraphicsPipelineDesc pipeline_desc;
			pipeline_desc.vertex_shader = _vs.get();
			pipeline_desc.pixel_shader = _ps.get();
			pipeline_desc.input_layout = _input_layout.get();
			pipeline_desc.binding_layouts.push_back(_binding_layout.get());
			ReturnIfFalse(_pipeline = std::unique_ptr<GraphicsPipelineInterface>(
				device->create_graphics_pipeline(pipeline_desc, _frame_buffer.get())
			));
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(N);
			binding_set_items[Index] = BindingSetItem::create_push_constants(Slot, sizeof(constant));
			binding_set_items[Index] = BindingSetItem::create_constant_buffer(Slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_structured_buffer_srv(Slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_structured_buffer_uav(Slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_raw_buffer_srv(Slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_raw_buffer_uav(Slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_typed_buffer_srv(Slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_typed_buffer_uav(Slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_texture_srv(Slot, _texture);
			binding_set_items[Index] = BindingSetItem::create_texture_uav(Slot, _texture);
			binding_set_items[Index] = BindingSetItem::create_sampler(Slot, sampler.Get());
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout.get()
			)));
		}

		// Graphics state.
		{
			_graphics_state.pipeline = _pipeline.get();
			_graphics_state.frame_buffer = _frame_buffer.get();
			_graphics_state.binding_sets.push_back(_binding_set.get());
			_graphics_state.index_buffer_binding = IndexBufferBinding{ .buffer = index_buffer };
			_graphics_state.vertex_buffer_bindings.push_back(VertexBufferBinding{ .buffer = vertex_buffer });
			_graphics_state.viewport_state = ViewportState::create_default_viewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

		return true;
	}

	bool VirtualGBufferPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		if (!_resource_writed)
		{
			ReturnIfFalse(cmdlist->write_buffer(vertex_buffer.Get(), vertices.data(), sizeof(Vertex) * vertices.size()));
			ReturnIfFalse(cmdlist->write_buffer(index_buffer.Get(), indices.data(), sizeof(Format::R16_UINT) * indices.size()));
			_resource_writed = true;
		}

		ReturnIfFalse(cmdlist->set_graphics_state(_graphics_state));
		ReturnIfFalse(cmdlist->set_push_constants(&_pass_constant, sizeof(constant)));
		ReturnIfFalse(cmdlist->draw_indexed(DrawArgument));

		ReturnIfFalse(cmdlist->close());
		return true;
	}
}

