/*--------------------------------------- Graphics Pass -----------------------------------------------*/

#ifndef RENDER_PASS_H
#define RENDER_PASS_H
 
#include "../../../RenderGraph/RenderGraph.h"
#include "../../../Core/ComCli.h"
#include "../../../gui/GuiPanel.h"
 
namespace fantasy
{
	namespace constant
	{
		struct PassConstant
		{

		};
	}

	class FPass : public RenderPassInterface
	{
	public:
		FPass() { type = RenderPassType::Graphics; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		bool _resource_writed = false;
		constant::PassConstant _pass_constant;

		TComPtr<BufferInterface> _buffer;
		TComPtr<TextureInterface> m_pTexture;
		
		TComPtr<BindingLayoutInterface> _binding_layout;
		TComPtr<InputLayoutInterface> _input_layout;

		TComPtr<Shader> _vs;
		TComPtr<Shader> _ps;

		TComPtr<FrameBufferInterface> _frame_buffer;
		TComPtr<GraphicsPipelineInterface> _pipeline;

		TComPtr<BindingSetInterface> _binding_set;
		FGraphicsState _graphics_state;
	};

}
 
#endif



#include "###RenderPass.h"
#include "../../../Shader/ShaderCompiler.h"

namespace fantasy
{
	bool compile(DeviceInterface* device, RenderResourceCache* cache)
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
			ReturnIfFalse(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items },
				IID_IBindingLayout,
				PPV_ARG(_binding_layout.GetAddressOf())
			));
		}

		// Input Layout.
		{
			VertexAttributeDescArray VertexAttriDescs(4);
			VertexAttriDescs[0].name = "POSITION";
			VertexAttriDescs[0].Format = Format::RGB32_FLOAT;
			VertexAttriDescs[0].offset = offsetof(Vertex, position);
			VertexAttriDescs[0].element_stride = sizeof(Vertex);
			VertexAttriDescs[1].name = "NORMAL";
			VertexAttriDescs[1].Format = Format::RGB32_FLOAT;
			VertexAttriDescs[1].offset = offsetof(Vertex, normal);
			VertexAttriDescs[1].element_stride = sizeof(Vertex);
			VertexAttriDescs[2].name = "TANGENT";
			VertexAttriDescs[2].Format = Format::RGB32_FLOAT;
			VertexAttriDescs[2].offset = offsetof(Vertex, tangent);
			VertexAttriDescs[2].element_stride = sizeof(Vertex);
			VertexAttriDescs[3].name = "TEXCOORD";
			VertexAttriDescs[3].Format = Format::RG32_FLOAT;
			VertexAttriDescs[3].offset = offsetof(Vertex, uv);
			VertexAttriDescs[3].element_stride = sizeof(Vertex);
			ReturnIfFalse(device->create_input_layout(
				VertexAttriDescs.data(), 
				VertexAttriDescs.size(), 
				nullptr, 
				IID_IInputLayout, 
				PPV_ARG(_input_layout.GetAddressOf())
			));
		}


		// Shader.
		{
			ShaderCompileDesc ShaderCompileDesc;
			ShaderCompileDesc.shader_name = ".hlsl";
			ShaderCompileDesc.entry_point = "vertex_shader";
			ShaderCompileDesc.target = ShaderTarget::Vertex;
			ShaderCompileDesc.defines.push_back("Define=" + std::to_string(Define));
			ShaderData vs_data = shader_compile::compile_shader(ShaderCompileDesc);
			ShaderCompileDesc.entry_point = "pixel_shader";
			ShaderCompileDesc.target = ShaderTarget::Pixel;
			ShaderCompileDesc.defines.push_back("Define=" + std::to_string(Define));
			ShaderData ps_data = shader_compile::compile_shader(ShaderCompileDesc);

			ShaderDesc vs_desc;
			vs_desc.entry = "vertex_shader";
			vs_desc.ShaderType = ShaderType::Vertex;
			ReturnIfFalse(device->create_shader(vs_desc, vs_data.data(), vs_data.size(), IID_IShader, PPV_ARG(_vs.GetAddressOf())));

			ShaderDesc ps_desc;
			ps_desc.ShaderType = ShaderType::Pixel;
			ps_desc.entry = "pixel_shader";
			ReturnIfFalse(device->create_shader(ps_desc, ps_data.data(), ps_data.size(), IID_IShader, PPV_ARG(_ps.GetAddressOf())));
		}

		// Buffer.
		{
			ReturnIfFalse(device->create_buffer(
				BufferDesc::create_constant(sizeof(constant)),
				IID_IBuffer,
				PPV_ARG(pPassConstantBuffer.GetAddressOf())
			));
			ReturnIfFalse(device->create_buffer(
				BufferDesc::create_vertex(byte_size, "VertexBuffer"),
				IID_IBuffer,
				PPV_ARG(vertex_buffer.GetAddressOf())
			));
			ReturnIfFalse(device->create_buffer(
				BufferDesc::create_index(byte_size, "IndexBuffer"),
				IID_IBuffer,
				PPV_ARG(index_buffer.GetAddressOf())
			));
			ReturnIfFalse(device->create_buffer(
				BufferDesc::create_structured(byte_size, dwStride, "StructuredBuffer"),
				IID_IBuffer,
				PPV_ARG(pStructuredBuffer.GetAddressOf())
			));
			ReturnIfFalse(device->create_buffer(
				BufferDesc::create_read_back(byte_size, "ReadBackBuffer"),
				IID_IBuffer,
				PPV_ARG(pReadBackBuffer.GetAddressOf())
			));
		}

		// Texture.
		{
			ReturnIfFalse(device->create_texture(
				TextureDesc::create_shader_resource(width, height, Format, "ShaderResourceTexture"),
				IID_ITexture,
				PPV_ARG(pShaderResourceTexture.GetAddressOf())
			));
			ReturnIfFalse(device->create_texture(
				TextureDesc::create_shader_resource(width, height, depth, Format, "ShaderResourceTexture3D"),
				IID_ITexture,
				PPV_ARG(pShaderResourceTexture3D.GetAddressOf())
			));
			ReturnIfFalse(device->create_texture(
				TextureDesc::create_render_target(width, height, Format, "RenderTargetTexture"),
				IID_ITexture,
				PPV_ARG(pRenderTargetTexture.GetAddressOf())
			));
			ReturnIfFalse(device->create_texture(
				TextureDesc::create_depth(width, height, Format, "DepthTexture"),
				IID_ITexture,
				PPV_ARG(pDepthTexture.GetAddressOf())
			));
			ReturnIfFalse(device->create_texture(
				TextureDesc::create_read_write(width, height, depth, Format, "ReadWriteTexture3D"),
				IID_ITexture,
				PPV_ARG(pReadWriteTexture3D.GetAddressOf())
			));
			ReturnIfFalse(device->create_texture(
				TextureDesc::create_read_back(width, height, Format, "ReadBackTexture"),
				IID_ITexture,
				PPV_ARG(pReadBackTexture.GetAddressOf())
			));
		}
 
		// Frame Buffer.
		{
			FrameBufferDesc FrameBufferDesc;
			FrameBufferDesc.color_attachments.push_back(FrameBufferAttachment::create_attachment(pRenderTargetTexture));
			FrameBufferDesc.color_attachments.push_back(FrameBufferAttachment::create_attachment(pRenderTargetTexture));
			FrameBufferDesc.color_attachments.push_back(FrameBufferAttachment::create_attachment(pRenderTargetTexture));
			FrameBufferDesc.depth_stencil_attachment = FrameBufferAttachment::create_attachment(pDepthStencilTexture);
			ReturnIfFalse(device->create_frame_buffer(FrameBufferDesc, IID_IFrameBuffer, PPV_ARG(_frame_buffer.GetAddressOf())));
		}
 
		// Pipeline.
		{
			GraphicsPipelineDesc PipelineDesc;
			PipelineDesc.vertex_shader = _vs.Get();
			PipelineDesc.pixel_shader = _ps.Get();
			PipelineDesc.input_layout = _input_layout.Get();
			PipelineDesc.binding_layouts.push_back(_binding_layout.Get());
			ReturnIfFalse(device->create_graphics_pipeline(
				PipelineDesc,
				_frame_buffer.Get(),
				IID_IGraphicsPipeline,
				PPV_ARG(_pipeline.GetAddressOf())
			));
		}

		// Binding Set.
		{
			SamplerInterface* pLinearClampSampler, * pPointClampSampler, * pLinearWarpSampler, * pPointWrapSampler;
			ReturnIfFalse(cache->require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));
			ReturnIfFalse(cache->require("PointClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointClampSampler)));
			ReturnIfFalse(cache->require("LinearWarpSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearWarpSampler)));
			ReturnIfFalse(cache->require("PointWrapSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointWrapSampler)));
 
			BindingSetItemArray binding_set_items(N);
			binding_set_items[Index] = BindingSetItem::create_push_constants(Slot, sizeof(constant));
			binding_set_items[Index] = BindingSetItem::create_constant_buffer(Slot, pPassConstantBuffer);
			binding_set_items[Index] = BindingSetItem::create_structured_buffer_srv(Slot, pShaderResourceBuffer);
			binding_set_items[Index] = BindingSetItem::create_structured_buffer_uav(Slot, pUnorderedAccessBuffer);
			binding_set_items[Index] = BindingSetItem::create_raw_buffer_srv(Slot, pShaderResourceRawBuffer);
			binding_set_items[Index] = BindingSetItem::create_raw_buffer_uav(Slot, pUnorderedAccessRawBuffer);
			binding_set_items[Index] = BindingSetItem::create_typed_buffer_srv(Slot, pShaderResourceTypedBuffer, pShaderResourceTypedBuffer->get_desc().Format);
			binding_set_items[Index] = BindingSetItem::create_typed_buffer_uav(Slot, pUnorderedAccessTypedBuffer, pUnorderedAccessTypedBuffer->get_desc().Format);
			binding_set_items[Index] = BindingSetItem::create_texture_srv(Slot, pShaderResourceTexture, pShaderResourceTexture->get_desc().Format);
			binding_set_items[Index] = BindingSetItem::create_texture_uav(Slot, pUnorderedAccessTexture, pUnorderedAccessTexture->get_desc().Format);
			binding_set_items[Index] = BindingSetItem::create_sampler(Slot, sampler.Get());
			ReturnIfFalse(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				binding_layout.Get(),
				IID_IBindingSet,
				PPV_ARG(_binding_set.GetAddressOf())
			));
		}

		// Graphics state.
		{
			_graphics_state.pipeline = _pipeline.Get();
			_graphics_state.frame_buffer = _frame_buffer.Get();
			_graphics_state.binding_sets.push_back(_binding_set.Get());
			_graphics_state.vertex_buffer_bindings.push_back(VertexBufferBinding{ .buffer = vertex_buffer });
			_graphics_state.index_buffer_binding = IndexBufferBinding{ .buffer = index_buffer, .Format = Format::R32_UINT };
			_graphics_state.ViewportState = ViewportState::create_default_viewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

 
 		gui::add(
			[this]()
			{
				bool bDirty = false;

				ImGui::SeparatorText("Render Pass");

				if (bDirty)
				{
					_resource_writed = false;
				}
			}
		);
		return true;
	}

	bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		// Update constant.
		{
			_pass_constant;
			ReturnIfFalse(cmdlist->write_buffer(_pass_constant_buffer.Get(), &_pass_constant, sizeof(constant::AerialLUTPassConstant)));
		}
 
		if (!_resource_writed)
		{
			ReturnIfFalse(cmdlist->write_buffer(vertex_buffer.Get(), vertices.data(), sizeof(Vertex) * vertices.size()));
			ReturnIfFalse(cmdlist->write_buffer(index_buffer.Get(), indices.data(), sizeof(Format::R16_UINT) * indices.size()));
			_resource_writed = true;
		}

		ReturnIfFalse(cmdlist->set_graphics_state(GraphicsState));
		ReturnIfFalse(cmdlist->set_push_constants(&PassConstant, sizeof(constant)));
		ReturnIfFalse(cmdlist->draw_indexed(DrawArgument));

		ReturnIfFalse(cmdlist->close());
		return true;
	}
}



/*--------------------------------------- Compute Pass -----------------------------------------------*/

#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "../../../RenderGraph/RenderGraph.h"
#include "../../../Core/ComCli.h"

namespace fantasy
{
	namespace constant
	{
		struct PassConstant
		{

		};
	}


	class FPass : public RenderPassInterface
	{
	public:
		FPass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		bool _resource_writed = false;
		constant::PassConstant _pass_constant;

		TComPtr<BufferInterface> _buffer;
		TComPtr<TextureInterface> m_pTexture;

		TComPtr<BindingLayoutInterface> _binding_layout;

		TComPtr<Shader> _cs;
		TComPtr<ComputePipelineInterface> _pipeline;

		TComPtr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}

#endif


#include "###RenderPass.h"
#include "../../../Shader/ShaderCompiler.h"
#include "../../../gui/GuiPanel.h"


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16 
#define THREAD_GROUP_SIZE_Y 16 
 
	bool compile(DeviceInterface* device, RenderResourceCache* cache)
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
			ReturnIfFalse(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items },
				IID_IBindingLayout,
				PPV_ARG(binding_layout.GetAddressOf())
			));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = ".hlsl";
			cs_compile_desc.entry_point = "compute_shader";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			ShaderData CSData = shader_compile::compile_shader(cs_compile_desc);

			ShaderDesc CSDesc;
			CSDesc.entry = "compute_shader";
			CSDesc.ShaderType = ShaderType::Compute;
			ReturnIfFalse(device->create_shader(CSDesc, CSData.data(), CSData.size(), IID_IShader, PPV_ARG(_cs.GetAddressOf())));
		}

		// Pipeline.
		{
			ComputePipelineDesc PipelineDesc;
			PipelineDesc.compute_shader = pCS.Get();
			PipelineDesc.binding_layouts.push_back(binding_layout.Get());
			ReturnIfFalse(device->create_compute_pipeline(PipelineDesc, IID_IComputePipeline, PPV_ARG(pipeline.GetAddressOf())));
		}


		// Buffer.
		{
			ReturnIfFalse(device->create_buffer(
				BufferDesc::create_constant(sizeof(constant)),
				IID_IBuffer,
				PPV_ARG(pPassConstantBuffer.GetAddressOf())
			));
			ReturnIfFalse(device->create_buffer(
				BufferDesc::create_structured(byte_size, dwStride, "StructuredBuffer"),
				IID_IBuffer,
				PPV_ARG(pStructuredBuffer.GetAddressOf())
			));
			ReturnIfFalse(device->create_buffer(
				BufferDesc::CreateRWStructured(byte_size, dwStride, "RWStructuredBuffer"),
				IID_IBuffer,
				PPV_ARG(pRWStructuredBuffer.GetAddressOf())
			));
			ReturnIfFalse(device->create_buffer(
				BufferDesc::create_read_back(byte_size, "ReadBackBuffer"),
				IID_IBuffer,
				PPV_ARG(pReadBackBuffer.GetAddressOf())
			));
		}

		// Texture.
		{
			ReturnIfFalse(device->create_texture(
				TextureDesc::create_shader_resource(width, height, Format, "ShaderResourceTexture"),
				IID_ITexture,
				PPV_ARG(pShaderResourceTexture.GetAddressOf())
			));
			ReturnIfFalse(device->create_texture(
				TextureDesc::create_shader_resource(width, height, depth, Format, "ShaderResourceTexture3D"),
				IID_ITexture,
				PPV_ARG(pShaderResourceTexture3D.GetAddressOf())
			));
			ReturnIfFalse(device->create_texture(
				TextureDesc::create_read_write(width, height, Format, "ReadWriteTexture"),
				IID_ITexture,
				PPV_ARG(pReadWriteTexture.GetAddressOf())
			));
			ReturnIfFalse(device->create_texture(
				TextureDesc::create_read_write(width, height, depth, Format, "ReadWriteTexture3D"),
				IID_ITexture,
				PPV_ARG(pReadWriteTexture3D.GetAddressOf())
			));
			ReturnIfFalse(device->create_texture(
				TextureDesc::create_read_back(width, height, Format, "ReadBackTexture"),
				IID_ITexture,
				PPV_ARG(pReadBackTexture.GetAddressOf())
			));
		}
 
		// Binding Set.
		{
			SamplerInterface* pLinearClampSampler, * pPointClampSampler, * pLinearWarpSampler, * pPointWrapSampler;
			ReturnIfFalse(cache->require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));
			ReturnIfFalse(cache->require("PointClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointClampSampler)));
			ReturnIfFalse(cache->require("LinearWarpSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearWarpSampler)));
			ReturnIfFalse(cache->require("PointWrapSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointWrapSampler)));
 
			BindingSetItemArray binding_set_items(N);
			binding_set_items[Index] = BindingSetItem::create_push_constants(Slot, sizeof(constant));
			binding_set_items[Index] = BindingSetItem::create_constant_buffer(Slot, pPassConstantBuffer);
			binding_set_items[Index] = BindingSetItem::create_structured_buffer_srv(Slot, pShaderResourceBuffer);
			binding_set_items[Index] = BindingSetItem::create_structured_buffer_uav(Slot, pUnorderedAccessBuffer);
			binding_set_items[Index] = BindingSetItem::create_raw_buffer_srv(Slot, pShaderResourceRawBuffer);
			binding_set_items[Index] = BindingSetItem::create_raw_buffer_uav(Slot, pUnorderedAccessRawBuffer);
			binding_set_items[Index] = BindingSetItem::create_typed_buffer_srv(Slot, pShaderResourceTypedBuffer, pShaderResourceTypedBuffer->get_desc().Format);
			binding_set_items[Index] = BindingSetItem::create_typed_buffer_uav(Slot, pUnorderedAccessTypedBuffer, pUnorderedAccessTypedBuffer->get_desc().Format);
			binding_set_items[Index] = BindingSetItem::create_texture_srv(Slot, pShaderResourceTexture, pShaderResourceTexture->get_desc().Format);
			binding_set_items[Index] = BindingSetItem::create_texture_uav(Slot, pUnorderedAccessTexture, pUnorderedAccessTexture->get_desc().Format);
			binding_set_items[Index] = BindingSetItem::create_sampler(Slot, sampler.Get());
			ReturnIfFalse(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout.Get(),
				IID_IBindingSet,
				PPV_ARG(_binding_set.GetAddressOf())
			));
		}

		// Compute state.
		{
			_compute_state.binding_sets.push_back(_binding_set.Get());
			_compute_state.pipeline = _pipeline.Get();
		}
 
 		gui::add(
			[this]()
			{
				bool bDirty = false;

				ImGui::SeparatorText("Render Pass");

				if (bDirty)
				{
					_resource_writed = false;
				}
			}
		);
 
		return true;
	}

	bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
      if ((type & RenderPassType::OnceFinished) != RenderPassType::OnceFinished)
		{
			ReturnIfFalse(cmdlist->open());

			// Update constant.
			{
				_pass_constant;
				ReturnIfFalse(cmdlist->write_buffer(_pass_constant_buffer.Get(), &_pass_constant, sizeof(constant::AerialLUTPassConstant)));
			}
 
			if (!_resource_writed)
			{
				_resource_writed = true;
			}


			Vector2I thread_group_num = {
				static_cast<uint32_t>((Align(OutputTextureRes.x, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X),
				static_cast<uint32_t>((Align(OutputTextureRes.y, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y),
					};

			ReturnIfFalse(cmdlist->set_compute_state(ComputeState));
			ReturnIfFalse(cmdlist->dispatch(thread_group_num.x, thread_group_num.y));

			ReturnIfFalse(cmdlist->close());
			return true;
		}
	}
}






