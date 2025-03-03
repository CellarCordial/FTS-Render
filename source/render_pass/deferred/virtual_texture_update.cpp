#include "virtual_texture_update.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/scene.h"
#include <cstring>


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16

	uint64_t create_texture_region_cache_key(uint32_t geometry_id, uint32_t mip_level, uint32_t material_type)
	{
		return (uint64_t(geometry_id) << 32) | (mip_level << 16) | (material_type & 0xffff);
	}
 
	bool VirtualTextureUpdatePass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		cache->get_world()->get_global_entity()->get_component<event::AddModel>()->add_event(
			[this]() -> bool
			{
				_update_texture_region_cache = true;
				return true;
			}
		);

		_geometry_texture_heap = check_cast<HeapInterface>(cache->require("geometry_texture_heap"));
		_vt_feed_back_read_back_buffer = check_cast<BufferInterface>(cache->require("vt_feed_back_read_back_buffer"));

		_vt_feed_back_resolution = { CLIENT_WIDTH / VT_FEED_BACK_SCALE_FACTOR, CLIENT_HEIGHT / VT_FEED_BACK_SCALE_FACTOR };
		_vt_feed_back_data.resize(_vt_feed_back_resolution.x * _vt_feed_back_resolution.y, uint3(INVALID_SIZE_32));

		ReturnIfFalse(cache->collect_constants("vt_new_shadow_pages", &_vt_new_shadow_pages));


        _pass_constant.vt_page_size = VT_PAGE_SIZE;
        _pass_constant.vt_physical_texture_size = VT_PHYSICAL_TEXTURE_RESOLUTION;
		_pass_constant.vt_texture_id_offset = 0;

		for (int32_t mip = 0; mip < VT_TEXTURE_MIP_LEVELS; ++mip)
		{
			_pass_constant.vt_axis_mip_tile_num[mip / 4][mip % 4] = (HIGHEST_TEXTURE_RESOLUTION >> mip) / VT_PAGE_SIZE;
			_pass_constant.vt_texture_mip_offset[mip / 4][mip % 4] = _pass_constant.vt_texture_id_offset;
			_pass_constant.vt_texture_id_offset += std::pow(4, VT_TEXTURE_MIP_LEVELS - mip - 1);
		}
 
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(14);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::VirtualTextureUpdatePassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[5] = BindingLayoutItem::create_texture_srv(4);
			binding_layout_items[6] = BindingLayoutItem::create_texture_srv(5);
			binding_layout_items[7] = BindingLayoutItem::create_texture_srv(6);
			binding_layout_items[8] = BindingLayoutItem::create_texture_srv(7);
			binding_layout_items[9] = BindingLayoutItem::create_texture_uav(0);
			binding_layout_items[10] = BindingLayoutItem::create_texture_uav(1);
			binding_layout_items[11] = BindingLayoutItem::create_texture_uav(2);
			binding_layout_items[12] = BindingLayoutItem::create_texture_uav(3);
			binding_layout_items[13] = BindingLayoutItem::create_sampler(0);
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
			cs_compile_desc.defines.push_back("VT_TEXTURE_MIP_LEVELS_UINT_4=" + std::to_string(VT_TEXTURE_MIP_LEVELS / 4 + 1));
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

		// Texture.
		{
			for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
			{
				ReturnIfFalse(_vt_physical_textures[ix] = std::shared_ptr<TextureInterface>(device->create_texture(
					TextureDesc::create_tiled_shader_resource_texture(
						VT_PHYSICAL_TEXTURE_RESOLUTION,
						VT_PHYSICAL_TEXTURE_RESOLUTION,
						Format::RGBA8_UNORM,
						VTPhysicalTable::get_texture_name(ix)
					)
				)));
				cache->collect(_vt_physical_textures[ix], ResourceType::Texture);
			}

			ReturnIfFalse(_vt_shadow_indirect_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					_vt_feed_back_resolution.x,
					_vt_feed_back_resolution.y,
					Format::RG32_UINT,
					"vt_shadow_indirect_texture"
				)
			)));
			cache->collect(_vt_shadow_indirect_texture, ResourceType::Texture);
		}

		// Binding Set.
		{
			ReturnIfFalse(Material::TextureType_Num == 4);

			_binding_set_items.resize(14);
			_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::VirtualTextureUpdatePassConstant));
			_binding_set_items[2] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require("vt_tile_uv_texture")));
			_binding_set_items[3] = BindingSetItem::create_texture_srv(2, _vt_physical_textures[0]);
			_binding_set_items[4] = BindingSetItem::create_texture_srv(3, _vt_physical_textures[1]);
			_binding_set_items[5] = BindingSetItem::create_texture_srv(4, _vt_physical_textures[2]);
			_binding_set_items[6] = BindingSetItem::create_texture_srv(5, _vt_physical_textures[3]);
			_binding_set_items[7] = BindingSetItem::create_texture_srv(6, check_cast<TextureInterface>(cache->require("world_space_tangent_texture")));
			_binding_set_items[8] = BindingSetItem::create_texture_srv(7, check_cast<TextureInterface>(cache->require("geometry_uv_mip_id_texture")));
			_binding_set_items[9] = BindingSetItem::create_texture_uav(0, check_cast<TextureInterface>(cache->require("world_space_normal_texture")));
			_binding_set_items[10] = BindingSetItem::create_texture_uav(1, check_cast<TextureInterface>(cache->require("base_color_texture")));
			_binding_set_items[11] = BindingSetItem::create_texture_uav(2, check_cast<TextureInterface>(cache->require("pbr_texture")));
			_binding_set_items[12] = BindingSetItem::create_texture_uav(3, check_cast<TextureInterface>(cache->require("emissive_texture")));
			_binding_set_items[13] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
		}

		// Compute state.
		{
			_compute_state.binding_sets.resize(1);
			_compute_state.pipeline = _pipeline.get();
		}

		return true;
	}

	bool VirtualTextureUpdatePass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(update_texture_region_cache(cmdlist->get_deivce(), cache));

		ReturnIfFalse(cmdlist->open());
		
        if (SceneSystem::loaded_submesh_count != 0)
		{
			uint3* mapped_data = static_cast<uint3*>(_vt_feed_back_read_back_buffer->map(CpuAccessMode::Read));
			memcpy(_vt_feed_back_data.data(), mapped_data, static_cast<uint32_t>(_vt_feed_back_data.size()) * sizeof(uint3)); 
			_vt_feed_back_read_back_buffer->unmap();

			std::array<TextureTilesMapping, Material::TextureType_Num> tile_mappings;

			for (uint32_t ix = 0; ix < _vt_feed_back_data.size(); ++ix)
			{
				const auto& data = _vt_feed_back_data[ix];

				if (data.z != INVALID_SIZE_32)
				{
					VTShadowPage page;
					page.tile_id = { data.z >> 16, data.z & 0xffff };

					if (!_vt_physical_shadow_table.check_page_loaded(page))
					{
						page.physical_position_in_page = _vt_physical_shadow_table.get_new_position();
						_vt_new_shadow_pages.push_back(page);
					}
					_vt_physical_shadow_table.add_page(page);

					// TODO: update shadow tile to pages.
				}

				if (data.x == INVALID_SIZE_32 || data.y == INVALID_SIZE_32)  continue;

				VTPage page;
				page.geometry_id = data.x;
				page.coordinate_mip_level = data.y;

				if (!_vt_physical_table.check_page_loaded(page))
				{
					page.physical_position_in_page = _vt_physical_table.get_new_position();

					for (uint32_t jx = 0; jx < Material::TextureType_Num; ++jx)
					{
						auto iter = _geometry_texture_region_cache.find(create_texture_region_cache_key(
							page.geometry_id, page.get_mip_level(), jx
						));

						if (iter == _geometry_texture_region_cache.end()) continue;

						auto [region, pixel_size_row_page_num] = iter->second;
						uint32_t pixel_size = pixel_size_row_page_num >> 16;
						uint32_t row_page_num = pixel_size_row_page_num & 0xffff;
					
						uint2 coordinate = page.get_coordinate_in_page();
						region.x = page.physical_position_in_page.x * VT_PAGE_SIZE;
						region.y = page.physical_position_in_page.y * VT_PAGE_SIZE;
						region.byte_offset += (coordinate.x + coordinate.y * row_page_num) * 
											  VT_PAGE_SIZE * VT_PAGE_SIZE * pixel_size;
					
						tile_mappings[jx].regions.emplace_back(region);
					}
				}
				_vt_physical_table.add_page(page);

				uint32_t mip = page.get_mip_level();
				uint2 tile_id = page.get_coordinate_in_page();
				uint32_t indirect_index = page.geometry_id * _pass_constant.vt_texture_id_offset + 
										  _pass_constant.vt_texture_mip_offset[mip / 4][mip % 4] + 
										  tile_id.y * _pass_constant.vt_axis_mip_tile_num[mip / 4][mip % 4] +
										  tile_id.x;

				_vt_indirect_data[indirect_index] = (page.physical_position_in_page.x << 16) |
													(page.physical_position_in_page.y & 0xffff);
			}

			for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
			{
				if (tile_mappings[ix].regions.empty()) continue;

				tile_mappings[ix].heap = _geometry_texture_heap.get();

				cmdlist->get_deivce()->update_texture_tile_mappings(
					_vt_physical_textures[ix].get(), 
					&tile_mappings[ix], 
					1,
					CommandQueueType::Compute 
				);
			}

			ReturnIfFalse(cmdlist->write_buffer(
				_vt_indirect_buffer.get(), 
				_vt_indirect_data.data(), 
				_vt_indirect_data.size() * sizeof(uint32_t)
			));
			
			uint2 thread_group_num = {
				static_cast<uint32_t>((align(CLIENT_WIDTH, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
				static_cast<uint32_t>((align(CLIENT_HEIGHT, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
			};

			ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constant));
		}

		ReturnIfFalse(cmdlist->close());
		
        return true;
	}

	bool VirtualTextureUpdatePass::update_texture_region_cache(DeviceInterface* device, RenderResourceCache* cache)
	{
		if (_update_texture_region_cache)
		{
			_vt_indirect_data.resize(_pass_constant.vt_texture_id_offset * SceneSystem::loaded_submesh_count, INVALID_SIZE_32);

			// Buffer.
			{
				ReturnIfFalse(_vt_indirect_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
					BufferDesc::create_structured_buffer(
						sizeof(uint32_t) * _pass_constant.vt_texture_id_offset * SceneSystem::loaded_submesh_count,
						sizeof(uint32_t),
						"vt_indirect_buffer"
					)
				)));
				cache->collect(_vt_indirect_buffer, ResourceType::Buffer);
			}

			// Binding Set.
			{
				_binding_set_items[1] = BindingSetItem::create_structured_buffer_srv(0, _vt_indirect_buffer);
				ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
					BindingSetDesc{ .binding_items = _binding_set_items },
					_binding_layout
				)));

				_compute_state.binding_sets[0] = _binding_set.get();
			}


			bool res = cache->get_world()->each<std::string, Mesh, Material>(
				[this, cache](Entity* entity, std::string* name, Mesh* mesh, Material* material) -> bool
				{
					for (uint32_t ix = 0; ix < material->submaterials.size(); ++ix)
					{
						if (material->image_resolution == 0) continue;

						uint32_t submesh_id = mesh->submesh_global_base_id + ix;
						for (uint32_t type = 0; type < Material::TextureType_Num; ++type)
						{
							if (!material->submaterials[ix].images[type].is_valid()) continue;
							
							const auto& texture_desc = check_cast<TextureInterface>(cache->require(
								get_geometry_texture_name(ix, type, *name).c_str()
							))->get_desc();

							uint32_t row_size_in_page = texture_desc.width / VT_PAGE_SIZE;
							uint32_t pixel_size = get_format_info(texture_desc.format).size;

							uint32_t mip0_size = texture_desc.width * texture_desc.height * pixel_size;

							TextureTilesMapping::Region region;
							region.width = VT_PAGE_SIZE;
							region.height = VT_PAGE_SIZE;
							region.byte_offset = texture_desc.offset_in_heap;

							uint64_t key = create_texture_region_cache_key(submesh_id, 0, type);
							_geometry_texture_region_cache[key] = std::make_pair(region, (pixel_size << 16) | (row_size_in_page & 0xffff));

							for (uint32_t mip = 1; mip < texture_desc.mip_levels; ++mip)
							{
								region.byte_offset += (mip0_size >> ((mip - 1) * 2));

								uint64_t key = create_texture_region_cache_key(submesh_id, mip, type);
								_geometry_texture_region_cache[key] = std::make_pair(region, (pixel_size << 16) | ((row_size_in_page >> mip) & 0xffff));
							}
						}
					}
					return true;
				}
			);
			ReturnIfFalse(res);

			_update_texture_region_cache = false;
		}
		return true;
	}

}
