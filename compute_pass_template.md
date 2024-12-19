

/*--------------------------------------- Compute Pass -----------------------------------------------*/

#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "../../render_graph/render_graph.h"
#include <memory>

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
		Pass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		bool _resource_writed = false;
		constant::PassConstant _pass_constant;

		std::shared_ptr<BufferInterface> _buffer;
		std::shared_ptr<TextureInterface> _texture;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
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

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = ".slang";
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
            ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
                BindingSetDesc{ .binding_items = binding_set_items },
                _binding_layout.get()
            )));
		}

		// Compute state.
		{
			_compute_state.binding_sets.push_back(_binding_set.Get());
			_compute_state.pipeline = _pipeline.Get();
		}
 
		return true;
	}

	bool Pass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
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
			static_cast<uint32_t>((Align(OutputTextureRes.x, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
			static_cast<uint32_t>((Align(OutputTextureRes.y, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
		};

		ReturnIfFalse(cmdlist->set_compute_state(ComputeState));
		ReturnIfFalse(cmdlist->dispatch(thread_group_num.x, thread_group_num.y));

		ReturnIfFalse(cmdlist->close());
		return true;
	}
}






