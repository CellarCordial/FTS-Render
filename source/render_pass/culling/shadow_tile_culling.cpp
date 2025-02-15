

#include "shadow_tile_culling.h"
#include "../../shader/shader_compiler.h"
#include "../../scene/virtual_texture.h"
#include "../../core/tools/check_cast.h"


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
 
    struct ShadowVisibleInfo
    {
        uint32_t cluster_id;
        uint2 tile_id;
    };

	bool ShadowTileCullingPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(6);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::ShadowTileCullingConstant));
			binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_uav(0);
			binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_uav(1);
			binding_layout_items[3] = BindingLayoutItem::create_structured_buffer_srv(0);
			binding_layout_items[4] = BindingLayoutItem::create_structured_buffer_srv(1);
			binding_layout_items[5] = BindingLayoutItem::create_structured_buffer_srv(2);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "culling/shadow_tile_culling_cs.slang";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			ShaderData cs_data = compile_shader(cs_compile_desc);

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
			ReturnIfFalse(_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));
		}


		// Buffer.
		{
			ReturnIfFalse(_virtual_shadow_draw_indirect_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_read_write_structured_buffer(
					sizeof(DrawIndexedIndirectArguments), 
					sizeof(DrawIndexedIndirectArguments), 
					"virtual_shadow_draw_indirect_buffer"
				)
			)));

            uint32_t virtual_shadow_resolution_in_page = VIRTUAL_SHADOW_RESOLUTION / VIRTUAL_SHADOW_PAGE_SIZE;
			ReturnIfFalse(_virtual_shadow_visible_cluster_id_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_read_write_structured_buffer(
					sizeof(ShadowVisibleInfo) * virtual_shadow_resolution_in_page * virtual_shadow_resolution_in_page, 
					sizeof(ShadowVisibleInfo), 
					"virtual_shadow_visible_cluster_id_buffer"
				)
			)));
		}
 
		// Binding Set.
		{
			_binding_set_items.resize(6);
			_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::ShadowTileCullingConstant));
			_binding_set_items[1] = BindingSetItem::create_structured_buffer_uav(0, _virtual_shadow_draw_indirect_buffer);
			_binding_set_items[2] = BindingSetItem::create_structured_buffer_uav(1, _virtual_shadow_visible_cluster_id_buffer);
			_binding_set_items[5] = BindingSetItem::create_structured_buffer_srv(2, check_cast<BufferInterface>(cache->require("shadow_tile_info_buffer")));
            ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
                BindingSetDesc{ .binding_items = _binding_set_items },
                _binding_layout
            )));
		}

		// Compute state.
		{
			_compute_state.pipeline = _pipeline.get();
            _compute_state.indirect_buffer = check_cast<BufferInterface>(cache->require("virtual_shadow_draw_indirect_buffer")).get();
		}
 
		return true;
	}

	bool ShadowTileCullingPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
        if (!_resource_writed)
        {
            DeviceInterface* device = cmdlist->get_deivce();
			_binding_set_items[3] = BindingSetItem::create_structured_buffer_srv(0, check_cast<BufferInterface>(cache->require("mesh_cluster_group_buffer")));
			_binding_set_items[4] = BindingSetItem::create_structured_buffer_srv(1, check_cast<BufferInterface>(cache->require("mesh_cluster_buffer")));
            ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
                BindingSetDesc{ .binding_items = _binding_set_items },
                _binding_layout
            )));
			_compute_state.binding_sets.push_back(_binding_set.get());
            _resource_writed = true;
        }

		ReturnIfFalse(cmdlist->open());

        uint32_t* cluster_group_count = nullptr;
        ReturnIfFalse(cache->require_constants("cluster_group_count", (void**)&cluster_group_count));
		ReturnIfFalse(cmdlist->dispatch(_compute_state, *cluster_group_count, 1, 1, &_pass_constant));

		ReturnIfFalse(cmdlist->close());
        return true;
	}
}