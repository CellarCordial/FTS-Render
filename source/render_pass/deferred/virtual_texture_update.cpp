#include "virtual_texture_update.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../core/parallel/parallel.h"
#include "../../scene/light.h"
#include "../../scene/scene.h"
#include <cstdint>
#include <memory>
#include <mutex>

namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
 
	struct ShadowTileInfo
	{
		uint2 id;
		float4x4 view_matrix;
	};

	struct TextureCopyInfo
	{
		uint32_t submesh_id = 0;
		VTPage* page = nullptr;
		uint2 page_physical_pos_in_page;
		std::string* model_name = nullptr;
	};

	bool VirtualTextureUpdatePass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		ReturnIfFalse(cache->collect_constants("shadow_tile_num", &_shadow_tile_num));
		ReturnIfFalse(_virtual_shadow_page_lut.initialize(VIRTUAL_SHADOW_RESOLUTION, VIRTUAL_SHADOW_RESOLUTION)); 

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
				TextureDesc::create_render_target_texture(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RG32_UINT,
					"vt_indirect_texture"
				)
			)));
			cache->collect(_vt_indirect_texture, ResourceType::Texture);

			const Format formats[Material::TextureType_Num] = {
				Format::RGBA16_FLOAT, Format::RGBA32_FLOAT, Format::RGBA8_UNORM, Format::RGBA16_FLOAT
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
			binding_set_items[1] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("tile_uv_texture")));
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
        if (SceneSystem::loaded_submesh_count == 0)
		{
			// 每个 render pass 都必须有一个有效的 cmdlist.
			ReturnIfFalse(cmdlist->open());
			ReturnIfFalse(cmdlist->close());
			return true;
		}

		ReturnIfFalse(cmdlist->open());

		DeviceInterface* device = cmdlist->get_deivce();
		World* world = cache->get_world();

		std::shared_ptr<BufferInterface> vt_page_info_read_back_buffer = 
			check_cast<BufferInterface>(cache->require("vt_page_info_read_back_buffer"));
		std::shared_ptr<BufferInterface> virtual_shadow_page_read_back_buffer = 
				check_cast<BufferInterface>(cache->require("virtual_shadow_page_read_back_buffer"));

		uint2* shadow_tile_data = static_cast<uint2*>(virtual_shadow_page_read_back_buffer->map(CpuAccessMode::Read));
		VTPageInfo* vt_page_data = static_cast<VTPageInfo*>(vt_page_info_read_back_buffer->map(CpuAccessMode::Read));
		
		std::mutex copy_info_mutex;
		std::mutex tile_info_mutex;
		std::vector<TextureCopyInfo> texture_copy_infos;
		std::vector<ShadowTileInfo> shadow_tile_infos;

		uint32_t virtual_shadow_tile_num = VIRTUAL_SHADOW_RESOLUTION / VIRTUAL_SHADOW_PAGE_SIZE;
		DirectionalLight* light = world->get_global_entity()->get_component<DirectionalLight>();
		float3 L = normalize(-light->get_position());
		float3 frustum_right = normalize(cross(float3(0.0f, 1.0f, 0.0f), L));
		float3 frustum_up = cross(L, frustum_right);

		parallel::parallel_for(
			[&](uint64_t x, uint64_t y) -> void
			{
				uint32_t offset = static_cast<uint32_t>(x + y * CLIENT_WIDTH);

				const uint2& tile_id = *(shadow_tile_data + offset);

				if (tile_id != uint2(INVALID_SIZE_32, INVALID_SIZE_32))
				{
					VTPage* page = _virtual_shadow_page_lut.query_page(tile_id, 0);
					uint2 page_physical_pos_in_page;

					{
						std::lock_guard lock(tile_info_mutex);

						VTPage::LoadFlag flag = page->flag;
						page_physical_pos_in_page = _physical_shadow_table.add_page(page);

						if (flag == VTPage::LoadFlag::Unload)
						{
							float3 offset = 
								frustum_right * (static_cast<float>(tile_id.x) / virtual_shadow_tile_num) * light->orthographic_size +
								frustum_up * (static_cast<float>(tile_id.y) / virtual_shadow_tile_num) * light->orthographic_size;
							shadow_tile_infos.emplace_back(ShadowTileInfo{
								.id = tile_id,
								.view_matrix = look_at_left_hand(
									light->get_position() + offset,
									offset,
									float3(0.0f, 1.0f, 0.0f)
								)
							});
						}
					}
				}

				
				const VTPageInfo& info = *(vt_page_data + offset);
				if (info.geometry_id != INVALID_SIZE_32 && info.page_id_mip_level != INVALID_SIZE_32)
				{
					uint2 page_id;
					uint32_t mip_level;
					uint32_t submesh_id = info.geometry_id;
					info.get_data(page_id, mip_level);
	
					bool res = world->each<std::string, Mesh, Material>(
						[&](Entity* mesh_entity, std::string* model_name, Mesh* mesh, Material* material) -> bool
						{
							// 若 mesh 没有 纹理.
							if (material->image_resolution == 0) return true;

							if (mesh->submesh_base_id <= submesh_id && submesh_id < mesh->submesh_base_id + mesh->submeshes.size())
							{
								const auto& submesh = mesh->submeshes[submesh_id - mesh->submesh_base_id];
	
								bool found = false;
								ReturnIfFalse(world->each<MipmapLUT>(
									[&](Entity* entity, MipmapLUT* mipmap_lut) -> bool
									{
										if (!found && mipmap_lut->get_mip0_resolution() == material->image_resolution)
										{
											found = true;
	
											VTPage* page = mipmap_lut->query_page(page_id, mip_level);
											uint2 page_physical_pos_in_page;
	
											{
												std::lock_guard lock(copy_info_mutex);
	
												VTPage::LoadFlag flag = page->flag;
												page_physical_pos_in_page = _vt_physical_table.add_page(page);
	
												if (flag == VTPage::LoadFlag::Unload)
												{
													texture_copy_infos.emplace_back(TextureCopyInfo{
														.submesh_id = submesh_id - mesh->submesh_base_id,
														.page = page,
														.page_physical_pos_in_page = page_physical_pos_in_page,
														.model_name = model_name
													});
												}
											}
											
											_vt_indirect_table.set_page(
												uint2(static_cast<uint32_t>(x), static_cast<uint32_t>(y)), 
												page_physical_pos_in_page
											);
										}
										return found;
									}
								));
							}
							return true;
						}
					);
					assert(res);
				}
			}, 
			CLIENT_WIDTH,
			CLIENT_HEIGHT
		);
		virtual_shadow_page_read_back_buffer->unmap();
		vt_page_info_read_back_buffer->unmap();

		
		_shadow_tile_num = static_cast<uint32_t>(shadow_tile_infos.size());

		ReturnIfFalse(cmdlist->write_buffer(
			_shadow_tile_info_buffer.get(), 
			shadow_tile_infos.data(), 
			_shadow_tile_num * sizeof(ShadowTileInfo)
		));


		for (const auto& info : texture_copy_infos)
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

				cmdlist->copy_texture(
					_vt_physical_textures[ix].get(), 
					dst_slice, 
					check_cast<TextureInterface>(
						cache->require(get_geometry_texture_name(
							info.submesh_id, 
							ix, 
							info.page->mip_level, 
							*info.model_name
						).c_str())).get(), 
					src_slice
				);
			}
		}

		const auto& desc = _vt_indirect_texture->get_desc();
		uint64_t size = get_format_info(desc.format).size * desc.width * desc.height;
		ReturnIfFalse(size == sizeof(uint2) * _vt_indirect_table.get_data_size());
		
		ReturnIfFalse(cmdlist->write_texture(
			_vt_indirect_texture.get(), 
			0, 
			0, 
			reinterpret_cast<uint8_t*>(_vt_indirect_table.get_data()), 
			size
		));

		uint2 thread_group_num = {
			static_cast<uint32_t>((align(CLIENT_WIDTH, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
			static_cast<uint32_t>((align(CLIENT_HEIGHT, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
		};

		ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y));

		ReturnIfFalse(cmdlist->close());
        return true;
	}
}

