#ifndef RENDER_PASS_H
#define RENDER_PASS_H
 
#include "../../render_graph/render_pass.h"
#include "../../core/math/matrix.h"
#include "../../scene/geometry.h"
 
namespace fantasy
{
	namespace constant
	{
		struct PassConstant
		{

		};
	}

	class Pass : public RenderPassInterface
	{
	public:
		Pass() { type = RenderPassType::Graphics; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		constant::PassConstant _pass_constant;

		std::shared_ptr<BufferInterface> _buffer;
		std::shared_ptr<TextureInterface> _texture;
		
		std::unique_ptr<BindingLayoutInterface> _binding_layout;
		std::unique_ptr<InputLayoutInterface> _input_layout;

		std::unique_ptr<Shader> _vs;
		std::unique_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		GraphicsState _graphics_state;
        DrawArguments _draw_arguments;
	};

}
 
#endif



#include ".h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"

namespace fantasy
{
	bool Pass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(N);
			binding_layout_items[Index] = BindingLayoutItem::create_push_constants(Slot, sizeof(constant));
			binding_layout_items[Index] = BindingLayoutItem::create_constant_buffer(Slot);
			binding_layout_items[Index] = BindingLayoutItem::create_structured_buffer_srv(Slot);
			binding_layout_items[Index] = BindingLayoutItem::create_structured_buffer_uav(Slot);
			binding_layout_items[Index] = BindingLayoutItem::create_raw_buffer_srv(Slot);
			binding_layout_items[Index] = BindingLayoutItem::create_raw_buffer_uav(Slot);
			binding_layout_items[Index] = BindingLayoutItem::create_typed_buffer_srv(Slot);
			binding_layout_items[Index] = BindingLayoutItem::create_typed_buffer_uav(Slot);
			binding_layout_items[Index] = BindingLayoutItem::create_texture_srv(Slot);
			binding_layout_items[Index] = BindingLayoutItem::create_texture_uav(Slot);
			binding_layout_items[Index] = BindingLayoutItem::create_sampler(Slot);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Input Layout.
		{
			VertexAttributeDescArray vertex_attribute_desc(4);
			vertex_attribute_desc[0].name = "POSITION";
			vertex_attribute_desc[0].format = Format::RGB32_FLOAT;
			vertex_attribute_desc[0].offset = offsetof(Vertex, position);
			vertex_attribute_desc[0].element_stride = sizeof(Vertex);
			vertex_attribute_desc[1].name = "NORMAL";
			vertex_attribute_desc[1].format = Format::RGB32_FLOAT;
			vertex_attribute_desc[1].offset = offsetof(Vertex, normal);
			vertex_attribute_desc[1].element_stride = sizeof(Vertex);
			vertex_attribute_desc[2].name = "TANGENT";
			vertex_attribute_desc[2].format = Format::RGB32_FLOAT;
			vertex_attribute_desc[2].offset = offsetof(Vertex, tangent);
			vertex_attribute_desc[2].element_stride = sizeof(Vertex);
			vertex_attribute_desc[3].name = "TEXCOORD";
			vertex_attribute_desc[3].format = Format::RG32_FLOAT;
			vertex_attribute_desc[3].offset = offsetof(Vertex, uv);
			vertex_attribute_desc[3].element_stride = sizeof(Vertex);
			ReturnIfFalse(_input_layout = std::unique_ptr<InputLayoutInterface>(device->create_input_layout(
				vertex_attribute_desc.data(),
				vertex_attribute_desc.size(),
				nullptr
			)));
		}


		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = ".slang";
			shader_compile_desc.entry_point = "vertex_shader";
			shader_compile_desc.target = ShaderTarget::Vertex;
			shader_compile_desc.defines.push_back("Define=" + std::to_string(Define));
			ShaderData vs_data = shader_compile::compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = ".slang";
			shader_compile_desc.entry_point = "pixel_shader";
			shader_compile_desc.target = ShaderTarget::Pixel;
			shader_compile_desc.defines.push_back("Define=" + std::to_string(Define));
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
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
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

	bool Pass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
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

