#include "sky_lut.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/light.h"
#include "../../scene/camera.h"
#include <memory>


namespace fantasy
{
#define SKY_LUT_RES	64

	bool SkyLUTPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(5);
			binding_layout_items[0] = BindingLayoutItem::create_volatile_constant_buffer(0);
			binding_layout_items[1] = BindingLayoutItem::create_constant_buffer(1);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[4] = BindingLayoutItem::create_sampler(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "full_screen_quad_vs.slang";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = shader_compile::compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = "atmosphere/sky_lut_ps.slang";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Pixel;
			ShaderData ps_data = shader_compile::compile_shader(shader_compile_desc);

			ShaderDesc vs_desc;
			vs_desc.entry = "main";
			vs_desc.shader_type = ShaderType::Vertex;
			ReturnIfFalse(_vs = std::unique_ptr<Shader>(create_shader(vs_desc, vs_data.data(), vs_data.size())));

			ShaderDesc ps_desc;
			ps_desc.shader_type = ShaderType::Pixel;
			ps_desc.entry = "main";
			ReturnIfFalse(_ps = std::unique_ptr<Shader>(create_shader(ps_desc, ps_data.data(), ps_data.size())));
		}

		// Buffer.
		{
			ReturnIfFalse(_pass_constant_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_constant_buffer(
					sizeof(constant::SkyLUTPassConstant),
					"sky_lut_pass_constant_buffer"
				)
			)));
		}
 
		// Texture.
		{
			ReturnIfFalse(_sky_lut_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target_texture(
					SKY_LUT_RES,
					SKY_LUT_RES,
					Format::RGBA32_FLOAT,
					"SkyLUTTexture"
				)
			)));
			cache->collect(_sky_lut_texture, ResourceType::Texture);
		}

		// Frame Buffer.
		{
			FrameBufferDesc frame_buffer_desc;
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_sky_lut_texture));
			ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(frame_buffer_desc)));
		}

		// Pipeline.
		{
			GraphicsPipelineDesc pipeline_desc;
			pipeline_desc.vertex_shader = _vs;
			pipeline_desc.pixel_shader = _ps;
			pipeline_desc.binding_layouts.push_back(_binding_layout);
			ReturnIfFalse(_pipeline = std::unique_ptr<GraphicsPipelineInterface>(device->create_graphics_pipeline(pipeline_desc, _frame_buffer.get())));
		}

		// Binding Set.
		{
			_multi_scattering_texture = check_cast<TextureInterface>(cache->require("multi_scattering_texture"));
			_transmittance_texture = check_cast<TextureInterface>(cache->require("transmittance_texture"));

			BindingSetItemArray binding_set_items(5);
			binding_set_items[0] = BindingSetItem::create_constant_buffer(0, check_cast<BufferInterface>(cache->require("atmosphere_properties_buffer")));
			binding_set_items[1] = BindingSetItem::create_constant_buffer(1, _pass_constant_buffer);
			binding_set_items[2] = BindingSetItem::create_texture_srv(0, _multi_scattering_texture);
			binding_set_items[3] = BindingSetItem::create_texture_srv(1, _transmittance_texture);
			binding_set_items[4] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout
			)));
		}

		// Graphics state.
		{
			_graphics_state.pipeline = _pipeline.get();
			_graphics_state.frame_buffer = _frame_buffer.get();
			_graphics_state.binding_sets.push_back(_binding_set.get());
			_graphics_state.viewport_state = ViewportState::create_default_viewport(SKY_LUT_RES, SKY_LUT_RES);
		}

		return true;
	}

	bool SkyLUTPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		// Update constant.
		{
			float* world_scale;
			ReturnIfFalse(cache->require_constants("WorldScale", reinterpret_cast<void**>(&world_scale)));

			ReturnIfFalse(cache->get_world()->each<Camera>(
				[this, world_scale](Entity* entity, Camera* _camera) -> bool
				{
					_pass_constant.camera_position = _camera->position * (*world_scale);
					return true;
				}
			));
			ReturnIfFalse(cache->get_world()->each<DirectionalLight>(
				[this](Entity* entity, DirectionalLight* pLight) -> bool
				{
					_pass_constant.sun_direction = pLight->direction;
					_pass_constant.sun_intensity = float3(pLight->intensity * pLight->color);
					return true;
				}
			));
			ReturnIfFalse(cmdlist->write_buffer(_pass_constant_buffer.get(), &_pass_constant, sizeof(constant::SkyLUTPassConstant)));
		}

		ReturnIfFalse(cmdlist->draw(_graphics_state, DrawArguments{ .index_count = 6 }));

		ReturnIfFalse(cmdlist->close());

		return true;
	}

}