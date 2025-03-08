
#include "virtual_shadow_map.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/scene.h"
#include <bit>
#include <cstdint>
#include <memory>

namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16u
#define THREAD_GROUP_SIZE_Y 16u

	void VirtualShadowMapPass::recalculate_shadow_view_matrixs()
	{
		uint32_t axis_tile_num = (VT_VIRTUAL_SHADOW_RESOLUTION / VT_SHADOW_PAGE_SIZE);
		float3 tile_right = normalize(cross(float3(0.0f, 1.0f, 0.0f), _directional_light->direction));
		float3 tile_up = normalize(cross(_directional_light->direction, tile_right));

		// 以左上角为原点.
		float3 horz_offset = tile_right * _directional_light->orthographic_length / axis_tile_num;
		float3 vert_offset = tile_up * _directional_light->orthographic_length / axis_tile_num;
		float3 offset = -(horz_offset - vert_offset);

		float tile_orthographic_length = _directional_light->orthographic_length / axis_tile_num;
		_shadow_tile_proj_matrix = orthographic_left_hand(
			tile_orthographic_length, 
			tile_orthographic_length, 
			_directional_light->near_plane, 
			_directional_light->far_plane
		);
		
		_shadow_tile_view_matrixs.resize(axis_tile_num * axis_tile_num);
		
		float3 first_tile_look =  offset * (axis_tile_num * 0.5f - 0.5f);
		float3 first_tile_center = _directional_light->get_position() + offset * (axis_tile_num * 0.5f - 0.5f);
		for (uint32_t y = 0; y < axis_tile_num; ++y)
		{
			for (uint32_t x = 0; x < axis_tile_num; ++x)
			{
				float3 tile_offset = x * horz_offset - y * vert_offset;
				
				_shadow_tile_view_matrixs[x + y * axis_tile_num] = look_at_left_hand(
					first_tile_center + tile_offset,
					first_tile_look + tile_offset,
					float3(0.0f, 1.0f, 0.0f)
				);
			}
		}
	}


	bool VirtualShadowMapPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		cache->get_world()->get_global_entity()->get_component<event::UpdateShadowMap>()->add_event(
			[this]() -> bool
			{
				_update_shadow_map = true;
				recalculate_shadow_view_matrixs();
				return true;
			}
		);

		cache->get_world()->get_global_entity()->get_component<event::AddModel>()->add_event(
			[this]() -> bool
			{
				_resource_writed = false;	
				_update_shadow_map = true;

				recalculate_shadow_view_matrixs();
				return true;
			}
		);


		ReturnIfFalse(cache->require_constants("vt_new_shadow_pages", (void**)&_vt_new_shadow_pages));
		ReturnIfFalse(cache->require_constants("cluster_group_count", (void**)&_cluster_group_count));
		_directional_light = cache->get_world()->get_global_entity()->get_component<DirectionalLight>();


		
		// Texture.
		{
			ReturnIfFalse(_shadow_map_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_depth_stencil_texture(
					_shadow_map_resolution,
					_shadow_map_resolution,
					Format::D32,
					"shadow_map_texture"
				)
			)));
			cache->collect(_shadow_map_texture, ResourceType::Texture);
		}

		ReturnIfFalse(compile_hi_z_update_pass(device, cache));
		ReturnIfFalse(compile_culling_pass(device, cache));
		ReturnIfFalse(compile_virtual_shadow_pass(device, cache));
		ReturnIfFalse(compile_shadow_map_pass(device, cache));

		return true;
	}

	bool VirtualShadowMapPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());
        
		if (SceneSystem::loaded_submesh_count != 0 && _update_shadow_map)
		{
			_shadow_map_cull_constant.group_count = *_cluster_group_count;
			_shadow_map_cull_constant.far_plane = _directional_light->far_plane;
			_shadow_map_cull_constant.near_plane = _directional_light->near_plane;
			_shadow_map_cull_constant.shadow_view_matrix = _directional_light->view_matrix;
			_shadow_map_cull_constant.shadow_orthographic_length = _directional_light->orthographic_length;
			_shadow_map_cull_constant.shadow_map_resolution = _shadow_map_resolution;
			_shadow_map_cull_constant.hzb_resolution = _shadow_hzb_resolution;
			_shadow_map_cull_constant.shadow_proj_matrix = _shadow_tile_proj_matrix;
			
			_shadow_map_constants.view_proj = _directional_light->get_view_proj();


			uint32_t axis_shadow_tile_num = VT_VIRTUAL_SHADOW_RESOLUTION / VT_SHADOW_PAGE_SIZE;
			float tile_orthographic_length = _directional_light->orthographic_length / axis_shadow_tile_num;

			_cull_pass_constants.resize(_vt_new_shadow_pages->size(), _shadow_map_cull_constant);
			_virtual_shadow_pass_constants.resize(_cull_pass_constants.size());
			for (uint32_t ix = 0; ix < _cull_pass_constants.size(); ++ix)
			{
				VTShadowPage page = (*_vt_new_shadow_pages)[ix];
				
				_cull_pass_constants[ix].shadow_orthographic_length = tile_orthographic_length;
				_cull_pass_constants[ix].packed_shadow_page_id = (page.physical_position_in_page.x << 16) | 
																 (page.physical_position_in_page.y & 0xffff);
				_cull_pass_constants[ix].shadow_view_matrix = 
					_shadow_tile_view_matrixs[page.tile_id.x + page.tile_id.y * axis_shadow_tile_num];

				_virtual_shadow_pass_constants[ix].view_proj = mul(
					_cull_pass_constants[ix].shadow_view_matrix, 
					_cull_pass_constants[ix].shadow_proj_matrix
				);
			}
			_vt_new_shadow_pages->clear();


			if (!_resource_writed)
			{
				DeviceInterface* device = cmdlist->get_deivce();
				_cull_binding_set_items[3] = BindingSetItem::create_structured_buffer_srv(0, check_cast<BufferInterface>(cache->require("mesh_cluster_group_buffer")));
				_cull_binding_set_items[4] = BindingSetItem::create_structured_buffer_srv(1, check_cast<BufferInterface>(cache->require("mesh_cluster_buffer")));
				_cull_binding_set_items[5] = BindingSetItem::create_structured_buffer_srv(2, check_cast<BufferInterface>(cache->require("geometry_constant_buffer")));
				ReturnIfFalse(_cull_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
					BindingSetDesc{ .binding_items = _cull_binding_set_items },
					_cull_binding_layout
				)));
				_cull_compute_state.binding_sets[0] = _cull_binding_set.get();

				_virtual_shadow_binding_set_items[1] = BindingSetItem::create_structured_buffer_srv(0, check_cast<BufferInterface>(cache->require("geometry_constant_buffer")));
				_virtual_shadow_binding_set_items[3] = BindingSetItem::create_structured_buffer_srv(2, check_cast<BufferInterface>(cache->require("mesh_cluster_buffer")));
				_virtual_shadow_binding_set_items[4] = BindingSetItem::create_structured_buffer_srv(3, check_cast<BufferInterface>(cache->require("cluster_vertex_buffer")));	
#if NANITE
				_virtual_shadow_binding_set_items[5] = BindingSetItem::create_structured_buffer_srv(4, check_cast<BufferInterface>(cache->require("cluster_triangle_buffer")));
#endif				
				ReturnIfFalse(_virtual_shadow_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
					BindingSetDesc{ .binding_items = _virtual_shadow_binding_set_items },
					_virtual_shadow_binding_layout
				)));
				_shadow_map_graphics_state.binding_sets[0] = _virtual_shadow_binding_set.get();
				_virtual_shadow_graphics_state.binding_sets[0] = _virtual_shadow_binding_set.get();

				_resource_writed = true;
			}

			
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
			
			_cull_compute_state.pipeline = _cull_pipeline.get();
			ReturnIfFalse(cmdlist->dispatch(
				_cull_compute_state, 
				align(*_cluster_group_count, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X, 
				1, 
				1, 
				&_shadow_map_cull_constant
			));
			
			ReturnIfFalse(clear_depth_stencil_attachment(cmdlist, _shadow_map_frame_buffer.get()));

			ReturnIfFalse(cmdlist->draw_indirect(_shadow_map_graphics_state, 0, 1, &_shadow_map_constants));

			cmdlist->set_texture_state(
				_shadow_map_texture.get(), 
				TextureSubresourceSet{}, 
				ResourceStates::GraphicsShaderResource | ResourceStates::DepthRead
			);

			cmdlist->clear_texture_float(
				_shadow_hi_z_texture.get(), 
				TextureSubresourceSet{
					.base_mip_level = 0,
					.mip_level_count = _shadow_hi_z_texture->get_desc().mip_levels,
					.base_array_slice = 0,
					.array_slice_count = 1
				}, 
				Color{ 0.0f }
			);

			uint2 thread_group_num = {
				static_cast<uint32_t>((align(_hi_z_update_pass_constants[0].hzb_resolution, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
				static_cast<uint32_t>((align(_hi_z_update_pass_constants[0].hzb_resolution, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
			};
			
			for (uint32_t ix = 0; ix < _hi_z_update_pass_constants.size(); ++ix)
			{
				ReturnIfFalse(cmdlist->dispatch(
					_hi_z_update_compute_state, 
					thread_group_num.x, 
					thread_group_num.y, 
					1, 
					&_hi_z_update_pass_constants[ix]
				));
			}

			cmdlist->clear_texture_uint(_vt_physical_shadow_texture.get(), TextureSubresourceSet{}, std::bit_cast<uint32_t>(1.0f));

			_cull_compute_state.pipeline = _hi_z_cull_pipeline.get();
			for (uint32_t ix = 0; ix < _cull_pass_constants.size(); ++ix)
			{
				const auto& cull_constant = _cull_pass_constants[ix];
				
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
					_cull_compute_state, 
					align(*_cluster_group_count, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X, 
					1, 
					1, 
					&cull_constant
				));

				ReturnIfFalse(clear_color_attachment(cmdlist, _virtual_shadow_frame_buffer.get()));
				ReturnIfFalse(cmdlist->draw_indirect(_virtual_shadow_graphics_state, 0, 1, &_virtual_shadow_pass_constants[ix]));
			}

			if (!_cull_pass_constants.empty())
			{
				cmdlist->copy_texture(
					_vt_physical_shadow_float_texture.get(), 
					TextureSlice{}, 
					_vt_physical_shadow_texture.get(), 
					TextureSlice{}
				);
			}

			_update_shadow_map = false;
		}

		ReturnIfFalse(cmdlist->close());
		return true;
	}

	bool VirtualShadowMapPass::compile_culling_pass(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray cull_binding_layout_items(8);
			cull_binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::ShadowCullingConstant));
			cull_binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_uav(0);
			cull_binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_uav(1);
			cull_binding_layout_items[3] = BindingLayoutItem::create_structured_buffer_srv(0);
			cull_binding_layout_items[4] = BindingLayoutItem::create_structured_buffer_srv(1);
			cull_binding_layout_items[5] = BindingLayoutItem::create_structured_buffer_srv(2);
			cull_binding_layout_items[6] = BindingLayoutItem::create_texture_srv(3);
			cull_binding_layout_items[7] = BindingLayoutItem::create_sampler(0);
			ReturnIfFalse(_cull_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = cull_binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "culling/shadow_culling_cs.hlsl";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.resize(3);
			cs_compile_desc.defines[0] = ("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines[1] = ("NANITE=" + std::to_string(NANITE));
			cs_compile_desc.defines[2] = ("HI_Z_CULLING=0");
			ShaderData cs_data = compile_shader(cs_compile_desc);
			cs_compile_desc.defines[2] = ("HI_Z_CULLING=1");
			ShaderData hi_z_cs_data = compile_shader(cs_compile_desc);


			ShaderDesc cs_desc;
			cs_desc.entry = "main";
			cs_desc.shader_type = ShaderType::Compute;
			ReturnIfFalse(_cull_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));
			ReturnIfFalse(_hi_z_cull_cs = std::unique_ptr<Shader>(create_shader(cs_desc, hi_z_cs_data.data(), hi_z_cs_data.size())));
		}

		// Pipeline.
		{
			ComputePipelineDesc cull_pipeline_desc;
			cull_pipeline_desc.binding_layouts.push_back(_cull_binding_layout);
			cull_pipeline_desc.compute_shader = _cull_cs;
			ReturnIfFalse(_cull_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(cull_pipeline_desc)));
			cull_pipeline_desc.compute_shader = _hi_z_cull_cs;
			ReturnIfFalse(_hi_z_cull_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(cull_pipeline_desc)));
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
			_cull_binding_set_items.resize(8);
			_cull_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::ShadowCullingConstant));
			_cull_binding_set_items[1] = BindingSetItem::create_structured_buffer_uav(0, _vt_shadow_draw_indirect_buffer);
			_cull_binding_set_items[2] = BindingSetItem::create_structured_buffer_uav(1, _vt_shadow_visible_cluster_buffer);
			_cull_binding_set_items[6] = BindingSetItem::create_texture_srv(3, _shadow_hi_z_texture);
			_cull_binding_set_items[7] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
		}

		// Compute state.
		{
			_cull_compute_state.binding_sets.resize(1);
		}

		return true;
	}

	bool VirtualShadowMapPass::compile_shadow_map_pass(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Frame Buffer.
		{
			FrameBufferDesc frame_buffer_desc;
			frame_buffer_desc.depth_stencil_attachment = FrameBufferAttachment::create_attachment(_shadow_map_texture);
			ReturnIfFalse(_shadow_map_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(frame_buffer_desc)));
		}
 
		// Pipeline.
		{
			GraphicsPipelineDesc pipeline_desc;
			pipeline_desc.vertex_shader = _virtual_shadow_vs;
			pipeline_desc.binding_layouts.push_back(_virtual_shadow_binding_layout);
			pipeline_desc.render_state.depth_stencil_state.enable_depth_test = true;
			pipeline_desc.render_state.depth_stencil_state.enable_depth_write = true;
			pipeline_desc.render_state.depth_stencil_state.depth_func = ComparisonFunc::Less;
			ReturnIfFalse(_shadow_map_pipeline = std::unique_ptr<GraphicsPipelineInterface>(
				device->create_graphics_pipeline(pipeline_desc, _shadow_map_frame_buffer.get())
			));
		}

		// Graphics state.
		{
			_shadow_map_graphics_state.binding_sets.resize(1);
			_shadow_map_graphics_state.pipeline = _shadow_map_pipeline.get();
			_shadow_map_graphics_state.frame_buffer = _shadow_map_frame_buffer.get();
			_shadow_map_graphics_state.viewport_state = ViewportState::create_default_viewport(_shadow_map_resolution, _shadow_map_resolution);
			_shadow_map_graphics_state.indirect_buffer = _vt_shadow_draw_indirect_buffer.get();
		}

		return true;
	}

	bool VirtualShadowMapPass::compile_hi_z_update_pass(DeviceInterface* device, RenderResourceCache* cache)
	{
        uint32_t hzb_mip_levels = search_most_significant_bit(_shadow_hzb_resolution) + 1;
		// Texture.
		{
			TextureDesc desc;
			desc.name = "shadow_hi_z_texture";
			desc.width = _shadow_hzb_resolution;
			desc.height = _shadow_hzb_resolution;
			desc.format = Format::R32_FLOAT;
			desc.mip_levels = hzb_mip_levels;
			desc.allow_unordered_access = true;
			ReturnIfFalse(_shadow_hi_z_texture = std::shared_ptr<TextureInterface>(device->create_texture(desc)));
		}

		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(3 + hzb_mip_levels);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::ShadowHiZUpdatePassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_sampler(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(0);
            for (uint32_t ix = 0; ix < hzb_mip_levels; ++ix)
            {
			    binding_layout_items[3 + ix] = BindingLayoutItem::create_texture_uav(ix);
            }
			ReturnIfFalse(_hi_z_update_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "culling/hierarchical_zbuffer_update_cs.hlsl";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			cs_compile_desc.defines.push_back("REVERSE_Z_BUFFER=0");
			ShaderData cs_data = compile_shader(cs_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.entry = "main";
			cs_desc.shader_type = ShaderType::Compute;
			ReturnIfFalse(_hi_z_update_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));
		}

		// Pipeline.
		{
			ComputePipelineDesc pipeline_desc;
			pipeline_desc.binding_layouts.push_back(_hi_z_update_binding_layout);
			pipeline_desc.compute_shader = _hi_z_update_cs;
			ReturnIfFalse(_hi_z_update_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(3 + hzb_mip_levels);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::ShadowHiZUpdatePassConstant));
			binding_set_items[1] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
			binding_set_items[2] = BindingSetItem::create_texture_srv(0, _shadow_map_texture, TextureSubresourceSet{}, Format::R32_FLOAT);
            for (uint32_t ix = 0; ix < hzb_mip_levels; ++ix)
            {
			    binding_set_items[3 + ix] = BindingSetItem::create_texture_uav(
                    ix, 
                    _shadow_hi_z_texture,
                    TextureSubresourceSet{
                        .base_mip_level = ix,
                        .mip_level_count = 1,
                        .base_array_slice = 0,
                        .array_slice_count = 1  
                    }
                );
            }
			ReturnIfFalse(_hi_z_update_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_hi_z_update_binding_layout
			)));
		}

		// Compute state.
		{
			_hi_z_update_compute_state.binding_sets.push_back(_hi_z_update_binding_set.get());
			_hi_z_update_compute_state.pipeline = _hi_z_update_pipeline.get();
		}

		_hi_z_update_pass_constants.resize(hzb_mip_levels);
        for (uint32_t ix = 0; ix < _hi_z_update_pass_constants.size(); ++ix)
		{
			_hi_z_update_pass_constants[ix].shadow_map_resolution = uint2(_shadow_map_resolution);
			_hi_z_update_pass_constants[ix].last_mip_level = ix;
			_hi_z_update_pass_constants[ix].hzb_resolution = _shadow_hzb_resolution;
		}
		return true;
	}

	bool VirtualShadowMapPass::compile_virtual_shadow_pass(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
#if NANITE
			BindingLayoutItemArray binding_layout_items(7);
			binding_layout_items[5] = BindingLayoutItem::create_structured_buffer_srv(4);
			binding_layout_items[6] = BindingLayoutItem::create_texture_uav(0);
#else
			BindingLayoutItemArray binding_layout_items(6);
			binding_layout_items[5] = BindingLayoutItem::create_texture_uav(0);
#endif
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::ShadowPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_structured_buffer_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_structured_buffer_srv(3);
			ReturnIfFalse(_virtual_shadow_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "shadow/virtual_shadow_map_vs.hlsl";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Vertex;
#if NANITE
			shader_compile_desc.defines.push_back("NANITE=1");
#endif
			ShaderData vs_data = compile_shader(shader_compile_desc);

			shader_compile_desc.shader_name = "shadow/virtual_shadow_map_ps.hlsl";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Pixel;
			ShaderData ps_data = compile_shader(shader_compile_desc);

			ShaderDesc vs_desc;
			vs_desc.entry = "main";
			vs_desc.shader_type = ShaderType::Vertex;
			ReturnIfFalse(_virtual_shadow_vs = std::unique_ptr<Shader>(create_shader(vs_desc, vs_data.data(), vs_data.size())));

			ShaderDesc ps_desc;
			ps_desc.shader_type = ShaderType::Pixel;
			ps_desc.entry = "main";
			ReturnIfFalse(_virtual_shadow_ps = std::unique_ptr<Shader>(create_shader(ps_desc, ps_data.data(), ps_data.size())));
		}

		// Texture.
		{
			// InterlockedMin 不支持浮点数, 故在这里使用整数格式存储浮点数, 深度值永远为正数, 将浮点深度转化为整数可直接比较.
			ReturnIfFalse(_vt_physical_shadow_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					VT_PHYSICAL_SHADOW_RESOLUTION,
					VT_PHYSICAL_SHADOW_RESOLUTION,
					Format::R32_UINT,
					"vt_physical_shadow_texture"
				)
			)));
			cache->collect(_vt_physical_shadow_texture, ResourceType::Texture);
			
			ReturnIfFalse(_vt_physical_shadow_float_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					VT_PHYSICAL_SHADOW_RESOLUTION,
					VT_PHYSICAL_SHADOW_RESOLUTION,
					Format::R32_FLOAT,
					"vt_physical_shadow_float_texture"
				)
			)));
			cache->collect(_vt_physical_shadow_float_texture, ResourceType::Texture);

			ReturnIfFalse(_black_render_target_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target_texture(
					VT_SHADOW_PAGE_SIZE,
					VT_SHADOW_PAGE_SIZE,
					Format::RGBA8_UNORM,
					"",
					false,
					Color(1.0f)
				)
			)));
		}
 
		// Frame Buffer.
		{
			FrameBufferDesc frame_buffer_desc;
			frame_buffer_desc.color_attachments.push_back(FrameBufferAttachment::create_attachment(_black_render_target_texture));
			ReturnIfFalse(_virtual_shadow_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(frame_buffer_desc)));
		}
 
		// Pipeline.
		{
			GraphicsPipelineDesc pipeline_desc;
			pipeline_desc.vertex_shader = _virtual_shadow_vs;
			pipeline_desc.pixel_shader = _virtual_shadow_ps;
			pipeline_desc.binding_layouts.push_back(_virtual_shadow_binding_layout);
			ReturnIfFalse(_virtual_shadow_pipeline = std::unique_ptr<GraphicsPipelineInterface>(
				device->create_graphics_pipeline(pipeline_desc, _virtual_shadow_frame_buffer.get())
			));
		}

		// Binding Set.
		{
#if NANITE
			_virtual_shadow_binding_set_items.resize(7);
			_virtual_shadow_binding_set_items[6] = BindingSetItem::create_texture_uav(0, _vt_physical_shadow_texture);
#else
			_virtual_shadow_binding_set_items.resize(6);
			_virtual_shadow_binding_set_items[5] = BindingSetItem::create_texture_uav(0, _vt_physical_shadow_texture);
#endif
			_virtual_shadow_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::ShadowPassConstant));
			_virtual_shadow_binding_set_items[2] = BindingSetItem::create_structured_buffer_srv(1, check_cast<BufferInterface>(cache->require("vt_shadow_visible_cluster_buffer")));
		}

		// Graphics state.
		{
			_virtual_shadow_graphics_state.binding_sets.resize(1);
			_virtual_shadow_graphics_state.pipeline = _virtual_shadow_pipeline.get();
			_virtual_shadow_graphics_state.frame_buffer = _virtual_shadow_frame_buffer.get();
			_virtual_shadow_graphics_state.viewport_state = ViewportState::create_default_viewport(VT_SHADOW_PAGE_SIZE, VT_SHADOW_PAGE_SIZE);
			_virtual_shadow_graphics_state.indirect_buffer = _vt_shadow_draw_indirect_buffer.get();
		}

		return true;
	}
}
