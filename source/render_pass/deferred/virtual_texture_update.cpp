#include "virtual_texture_update.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../core/parallel/parallel.h"
#include "../../scene/scene.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>
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

		_geometry_texture_heap = check_cast<HeapInterface>(cache->require("geometry_texture_heap"));
		_vt_page_read_back_buffer = check_cast<BufferInterface>(cache->require("vt_page_read_back_buffer"));

		uint32_t vt_feed_back_scale_factor = 0;
		uint32_t* vt_feed_back_scale_factor_ptr = &vt_feed_back_scale_factor;
		ReturnIfFalse(cache->require_constants("vt_feed_back_scale_factor", (void**)&vt_feed_back_scale_factor_ptr));

		_vt_feed_back_resolution = { CLIENT_WIDTH / vt_feed_back_scale_factor, CLIENT_HEIGHT / vt_feed_back_scale_factor };
		_vt_indirect_table.initialize(_vt_feed_back_resolution.x, _vt_feed_back_resolution.y);

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

		// Texture.
		{	

			const Format formats[Material::TextureType_Num] = {
				Format::RGBA16_FLOAT, Format::RGBA32_FLOAT, Format::RGBA8_UNORM, Format::R11G11B10_FLOAT
			};

			for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
			{
				ReturnIfFalse(_vt_physical_textures[ix] = std::shared_ptr<TextureInterface>(device->create_texture(
					TextureDesc::create_tiled_shader_resource_texture(
						CLIENT_WIDTH,
						CLIENT_HEIGHT,
						formats[ix],
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
								region.byte_offset = texture_desc.offset_in_heap + (mip0_size >> (mip - 1));

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
			if (_finish_pass_thread_id != INVALID_SIZE_64)
			{
				if (!parallel::thread_finished(_finish_pass_thread_id)) std::this_thread::yield();
				ReturnIfFalse(parallel::thread_success(_finish_pass_thread_id));
			}

			uint2* mapped_data = static_cast<uint2*>(_vt_page_read_back_buffer->map(CpuAccessMode::Read));
			
			uint32_t vt_page_num = _vt_feed_back_resolution.x * _vt_feed_back_resolution.y;
			_vt_feed_back_data.resize(vt_page_num);
			memcpy(_vt_feed_back_data.data(), mapped_data, vt_page_num * sizeof(uint2)); 

			_vt_page_read_back_buffer->unmap();

			
			std::array<TextureTilesMapping, Material::TextureType_Num> tile_mappings;
			
			for (uint32_t ix = 0; ix < _vt_feed_back_data.size(); ++ix)
			{
				const auto& data = _vt_feed_back_data[ix];
				if (data.x == INVALID_SIZE_32 && data.y == INVALID_SIZE_32) continue;

				VTPage page;
				page.geometry_id = data.x;
				page.coordinate_mip_level = data.y;

				if (!_vt_physical_table.check_page_loaded(page))
				{
					page.physical_position_in_page = _vt_physical_table.get_new_position();
				}
				else
				{
					for (uint32_t jx = 0; jx < Material::TextureType_Num; ++jx)
					{
						auto [region, pixel_size_row_page_num] = _geometry_texture_region_cache[create_texture_region_cache_key(
							page.geometry_id, page.get_mip_level(), jx
						)];

						uint32_t pixel_size = pixel_size_row_page_num >> 16;
						uint32_t row_page_num = pixel_size_row_page_num & 0xffff;
					
						uint2 coordinate = page.get_coordinate_in_page();
						region.x = page.physical_position_in_page.x * VT_PAGE_SIZE * pixel_size;
						region.y = page.physical_position_in_page.y * VT_PAGE_SIZE * pixel_size;
						region.byte_offset += (coordinate.x + coordinate.y * row_page_num) * VT_PAGE_SIZE * pixel_size;
					
						tile_mappings[jx].regions.emplace_back(region);
					}
				}

				_vt_physical_table.add_page(page);
				_vt_indirect_table.set_page(
					uint2(ix % _vt_feed_back_resolution.x, ix / _vt_feed_back_resolution.x), 
					page.physical_position_in_page
				);
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
		// for (const auto& info : _virtual_texture_position_infos)
		// {
		// 	for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
		// 	{
		// 		TextureSlice dst_slice = TextureSlice{
		// 			.x = info.page_physical_pos_in_page.x * VT_PAGE_SIZE,
		// 			.y = info.page_physical_pos_in_page.y * VT_PAGE_SIZE,
		// 			.width = VT_PAGE_SIZE,
		// 			.height = VT_PAGE_SIZE
		// 		};
		// 		// TextureSlice src_slice = TextureSlice{
		// 		// 	.x = info.page->base_position.x,
		// 		// 	.y = info.page->base_position.y,
		// 		// 	.width = VT_PAGE_SIZE,
		// 		// 	.height = VT_PAGE_SIZE
		// 		// };

		// 		if (info.texture[ix]) 
		// 		{
		// 			// 若为 nullptr, 相应的 factor 会是 0, 不必担心会因为没有覆盖 physical texture 中的 page 而担心. 
		// 			// cmdlist->copy_texture(_vt_physical_textures[ix].get(), dst_slice, info.texture[ix], src_slice);
		// 		}
		// 	}
		// }
		return true;
	}
}
