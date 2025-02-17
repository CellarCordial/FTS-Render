

#include "shadow_tile_culling.h"
#include "../../shader/shader_compiler.h"
#include "../../scene/virtual_texture.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/scene.h"
#include "../../scene/light.h"


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16u
 
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
			cache->collect(_virtual_shadow_draw_indirect_buffer, ResourceType::Buffer);

            uint32_t virtual_shadow_resolution_in_page = VIRTUAL_SHADOW_RESOLUTION / VIRTUAL_SHADOW_PAGE_SIZE;
			ReturnIfFalse(_virtual_shadow_visible_cluster_id_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_read_write_structured_buffer(
					sizeof(ShadowVisibleInfo) * virtual_shadow_resolution_in_page * virtual_shadow_resolution_in_page, 
					sizeof(ShadowVisibleInfo), 
					"virtual_shadow_visible_cluster_id_buffer"
				)
			)));
			cache->collect(_virtual_shadow_visible_cluster_id_buffer, ResourceType::Buffer);
		}
 
		// Binding Set.
		{
			_binding_set_items.resize(6);
			_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::ShadowTileCullingConstant));
			_binding_set_items[1] = BindingSetItem::create_structured_buffer_uav(0, _virtual_shadow_draw_indirect_buffer);
			_binding_set_items[2] = BindingSetItem::create_structured_buffer_uav(1, _virtual_shadow_visible_cluster_id_buffer);
			_binding_set_items[5] = BindingSetItem::create_structured_buffer_srv(2, check_cast<BufferInterface>(cache->require("shadow_tile_info_buffer")));
		}

		// Compute state.
		{
			_compute_state.binding_sets.resize(1);
			_compute_state.pipeline = _pipeline.get();
		}
 
		return true;
	}

	bool ShadowTileCullingPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		if (SceneSystem::loaded_submesh_count != 0)
		{
			_pass_constant.shadow_tile_num = VIRTUAL_SHADOW_RESOLUTION / VIRTUAL_SHADOW_PAGE_SIZE;

			uint32_t* cluster_group_count = nullptr;
			ReturnIfFalse(cache->require_constants("cluster_group_count", (void**)&cluster_group_count));
			_pass_constant.group_count = *cluster_group_count;

			DirectionalLight* light = cache->get_world()->get_global_entity()->get_component<DirectionalLight>();
			_pass_constant.near_plane = light->near_plane;
			_pass_constant.far_plane = light->far_plane;
			_pass_constant.frustum_right_normal = normalize(cross(float3(0.0f, 1.0f, 0.0f), light->direction));
			_pass_constant.frustum_top_normal = cross(light->direction, _pass_constant.frustum_right_normal);

			if (!_resource_writed)
			{
				DeviceInterface* device = cmdlist->get_deivce();
				_binding_set_items[3] = BindingSetItem::create_structured_buffer_srv(0, check_cast<BufferInterface>(cache->require("mesh_cluster_group_buffer")));
				_binding_set_items[4] = BindingSetItem::create_structured_buffer_srv(1, check_cast<BufferInterface>(cache->require("mesh_cluster_buffer")));
				ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
					BindingSetDesc{ .binding_items = _binding_set_items },
					_binding_layout
				)));
				_compute_state.binding_sets[0] = _binding_set.get();
				_resource_writed = true;
			}

			ReturnIfFalse(cmdlist->dispatch(
				_compute_state, 
				align(*cluster_group_count, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X, 
				1, 
				1, 
				&_pass_constant
			));
		}
		ReturnIfFalse(cmdlist->close());
        return true;
	}
}