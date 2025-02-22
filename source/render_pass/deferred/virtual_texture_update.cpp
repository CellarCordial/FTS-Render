#include "virtual_texture_update.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../core/parallel/parallel.h"
// #include "../../scene/light.h"
#include "../../scene/scene.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
 
	bool VirtualTextureUpdatePass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		cache->get_world()->get_global_entity()->get_component<event::AddModel>()->add_event(
			[this]() -> bool
			{
				_update_material_cache = true;
				return true;
			}
		);

		uint32_t current_resolution = LOWEST_TEXTURE_RESOLUTION;
		while (current_resolution < HIGHEST_TEXTURE_RESOLUTION)
		{
			_vt_mipmap_luts.emplace_back().initialize(current_resolution, VT_PAGE_SIZE);
			current_resolution <<= 1;
		}

		// ReturnIfFalse(cache->collect_constants("shadow_tile_num", &_shadow_tile_num));
		// ReturnIfFalse(_virtual_shadow_page_lut.initialize(VIRTUAL_SHADOW_RESOLUTION, VIRTUAL_SHADOW_RESOLUTION)); 

		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(13);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::VirtualGeometryTexturePassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[5] = BindingLayoutItem::create_texture_srv(4);
			binding_layout_items[6] = BindingLayoutItem::create_texture_srv(5);
			binding_layout_items[7] = BindingLayoutItem::create_texture_srv(6);
			binding_layout_items[8] = BindingLayoutItem::create_texture_uav(0);
			binding_layout_items[9] = BindingLayoutItem::create_texture_uav(1);
			binding_layout_items[10] = BindingLayoutItem::create_texture_uav(2);
			binding_layout_items[11] = BindingLayoutItem::create_texture_uav(3);
			binding_layout_items[12] = BindingLayoutItem::create_sampler(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "deferred/virtual_texture_update_cs.hlsl";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
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
			uint32_t virtual_shadow_resolution_in_page = VIRTUAL_SHADOW_RESOLUTION / VIRTUAL_SHADOW_PAGE_SIZE;
			ReturnIfFalse(_shadow_tile_info_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_structured_buffer(
					sizeof(ShadowTileInfo) * virtual_shadow_resolution_in_page * virtual_shadow_resolution_in_page, 
					sizeof(ShadowTileInfo), 
					"shadow_tile_info_buffer"
				)
			)));
			cache->collect(_shadow_tile_info_buffer, ResourceType::Buffer);
		}

		// Texture.
		{	
			ReturnIfFalse(_vt_indirect_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_shader_resource_texture(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RG32_UINT,
					"vt_indirect_texture"
				)
			)));
			cache->collect(_vt_indirect_texture, ResourceType::Texture);

			const Format formats[Material::TextureType_Num] = {
				Format::RGBA16_FLOAT, Format::RGBA32_FLOAT, Format::RGBA8_UNORM, Format::R11G11B10_FLOAT
			};

			for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
			{
				ReturnIfFalse(_vt_physical_textures[ix] = std::shared_ptr<TextureInterface>(device->create_texture(
					TextureDesc::create_render_target_texture(
						CLIENT_WIDTH,
						CLIENT_HEIGHT,
						formats[ix],
						VTPhysicalTable::get_texture_name(ix)
					)
				)));
				cache->collect(_vt_physical_textures[ix], ResourceType::Texture);
			}
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(13);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::VirtualGeometryTexturePassConstant));
			binding_set_items[1] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("vt_tile_uv_texture")));
			binding_set_items[2] = BindingSetItem::create_texture_srv(1, _vt_indirect_texture);
			binding_set_items[3] = BindingSetItem::create_texture_srv(2, _vt_physical_textures[0]);
			binding_set_items[4] = BindingSetItem::create_texture_srv(3, _vt_physical_textures[1]);
			binding_set_items[5] = BindingSetItem::create_texture_srv(4, _vt_physical_textures[2]);
			binding_set_items[6] = BindingSetItem::create_texture_srv(5, _vt_physical_textures[3]);
			binding_set_items[7] = BindingSetItem::create_texture_srv(6, check_cast<TextureInterface>(cache->require("world_space_tangent_texture")));
			binding_set_items[8] = BindingSetItem::create_texture_uav(0, check_cast<TextureInterface>(cache->require("world_space_normal_texture")));
			binding_set_items[9] = BindingSetItem::create_texture_uav(1, check_cast<TextureInterface>(cache->require("base_color_texture")));
			binding_set_items[10] = BindingSetItem::create_texture_uav(2, check_cast<TextureInterface>(cache->require("pbr_texture")));
			binding_set_items[11] = BindingSetItem::create_texture_uav(3, check_cast<TextureInterface>(cache->require("emissive_texture")));
			binding_set_items[12] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
            ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
                BindingSetDesc{ .binding_items = binding_set_items },
                _binding_layout
            )));
		}

		// Compute state.
		{
			_compute_state.binding_sets.push_back(_binding_set.get());
			_compute_state.pipeline = _pipeline.get();
		}

        _pass_constant.vt_page_size = VT_PAGE_SIZE;
        _pass_constant.vt_physical_texture_size = VT_PHYSICAL_TEXTURE_RESOLUTION;
 
		return true;
	}

	bool VirtualTextureUpdatePass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		if (_update_material_cache)
		{
			ReturnIfFalse(cache->get_world()->each<Material>(
				[this](Entity* entity, Material* material) -> bool
				{
					for (uint32_t ix = 0; ix < material->submaterials.size(); ++ix)
					{
						_material_cache.emplace_back(MaterialCache{
							.submaterial = &material->submaterials[ix],
							.model_name = entity->get_component<std::string>(),
							.submesh_id = ix,
							.texture_resolution = material->image_resolution
						});
					}
					return true;
				}
			));
			_update_material_cache = false;
		}


		ReturnIfFalse(cmdlist->open());
		
        if (SceneSystem::loaded_submesh_count != 0)
		{
			if (_finish_pass_thread_id != INVALID_SIZE_64)
			{
				if (!parallel::thread_finished(_finish_pass_thread_id)) std::this_thread::yield();
				ReturnIfFalse(parallel::thread_success(_finish_pass_thread_id));
			}

			// _shadow_tile_num = static_cast<uint32_t>(shadow_tile_infos.size());

			// ReturnIfFalse(cmdlist->write_buffer(
			// 	_shadow_tile_info_buffer.get(), 
			// 	shadow_tile_infos.data(), 
			// 	_shadow_tile_num * sizeof(ShadowTileInfo)
			// ));

			for (const auto& info : _virtual_texture_position_infos)
			{
				for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
				{
					TextureSlice dst_slice = TextureSlice{
						.x = info.page_physical_pos_in_page.x * VT_PAGE_SIZE,
						.y = info.page_physical_pos_in_page.y * VT_PAGE_SIZE,
						.width = VT_PAGE_SIZE,
						.height = VT_PAGE_SIZE
					};
					TextureSlice src_slice = TextureSlice{
						.x = info.page->base_position.x,
						.y = info.page->base_position.y,
						.width = VT_PAGE_SIZE,
						.height = VT_PAGE_SIZE
					};

					if (info.texture[ix]) 
					{
						// 若为 nullptr, 相应的 factor 会是 0, 不必担心会因为没有覆盖 physical texture 中的 page 而担心. 
						cmdlist->copy_texture(_vt_physical_textures[ix].get(), dst_slice, info.texture[ix], src_slice);
					}
				}
			}
			
			ReturnIfFalse(cmdlist->write_texture(
				_vt_indirect_texture.get(), 
				0, 
				0, 
				reinterpret_cast<uint8_t*>(_vt_indirect_table.get_data()), 
				_vt_indirect_table.get_data_size()
			));

			uint2 thread_group_num = {
				static_cast<uint32_t>((align(CLIENT_WIDTH, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
				static_cast<uint32_t>((align(CLIENT_HEIGHT, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
			};

			ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y));
		}

		ReturnIfFalse(cmdlist->close());
		
        return true;
	}

	bool VirtualTextureUpdatePass::finish_pass(RenderResourceCache* cache)
	{
		if (SceneSystem::loaded_submesh_count != 0)
		{
			_finish_pass_thread_id = parallel::begin_thread(
				[this, cache]() -> bool
				{
					World* world = cache->get_world();

					_virtual_texture_position_infos.clear();
					std::shared_ptr<BufferInterface> vt_page_info_read_back_buffer = 
						check_cast<BufferInterface>(cache->require("vt_page_info_read_back_buffer"));
					VTPageInfo* vt_page_data = static_cast<VTPageInfo*>(vt_page_info_read_back_buffer->map(CpuAccessMode::Read));
					
					
					// std::shared_ptr<BufferInterface> virtual_shadow_page_read_back_buffer = 
					// 		check_cast<BufferInterface>(cache->require("virtual_shadow_page_read_back_buffer"));
					
					// uint2* shadow_tile_data = static_cast<uint2*>(_virtual_shadow_page_read_back_buffer->map(CpuAccessMode::Read));
					
					// uint32_t virtual_shadow_tile_num = VIRTUAL_SHADOW_RESOLUTION / VIRTUAL_SHADOW_PAGE_SIZE;
					// DirectionalLight* light = world->get_global_entity()->get_component<DirectionalLight>();
					// float3 L = normalize(-light->get_position());
					// float3 frustum_right = normalize(cross(float3(0.0f, 1.0f, 0.0f), L));
					// float3 frustum_up = cross(L, frustum_right);
					
					// std::mutex tile_info_mutex;
		
					std::mutex position_info_mutex;
					parallel::parallel_for(
						[&](uint64_t x, uint64_t y) -> void
						{
							uint32_t offset = static_cast<uint32_t>(x + y * CLIENT_WIDTH);
			
							// const uint2& tile_id = *(shadow_tile_data + offset);
			
							// if (tile_id != uint2(INVALID_SIZE_32, INVALID_SIZE_32))
							// {
							// 	VTPage* page = _virtual_shadow_page_lut.query_page(tile_id, 0);
							// 	uint2 page_physical_pos_in_page;
			
							// 	{
							// 		std::lock_guard lock(tile_info_mutex);
			
							// 		VTPage::LoadFlag flag = page->flag;
							// 		page_physical_pos_in_page = _physical_shadow_table.add_page(page);
			
							// 		if (flag == VTPage::LoadFlag::Unload)
							// 		{
							// 			float3 offset = 
							// 				frustum_right * (static_cast<float>(tile_id.x) / virtual_shadow_tile_num) * light->orthographic_size +
							// 				frustum_up * (static_cast<float>(tile_id.y) / virtual_shadow_tile_num) * light->orthographic_size;
							// 			shadow_tile_infos.emplace_back(ShadowTileInfo{
							// 				.id = tile_id,
							// 				.view_matrix = look_at_left_hand(
							// 					light->get_position() + offset,
							// 					offset,
							// 					float3(0.0f, 1.0f, 0.0f)
							// 				)
							// 			});
							// 		}
							// 	}
							// }
			
							
							const VTPageInfo& info = *(vt_page_data + offset);
							if (info.geometry_id != INVALID_SIZE_32 && info.page_id_mip_level != INVALID_SIZE_32)
							{
								uint2 page_id;
								uint32_t mip_level;
								uint32_t submesh_global_id = info.geometry_id;
								info.get_data(page_id, mip_level);
								
								const MaterialCache& material_cache = _material_cache[submesh_global_id];

								uint32_t lut_index = std::log2(material_cache.texture_resolution / LOWEST_TEXTURE_RESOLUTION);
								VTPage* page = _vt_mipmap_luts[lut_index].query_page(page_id, mip_level);

								
								VirtualTexturePositionInfo position_info{};
								position_info.page = page;
								for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
								{
									if (material_cache.submaterial->images[ix].is_valid())
									{
										position_info.texture[ix] = check_cast<TextureInterface>(
											cache->require(get_geometry_texture_name(
												material_cache.submesh_id, 
												ix, 
												page->mip_level, 
												*material_cache.model_name
											).c_str())
										).get();
									}
								}

								{
									std::lock_guard lock(position_info_mutex);

									VTPage::LoadFlag flag = page->flag;
									position_info.page_physical_pos_in_page = _vt_physical_table.add_page(page);

									if (flag == VTPage::LoadFlag::Unload)
									{
										_virtual_texture_position_infos.push_back(position_info);
									}
								}
								
								_vt_indirect_table.set_page(
									uint2(static_cast<uint32_t>(x), static_cast<uint32_t>(y)), 
									position_info.page_physical_pos_in_page
								);
							}
						}, 
						CLIENT_WIDTH,
						CLIENT_HEIGHT
					);
					// virtual_shadow_page_read_back_buffer->unmap();
					vt_page_info_read_back_buffer->unmap();

					return true;
				}
			);
		}
		return true;
	}
}
