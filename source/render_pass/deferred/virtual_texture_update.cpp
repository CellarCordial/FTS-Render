#include "virtual_texture_update.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/scene.h"
#include "../../scene/light.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

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

		ReturnIfFalse(cache->collect_constants("update_shadow_pages", &_update_shadow_pages));
		_geometry_texture_heap = check_cast<HeapInterface>(cache->require("geometry_texture_heap"));
		_vt_feed_back_read_back_buffer = check_cast<BufferInterface>(cache->require("vt_feed_back_read_back_buffer"));

		_vt_feed_back_resolution = { CLIENT_WIDTH / VT_FEED_BACK_SCALE_FACTOR, CLIENT_HEIGHT / VT_FEED_BACK_SCALE_FACTOR };
		_vt_indirect_table.initialize(_vt_feed_back_resolution.x, _vt_feed_back_resolution.y);

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
			cs_compile_desc.defines.push_back("VT_FEED_BACK_SCALE_FACTOR=" + std::to_string(VT_FEED_BACK_SCALE_FACTOR));
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
			uint32_t axis_vt_shadow_page_num = VT_PHYSICAL_SHADOW_RESOLUTION / VT_SHADOW_PAGE_SIZE;
			ReturnIfFalse(_vt_shadow_page_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
					BufferDesc::create_structured_buffer(
						sizeof(uint2) * axis_vt_shadow_page_num * axis_vt_shadow_page_num, 
						sizeof(uint2),
						"vt_shadow_page_buffer"
					)
				)
			));
			cache->collect(_vt_shadow_page_buffer, ResourceType::Buffer);
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

			ReturnIfFalse(_vt_indirect_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					_vt_feed_back_resolution.x,
					_vt_feed_back_resolution.y,
					Format::RG32_UINT,
					"vt_indirect_texture"
				)
			)));
			cache->collect(_vt_indirect_texture, ResourceType::Texture);
		}

		// Binding Set.
		{
			ReturnIfFalse(Material::TextureType_Num == 4);

			BindingSetItemArray binding_set_items(13);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::VirtualGeometryTexturePassConstant));
			binding_set_items[1] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("vt_page_uv_texture")));
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
		if (_update_texture_region_cache)
		{
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
								get_geometry_texture_name(submesh_id, type, *name).c_str()
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


		ReturnIfFalse(cmdlist->open());
		
        if (SceneSystem::loaded_submesh_count != 0)
		{
			uint3* mapped_data = static_cast<uint3*>(_vt_feed_back_read_back_buffer->map(CpuAccessMode::Read));
			
			_vt_feed_back_data.resize(_vt_feed_back_resolution.x * _vt_feed_back_resolution.y);
			memcpy(_vt_feed_back_data.data(), mapped_data, static_cast<uint32_t>(_vt_feed_back_data.size()) * sizeof(uint3)); 

			_vt_feed_back_read_back_buffer->unmap();

			_update_shadow_pages.clear();
			std::array<TextureTilesMapping, Material::TextureType_Num> tile_mappings;
			
			for (uint32_t ix = 0; ix < _vt_feed_back_data.size(); ++ix)
			{
				const auto& data = _vt_feed_back_data[ix];
				if (data.z != INVALID_SIZE_32)
				{
					VTShadowPage page{ .tile_id = uint2(data.z >> 16, data.z & 0xffff) };

					if (!_physical_shadow_table.check_page_loaded(page))
					{
						page.physical_position_in_page = _physical_shadow_table.get_new_position();
						_update_shadow_pages.push_back(uint2(
							(page.tile_id.x << 16) | (page.tile_id.y & 0xffff), 
							(page.physical_position_in_page.x << 16) | (page.physical_position_in_page.y & 0xffff) 
						));
					}
					_physical_shadow_table.add_page(page);
				}


				if (data.x == INVALID_SIZE_32 || data.y == INVALID_SIZE_32) continue;

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
				_vt_indirect_table.set_page(ix, page.physical_position_in_page);
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
				_vt_shadow_page_buffer.get(), 
				_update_shadow_pages.data(), 
				_update_shadow_pages.size() * sizeof(uint2)
			));

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

			ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constant));
		}

		ReturnIfFalse(cmdlist->close());
		
        return true;
	}
}
