#include "final_test.h"

#include "../shader/shader_compiler.h"
#include "../core/tools/check_cast.h"
#include "../gui/gui_panel.h"

#include "../render_pass/culling/mesh_cluster_culling.h"
#include "../render_pass/culling/hierarchical_zbuffer.h"
#include "../render_pass/culling/shadow_tile_culling.h"
#include "../render_pass/deferred/virtual_gbuffer.h"
#include "../render_pass/deferred/virtual_texture_update.h"
#include "../render_pass/deferred/mipmap_generation.h"
#include "../render_pass/shadow/virtual_shadow_map.h"
#include <memory>

namespace fantasy
{
	bool FinalTestPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(7);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::FinalTestPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[5] = BindingLayoutItem::create_texture_srv(4);
			// binding_layout_items[6] = BindingLayoutItem::create_texture_srv(5);
			binding_layout_items[6] = BindingLayoutItem::create_sampler(0);
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
			shader_compile_desc.shader_name = "test/final_test_ps.hlsl";
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

 
		// Frame Buffer.
		{
			FrameBufferDesc frame_buffer_desc;
			frame_buffer_desc.color_attachments.push_back(
                FrameBufferAttachment::create_attachment(check_cast<TextureInterface>(cache->require("final_texture")))
            );
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
			BindingSetItemArray binding_set_items(7);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::FinalTestPassConstant));
			binding_set_items[1] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("world_position_view_depth_texture")));
			binding_set_items[2] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require("world_space_normal_texture")));
			binding_set_items[3] = BindingSetItem::create_texture_srv(2, check_cast<TextureInterface>(cache->require("base_color_texture")));
			binding_set_items[4] = BindingSetItem::create_texture_srv(3, check_cast<TextureInterface>(cache->require("pbr_texture")));
			binding_set_items[5] = BindingSetItem::create_texture_srv(4, check_cast<TextureInterface>(cache->require("emissive_texture")));
			// binding_set_items[6] = BindingSetItem::create_texture_srv(5, check_cast<TextureInterface>(cache->require("shadow_map_texture")));
			binding_set_items[6] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_wrap_sampler")));
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
			_graphics_state.viewport_state = ViewportState::create_default_viewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

        gui::add(
            [this]()
            {
                if (ImGui::CollapsingHeader("Final Test"))
				{
                    const char* types[] = { "FinalGather", "World Position", "View Depth", "World Normal", "BaseColor", "Metallic", "Roughness", "Occlusion", "Emissive" };
                    ImGui::Combo("Show Type", &_pass_constant.show_type, types, IM_ARRAYSIZE(types));
                }
            }
        );

		return true;
	}

	bool FinalTestPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());
		ReturnIfFalse(cmdlist->draw(_graphics_state, DrawArguments::full_screen_quad(), &_pass_constant));
		ReturnIfFalse(cmdlist->close());
		return true;
	}

    RenderPassInterface* FinalTest::init_render_pass(RenderGraph* render_graph)
    {
		RenderPassInterface* mipmap_generation_pass = render_graph->add_pass(std::make_shared<MipmapGenerationPass>());

		RenderPassInterface* mesh_cluster_culling_pass = render_graph->add_pass(std::make_shared<MeshClusterCullingPass>());
		RenderPassInterface* hierarchical_zbuffer_pass = render_graph->add_pass(std::make_shared<HierarchicalZBufferPass>());
		RenderPassInterface* virtual_gbuffer_pass = render_graph->add_pass(std::make_shared<VirtualGBufferPass>());
		// RenderPassInterface* virtual_texture_update_pass = render_graph->add_pass(std::make_shared<VirtualTextureUpdatePass>());
		// RenderPassInterface* shadow_tile_culling_pass = render_graph->add_pass(std::make_shared<ShadowTileCullingPass>());
		// RenderPassInterface* virtual_shadow_map_pass = render_graph->add_pass(std::make_shared<VirtualShadowMapPass>());
		RenderPassInterface* test_pass = render_graph->add_pass(std::make_shared<FinalTestPass>());
		
		mesh_cluster_culling_pass->precede(virtual_gbuffer_pass);
		virtual_gbuffer_pass->precede(hierarchical_zbuffer_pass);
		// virtual_gbuffer_pass->precede(virtual_texture_update_pass);
		// virtual_texture_update_pass->precede(shadow_tile_culling_pass);
		// shadow_tile_culling_pass->precede(virtual_shadow_map_pass);
		hierarchical_zbuffer_pass->precede(test_pass);
		// virtual_shadow_map_pass->precede(test_pass);

		return test_pass;  
    }
}


int main()
{
	fantasy::FinalTest test(fantasy::GraphicsAPI::D3D12);
	if (!test.initialize() || !test.run()) return -1;
	return 0;
}