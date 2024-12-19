#include "test_restir.h"

#include "../shader/shader_compiler.h"
#include "../core/tools/check_cast.h"
#include "../gui/gui_panel.h"

namespace fantasy
{
	bool RestirTestPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(7);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::RestirTestPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[5] = BindingLayoutItem::create_texture_srv(4);
			binding_layout_items[6] = BindingLayoutItem::create_sampler(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "full_screen_quad_vs.slang";
			shader_compile_desc.entry_point = "vertex_shader";
			shader_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = shader_compile::compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = "test/restir_test_ps.slang";
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

 
		// Frame Buffer.
		{
			FrameBufferDesc frame_buffer_desc;
			frame_buffer_desc.color_attachments.push_back(
                FrameBufferAttachment::create_attachment(check_cast<TextureInterface>(cache->require("FinalTexture")))
            );
			ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(frame_buffer_desc)));
		}
 
		// Pipeline.
		{
			GraphicsPipelineDesc pipeline_desc;
			pipeline_desc.vertex_shader = _vs.get();
			pipeline_desc.pixel_shader = _ps.get();
			pipeline_desc.binding_layouts.push_back(_binding_layout.get());
			ReturnIfFalse(_pipeline = std::unique_ptr<GraphicsPipelineInterface>(
				device->create_graphics_pipeline(pipeline_desc, _frame_buffer.get())
			));
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(7);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::RestirTestPassConstant));
			binding_set_items[1] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("world_space_position_depth_texture")));
			binding_set_items[2] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require("normal_texture")));
			binding_set_items[3] = BindingSetItem::create_texture_srv(2, check_cast<TextureInterface>(cache->require("base_color_texture")));
			binding_set_items[4] = BindingSetItem::create_texture_srv(3, check_cast<TextureInterface>(cache->require("pbr_texture")));
			binding_set_items[5] = BindingSetItem::create_texture_srv(4, check_cast<TextureInterface>(cache->require("emissive_texture")));
			binding_set_items[6] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_wrap_sampler")));
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
			_graphics_state.viewport_state = ViewportState::create_default_viewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

        gui::add(
            [this]()
            {
                if (ImGui::CollapsingHeader("Restir Test"))
				{
                    const char* types[] = { "FinalGather", "Position", "Depth", "Normal", "BaseColor", "Metallic", "Roughness", "Occlusion", "Emissive" };
                    static int current_type_index = 0;
                    ImGui::Combo("Show Type", &current_type_index, types, IM_ARRAYSIZE(types));
                }
            }
        );

		return true;
	}

	bool RestirTestPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		ReturnIfFalse(cmdlist->set_graphics_state(_graphics_state));
		ReturnIfFalse(cmdlist->set_push_constants(&_pass_constant, sizeof(constant::RestirTestPassConstant)));
		ReturnIfFalse(cmdlist->draw(DrawArguments::full_screen_quad()));

		ReturnIfFalse(cmdlist->close());
		return true;
	}

    bool RestirTest::setup(RenderGraph* render_graph)
    {
        ReturnIfFalse(render_graph != nullptr);

		_gbuffer_pass = std::make_shared<GBufferPass>();
		_test_pass = std::make_shared<RestirTestPass>();

		render_graph->add_pass(_gbuffer_pass);
		render_graph->add_pass(_test_pass);

		return true;
    }

}

