#include "sky.h"
#include "../../scene/camera.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include <memory>

namespace fantasy
{
	bool SkyPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		_reverse_depth_texture = check_cast<TextureInterface>(cache->require("reverse_depth_texture"));
		
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(3);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::SkyPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_sampler(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "common/full_screen_quad_vs.hlsl";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = "atmosphere/sky_ps.hlsl";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Pixel;
			ShaderData ps_data = compile_shader(shader_compile_desc);

			ShaderDesc vs_desc;
			vs_desc.entry = "main";
			vs_desc.shader_type = ShaderType::Vertex;
			ReturnIfFalse(_vs = std::unique_ptr<Shader>(create_shader(vs_desc, vs_data.data(), vs_data.size())));

			ShaderDesc ps_desc;
			ps_desc.shader_type = ShaderType::Pixel;
			ps_desc.entry = "main";
			ReturnIfFalse(_ps = std::unique_ptr<Shader>(create_shader(ps_desc, ps_data.data(), ps_data.size())));
		}

		// Sampler.
		{
			SamplerDesc sampler_desc;	// U_Wrap VW_Clamp Linear
			sampler_desc.address_v = SamplerAddressMode::Clamp;
			sampler_desc.address_w = SamplerAddressMode::Clamp;
			ReturnIfFalse(_sampler = std::shared_ptr<SamplerInterface>(device->create_sampler(sampler_desc)));
		}

		// Frame Buffer.
		{
			FrameBufferDesc frame_buffer_desc;
			frame_buffer_desc.color_attachments.push_back(
				FrameBufferAttachment::create_attachment(check_cast<TextureInterface>(cache->require("final_texture")))
			);
			frame_buffer_desc.depth_stencil_attachment = FrameBufferAttachment::create_attachment(_reverse_depth_texture);
			ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(frame_buffer_desc)));
		}
		
		// Pipeline.
		{
			GraphicsPipelineDesc pipeline_desc;
			pipeline_desc.vertex_shader = _vs;
			pipeline_desc.pixel_shader = _ps;
			pipeline_desc.binding_layouts.push_back(_binding_layout);
			pipeline_desc.render_state.depth_stencil_state.enable_depth_test = true;
			pipeline_desc.render_state.depth_stencil_state.enable_depth_write = true;
			pipeline_desc.render_state.depth_stencil_state.depth_func = ComparisonFunc::Greater;
			ReturnIfFalse(_pipeline = std::unique_ptr<GraphicsPipelineInterface>(device->create_graphics_pipeline(pipeline_desc, _frame_buffer.get())));
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(3);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::SkyPassConstant));
			binding_set_items[1] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("sky_lut_texture")));
			binding_set_items[2] = BindingSetItem::create_sampler(0, _sampler);
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout
			)));
		}

		// Graphics state.
		{
			_graphics_state.binding_sets.push_back(_binding_set.get());
			_graphics_state.pipeline = _pipeline.get();
			_graphics_state.frame_buffer = _frame_buffer.get();
			_graphics_state.viewport_state = ViewportState::create_default_viewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

		return true;
	}

	bool SkyPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		auto frustum = cache->get_world()->get_global_entity()->get_component<Camera>()->get_frustum_directions();
		_pass_constant.frustum_a = frustum.A;
		_pass_constant.frustum_b = frustum.B;
		_pass_constant.frustum_c = frustum.C;
		_pass_constant.frustum_d = frustum.D;


		ReturnIfFalse(cmdlist->draw(
			_graphics_state, 
			DrawArguments{ .index_count = 6 },
			&_pass_constant
		));

		ReturnIfFalse(cmdlist->close());

		return true;
	}
}