#ifndef RENDER_PASS_H
#define RENDER_PASS_H
 
#include "../../render_graph/render_pass.h"
#include "../../core/math/matrix.h"
 
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
		constant::PassConstant _pass_constant;

		std::shared_ptr<ray_tracing::AccelStructInterface> _top_level_accel_struct;
		std::shared_ptr<ray_tracing::AccelStructInterface> _bottom_level_accel_struct;
		
		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _ray_gen_shader;
		std::shared_ptr<Shader> _miss_shader;

		std::unique_ptr<ray_tracing::PipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ray_tracing::PipelineState _ray_tracing_state;
        ray_tracing::DispatchRaysArguments _dispatch_rays_arguments;
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
			binding_layout_items[Index] = BindingLayoutItem::create_push_constants(slot, sizeof(constant));
			binding_layout_items[Index] = BindingLayoutItem::create_constant_buffer(slot);
			binding_layout_items[Index] = BindingLayoutItem::create_structured_buffer_srv(slot);
			binding_layout_items[Index] = BindingLayoutItem::create_structured_buffer_uav(slot);
			binding_layout_items[Index] = BindingLayoutItem::create_raw_buffer_srv(slot);
			binding_layout_items[Index] = BindingLayoutItem::create_raw_buffer_uav(slot);
			binding_layout_items[Index] = BindingLayoutItem::create_typed_buffer_srv(slot);
			binding_layout_items[Index] = BindingLayoutItem::create_typed_buffer_uav(slot);
			binding_layout_items[Index] = BindingLayoutItem::create_texture_srv(slot);
			binding_layout_items[Index] = BindingLayoutItem::create_texture_uav(slot);
			binding_layout_items[Index] = BindingLayoutItem::create_sampler(slot);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = ".slang";
			shader_compile_desc.entry_point = "ray_generation_shader";
			ShaderData ray_gen_data = shader_compile::compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = ".slang";
			shader_compile_desc.entry_point = "miss_shader";
			ShaderData miss_data = shader_compile::compile_shader(shader_compile_desc);

			ShaderDesc ray_gen_desc;
			ray_gen_desc.entry = "ray_generation_shader";
			ray_gen_desc.shader_type = ShaderType::RayGeneration;
			ReturnIfFalse(_ray_gen_shader = std::unique_ptr<Shader>(create_shader(ray_gen_desc, ray_gen_data.data(), ray_gen_data.size())));

			ShaderDesc miss_desc;
			miss_desc.shader_type = ShaderType::Miss;
			miss_desc.entry = "miss_shader";
			ReturnIfFalse(_miss_shader = std::unique_ptr<Shader>(create_shader(miss_desc, miss_data.data(), miss_data.size())));
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
 
		// Pipeline & Shader Table.
		{
            ray_tracing::PipelineDesc pipeline_desc;
            pipeline_desc.max_payload_size = sizeof();
            pipeline_desc.global_binding_layouts.push_back(_binding_layout.get());
            pipeline_desc.shader_descs.push_back(ray_tracing::ShaderDesc{ .shader = _ray_gen_shader.get() });
            pipeline_desc.shader_descs.push_back(ray_tracing::ShaderDesc{ .shader = _miss_shader.get() });
            ReturnIfFalse(_pipeline = std::unique_ptr<ray_tracing::PipelineInterface>(device->create_ray_tracing_pipline(pipeline_desc)));
            ReturnIfFalse(_shader_table = std::unique_ptr<ray_tracing::ShaderTableInterface>(_pipeline->create_shader_table()));
            _shader_table->set_raygen_shader("ray_generation_shader");
            _shader_table->add_miss_shader("miss_shader");
		}


		// Binding Set.
		{
			BindingSetItemArray binding_set_items(N);
			binding_set_items[Index] = BindingSetItem::create_push_constants(slot, sizeof(constant));
			binding_set_items[Index] = BindingSetItem::create_constant_buffer(slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_structured_buffer_srv(slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_structured_buffer_uav(slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_raw_buffer_srv(slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_raw_buffer_uav(slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_typed_buffer_srv(slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_typed_buffer_uav(slot, _buffer);
			binding_set_items[Index] = BindingSetItem::create_texture_srv(slot, _texture);
			binding_set_items[Index] = BindingSetItem::create_texture_uav(slot, _texture);
			binding_set_items[Index] = BindingSetItem::create_sampler(slot, sampler.Get());
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout.get()
			)));
		}

		// Ray Tracing State.
		{
            _ray_tracing_state.binding_sets.push_back(_binding_set.get());
            _ray_tracing_state.shader_table = _shader_table.get();

            _dispatch_rays_arguments.width = CLIENT_WIDTH;
            _dispatch_rays_arguments.height = CLIENT_HEIGHT;
		}

		return true;
	}

	bool Pass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

        ReturnIfFalse(cmdlist->set_ray_tracing_state(_ray_tracing_state));
        ReturnIfFalse(cmdlist->set_push_constants(&_pass_constant, sizeof(constant)));
        ReturnIfFalse(cmdlist->dispatch_rays(_dispatch_rays_arguments));

		ReturnIfFalse(cmdlist->close());
		return true;
	}
}

