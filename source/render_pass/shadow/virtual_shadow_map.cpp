
#include "virtual_shadow_map.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/light.h"
#include "../../scene/scene.h"
#include <memory>

namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16u

	bool VirtualShadowMapPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		ReturnIfFalse(cache->require_constants("update_shadow_pages", (void**)&_update_shadow_pages));
		ReturnIfFalse(cache->require_constants("cluster_group_count", (void**)&_cluster_group_count));

		
		DirectionalLight* _directional_light = cache->get_world()->get_global_entity()->get_component<DirectionalLight>();

		uint32_t axis_tile_num = (VT_VIRTUAL_SHADOW_RESOLUTION / VT_SHADOW_PAGE_SIZE);
		float3 tile_right = normalize(cross(float3(0.0f, 1.0f, 0.0f), _directional_light->direction));
		float3 tile_up = normalize(cross(_directional_light->direction, tile_right));

		float3 right_offset = tile_right * _directional_light->orthographic_length / axis_tile_num;
		float3 up_offset = tile_up * _directional_light->orthographic_length / axis_tile_num;
		
		_shadow_tile_view_matrixs.resize(axis_tile_num * axis_tile_num);
		for (uint32_t y = 0; y < axis_tile_num; ++y)
		{
			for (uint32_t x = 0; x < axis_tile_num; ++x)
			{
				float3 offset = x * right_offset + y * up_offset;
				
				_shadow_tile_view_matrixs[x + y * axis_tile_num] = look_at_left_hand(
					_directional_light->get_position() + offset,
					offset,
					float3(0.0f, 1.0f, 0.0f)
				);
			}
		}

		// Binding Layout.
		{
			BindingLayoutItemArray cull_binding_layout_items(6);
			cull_binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::ShadowTileCullingConstant));
			cull_binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_uav(0);
			cull_binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_uav(1);
			cull_binding_layout_items[3] = BindingLayoutItem::create_structured_buffer_srv(0);
			cull_binding_layout_items[4] = BindingLayoutItem::create_structured_buffer_srv(1);
			cull_binding_layout_items[5] = BindingLayoutItem::create_structured_buffer_srv(2);
			ReturnIfFalse(_cull_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = cull_binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "culling/shadow_tile_culling_cs.hlsl";
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
			ComputePipelineDesc cull_pipeline_desc;
			cull_pipeline_desc.compute_shader = _cs;
			cull_pipeline_desc.binding_layouts.push_back(_binding_layout);
			ReturnIfFalse(_cull_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(cull_pipeline_desc)));
		}


		// Buffer.
		{
			ReturnIfFalse(_vt_shadow_draw_indirect_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_read_write_structured_buffer(
					sizeof(DrawIndexedIndirectArguments), 
					sizeof(DrawIndexedIndirectArguments), 
					"vt_shadow_draw_indirect_buffer"
				)
			)));
			cache->collect(_vt_shadow_draw_indirect_buffer, ResourceType::Buffer);

            uint32_t virtual_shadow_resolution_in_page = VT_VIRTUAL_SHADOW_RESOLUTION / VT_SHADOW_PAGE_SIZE;
			ReturnIfFalse(_vt_shadow_visible_cluster_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_read_write_structured_buffer(
					sizeof(uint2) * virtual_shadow_resolution_in_page * virtual_shadow_resolution_in_page, 
					sizeof(uint2), 
					"vt_shadow_visible_cluster_buffer"
				)
			)));
			cache->collect(_vt_shadow_visible_cluster_buffer, ResourceType::Buffer);
		}
 
		// Binding Set.
		{
			_cull_binding_set_items.resize(6);
			_cull_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::ShadowTileCullingConstant));
			_cull_binding_set_items[1] = BindingSetItem::create_structured_buffer_uav(0, _vt_shadow_draw_indirect_buffer);
			_cull_binding_set_items[2] = BindingSetItem::create_structured_buffer_uav(1, _vt_shadow_visible_cluster_buffer);
			_cull_binding_set_items[5] = BindingSetItem::create_structured_buffer_srv(2, check_cast<BufferInterface>(cache->require("vt_shadow_page_buffer")));
		}

		// Compute state.
		{
			_compute_state.binding_sets.resize(1);
			_compute_state.pipeline = _cull_pipeline.get();
		}
 


		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(7);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::VirtualShadowMapPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_structured_buffer_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_structured_buffer_srv(3);
			binding_layout_items[5] = BindingLayoutItem::create_structured_buffer_srv(4);
			binding_layout_items[6] = BindingLayoutItem::create_texture_uav(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "shadow/virtual_shadow_map_vs.hlsl";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = "shadow/virtual_shadow_map_ps.hlsl";
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

		// Texture.
		{
			// InterlockedMin 不支持浮点数, 故在这里使用整数格式存储浮点数, 深度值永远为正数, 将浮点深度转化为整数可直接比较.
			ReturnIfFalse(_physical_shadow_map_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					VT_PHYSICAL_SHADOW_RESOLUTION,
					VT_PHYSICAL_SHADOW_RESOLUTION,
					Format::R32_UINT,
					"physical_shadow_map_texture"
				)
			)));
			cache->collect(_physical_shadow_map_texture, ResourceType::Texture);

			ReturnIfFalse(_black_render_target_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target_texture(
					VT_SHADOW_PAGE_SIZE,
					VT_SHADOW_PAGE_SIZE,
					Format::RGBA8_UNORM
				)
			)));
		}
 
		// Frame Buffer.
		{
			FrameBufferDesc frame_buffer_desc;
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_black_render_target_texture));
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
			_binding_set_items.resize(7);
			_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::VirtualShadowMapPassConstant));
			_binding_set_items[2] = BindingSetItem::create_structured_buffer_srv(1, check_cast<BufferInterface>(cache->require("vt_shadow_visible_cluster_buffer")));
			_binding_set_items[6] = BindingSetItem::create_texture_uav(0, _physical_shadow_map_texture);
		}

		// Graphics state.
		{
			_graphics_state.binding_sets.resize(1);
			_graphics_state.pipeline = _pipeline.get();
			_graphics_state.frame_buffer = _frame_buffer.get();
			_graphics_state.viewport_state = ViewportState::create_default_viewport(VT_SHADOW_PAGE_SIZE, VT_SHADOW_PAGE_SIZE);
			_graphics_state.indirect_buffer = check_cast<BufferInterface>(cache->require("vt_shadow_draw_indirect_buffer")).get();
		}

		return true;
	}

	bool VirtualShadowMapPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());
        
		if (SceneSystem::loaded_submesh_count != 0)
		{
			DirectionalLight* light = cache->get_world()->get_global_entity()->get_component<DirectionalLight>();
			
			constant::ShadowTileCullingConstant cull_constant;
			cull_constant.group_count = *_cluster_group_count;
			cull_constant.near_plane = light->near_plane;
			cull_constant.far_plane = light->far_plane;
			cull_constant.shadow_orthographic_length = light->orthographic_length;

			uint32_t axis_shadow_tile_num = VT_VIRTUAL_SHADOW_RESOLUTION / VT_SHADOW_PAGE_SIZE;
			_cull_pass_constants.resize(_update_shadow_pages->size(), cull_constant);
			for (uint32_t ix = 0; ix < _cull_pass_constants.size(); ++ix)
			{
				_cull_pass_constants[ix].packed_shadow_page_id = (*_update_shadow_pages)[ix].y;

				uint32_t packed_tile_id = (*_update_shadow_pages)[ix].x;
				uint2 tile_id(packed_tile_id >> 16, packed_tile_id & 0xffff);
				_cull_pass_constants[ix].shadow_view_matrix = 
					_shadow_tile_view_matrixs[tile_id.x + tile_id.y * axis_shadow_tile_num];
			}

			_pass_constant.view_matrix = light->view_matrix;
			_pass_constant.view_proj = light->get_view_proj();


			if (!_resource_writed)
			{
				DeviceInterface* device = cmdlist->get_deivce();
				_cull_binding_set_items[3] = BindingSetItem::create_structured_buffer_srv(0, check_cast<BufferInterface>(cache->require("mesh_cluster_group_buffer")));
				_cull_binding_set_items[4] = BindingSetItem::create_structured_buffer_srv(1, check_cast<BufferInterface>(cache->require("mesh_cluster_buffer")));
				ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
					BindingSetDesc{ .binding_items = _cull_binding_set_items },
					_binding_layout
				)));
				_compute_state.binding_sets[0] = _cull_binding_set.get();

				_binding_set_items[1] = BindingSetItem::create_structured_buffer_srv(0, check_cast<BufferInterface>(cache->require("geometry_constant_buffer")));
				_binding_set_items[3] = BindingSetItem::create_structured_buffer_srv(2, check_cast<BufferInterface>(cache->require("mesh_cluster_buffer")));
				_binding_set_items[4] = BindingSetItem::create_structured_buffer_srv(3, check_cast<BufferInterface>(cache->require("cluster_vertex_buffer")));
				_binding_set_items[5] = BindingSetItem::create_structured_buffer_srv(4, check_cast<BufferInterface>(cache->require("cluster_triangle_buffer")));
				ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
					BindingSetDesc{ .binding_items = _binding_set_items },
					_binding_layout
				)));
				_graphics_state.binding_sets[0] = _binding_set.get();

				_resource_writed = true;
			}

			for (auto& cull_constant : _cull_pass_constants)
			{
				cmdlist->clear_buffer_uint(
					_vt_shadow_draw_indirect_buffer.get(), 
					BufferRange(0, sizeof(DrawIndexedIndirectArguments)), 
					0
				);
	
				cmdlist->clear_buffer_uint(
					_vt_shadow_visible_cluster_buffer.get(), 
					BufferRange(0, _vt_shadow_visible_cluster_buffer->get_desc().byte_size), 
					0
				);
	
				ReturnIfFalse(cmdlist->dispatch(
					_compute_state, 
					align(*_cluster_group_count, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X, 
					1, 
					1, 
					&cull_constant
				));
	
				ReturnIfFalse(cmdlist->draw_indexed_indirect(_graphics_state, 0, 1, &_pass_constant));
			}

		}

		ReturnIfFalse(cmdlist->close());
		return true;
	}
}
