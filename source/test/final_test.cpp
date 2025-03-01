#include "final_test.h"

#include "../shader/shader_compiler.h"
#include "../core/tools/check_cast.h"
#include "../gui/gui_panel.h"

#include "../render_pass/culling/mesh_cluster_culling.h"
#include "../render_pass/culling/hierarchical_zbuffer.h"
#include "../render_pass/deferred/virtual_gbuffer.h"
#include "../render_pass/deferred/virtual_texture_update.h"
#include "../render_pass/shadow/virtual_shadow_map.h"
#include "../render_pass/deferred/mipmap_generation.h"
#include "../render_pass/deferred/virtual_texture_feed_back.h"

#include "../render_pass/atmosphere/multi_scattering_lut.h"
#include "../render_pass/atmosphere/transmittance_lut.h"
#include "../render_pass/atmosphere/sun_disk.h"
#include "../render_pass/atmosphere/sky_lut.h"
#include "../render_pass/atmosphere/sky.h"

namespace fantasy
{	
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16

	bool FinalTestPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{	
		_final_texture = check_cast<TextureInterface>(cache->require("final_texture"));
		_base_color_texture = check_cast<TextureInterface>(cache->require("base_color_texture"));

		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(8);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::FinalTestPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_uav(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[5] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[6] = BindingLayoutItem::create_texture_srv(4);
			binding_layout_items[7] = BindingLayoutItem::create_texture_srv(5);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "test/final_test_cs.hlsl";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Compute;
			shader_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			shader_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			ShaderData cs_data = compile_shader(shader_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.entry = "main";
			cs_desc.shader_type = ShaderType::Compute;
			ReturnIfFalse(_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));
		}
 
		// Pipeline.
		{
			ComputePipelineDesc pipeline_desc;
			pipeline_desc.compute_shader = _cs;
			pipeline_desc.binding_layouts.push_back(_binding_layout);
			ReturnIfFalse(_pipeline = std::unique_ptr<ComputePipelineInterface>(
				device->create_compute_pipeline(pipeline_desc)
			));
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(8);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::FinalTestPassConstant));
			binding_set_items[1] = BindingSetItem::create_texture_uav(0, _final_texture);
			binding_set_items[2] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("world_position_view_depth_texture")));
			binding_set_items[3] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require("world_space_normal_texture")));
			binding_set_items[4] = BindingSetItem::create_texture_srv(2, _base_color_texture);
			binding_set_items[5] = BindingSetItem::create_texture_srv(3, check_cast<TextureInterface>(cache->require("pbr_texture")));
			binding_set_items[6] = BindingSetItem::create_texture_srv(4, check_cast<TextureInterface>(cache->require("emissive_texture")));
			binding_set_items[7] = BindingSetItem::create_texture_srv(5, check_cast<TextureInterface>(cache->require("virtual_mesh_visual_texture")));
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout
			)));
		}

		// Graphics state.
		{
			_compute_state.pipeline = _pipeline.get();
			_compute_state.binding_sets.push_back(_binding_set.get());
		}

        gui::add(
            [this]()
            {
                if (ImGui::CollapsingHeader("Final Test"))
				{
                    const char* types[] = { 
						"FinalGather", 
						"World Position", 
						"View Depth", 
						"World Normal", 
						"BaseColor", 
						"Metallic", 
						"Roughness", 
						"Occlusion", 
						"Emissive", 
						"Virtual Mesh" 
					};
                    ImGui::Combo("Show Type", &_pass_constant.show_type, types, IM_ARRAYSIZE(types));
                }
            }
        );

		return true;
	}

	bool FinalTestPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());
		cmdlist->copy_texture(_final_texture.get(), TextureSlice{}, _base_color_texture.get(), TextureSlice{});
		uint2 thread_group_num = {
			static_cast<uint32_t>((align(CLIENT_WIDTH, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
			static_cast<uint32_t>((align(CLIENT_HEIGHT, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
		};
		ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constant));
		ReturnIfFalse(cmdlist->close());
		return true;
	}

    RenderPassInterface* FinalTest::init_render_pass(RenderGraph* render_graph)
    {
		RenderPassInterface* transmittance_lut_pass = render_graph->add_pass(std::make_shared<TransmittanceLUTPass>());
		RenderPassInterface* multi_scattering_lut_pass = render_graph->add_pass(std::make_shared<MultiScatteringLUTPass>());
		RenderPassInterface* sky_lut_pass = render_graph->add_pass(std::make_shared<SkyLUTPass>());
		RenderPassInterface* sky_pass = render_graph->add_pass(std::make_shared<SkyPass>());
		RenderPassInterface* sun_disk_pass = render_graph->add_pass(std::make_shared<SunDiskPass>());


		RenderPassInterface* mipmap_generation_pass = render_graph->add_pass(std::make_shared<MipmapGenerationPass>());

		RenderPassInterface* mesh_cluster_culling_pass = render_graph->add_pass(std::make_shared<MeshClusterCullingPass>());
		RenderPassInterface* hierarchical_zbuffer_pass = render_graph->add_pass(std::make_shared<HierarchicalZBufferPass>());
		RenderPassInterface* virtual_gbuffer_pass = render_graph->add_pass(std::make_shared<VirtualGBufferPass>());
		RenderPassInterface* virtual_texture_update_pass = render_graph->add_pass(std::make_shared<VirtualTextureUpdatePass>());
		RenderPassInterface* virtual_texture_feed_back_pass = render_graph->add_pass(std::make_shared<VirtualTextureFeedBackPass>());
		RenderPassInterface* virtual_shadow_map_pass = render_graph->add_pass(std::make_shared<VirtualShadowMapPass>());
		RenderPassInterface* test_pass = render_graph->add_pass(std::make_shared<FinalTestPass>());

		mesh_cluster_culling_pass->precede(virtual_gbuffer_pass);
		virtual_gbuffer_pass->precede(hierarchical_zbuffer_pass);
		hierarchical_zbuffer_pass->precede(test_pass);
		virtual_gbuffer_pass->precede(virtual_texture_feed_back_pass);
		virtual_texture_feed_back_pass->precede(virtual_shadow_map_pass);
		virtual_shadow_map_pass->precede(virtual_texture_update_pass);
		virtual_texture_update_pass->precede(test_pass);

		test_pass->precede(sky_lut_pass);
		sky_lut_pass->precede(sky_pass);
		sky_pass->precede(sun_disk_pass);

		World* world = render_graph->get_resource_cache()->get_world();
		constant::AtmosphereProperties* properties = world->get_global_entity()->assign<constant::AtmosphereProperties>();

		return sun_disk_pass;
    }
} 


int main()
{
	fantasy::FinalTest test(fantasy::GraphicsAPI::D3D12);
	if (!test.initialize() || !test.run()) return -1;
	return 0;
}