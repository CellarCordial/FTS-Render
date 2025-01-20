#include "virtual_geometry_texture.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../core/parallel/parallel.h"

namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
 
	bool VirtualGeometryTexturePass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
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
			cs_compile_desc.shader_name = "deferred/virtual_geometry_texture_cs.slang";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			ShaderData cs_data = shader_compile::compile_shader(cs_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.entry = "main";
			cs_desc.shader_type = ShaderType::Compute;
			ReturnIfFalse(_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));
		}

		// Pipeline.
		{
			ComputePipelineDesc pipeline_desc;
			pipeline_desc.compute_shader = _cs.get();
			pipeline_desc.binding_layouts.push_back(_binding_layout.get());
			ReturnIfFalse(_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));
		}

		{	
			ReturnIfFalse(_vt_indirect_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_render_target(
					CLIENT_WIDTH,
					CLIENT_HEIGHT,
					Format::RG32_UINT,
					"vt_indirect_texture"
				)
			)));
			cache->collect(_vt_indirect_texture, ResourceType::Texture);

			for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
			{
				ReturnIfFalse(_vt_physical_textures[ix] = std::shared_ptr<TextureInterface>(device->create_texture(
					TextureDesc::create_render_target(
						CLIENT_WIDTH,
						CLIENT_HEIGHT,
						Format::RGBA32_FLOAT,
						VTPhysicalTable::get_texture_name(ix)
					)
				)));
				cache->collect(_vt_physical_textures[index], ResourceType::Texture);
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
                _binding_layout.get()
            )));
		}

		// Compute state.
		{
			_compute_state.binding_sets.push_back(_binding_set.get());
			_compute_state.pipeline = _pipeline.get();
		}

        _pass_constant.vt_page_size = vt_page_size;
        _pass_constant.vt_physical_texture_size = vt_physical_texture_resolution;
 
		return true;
	}

	bool VirtualGeometryTexturePass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		
		DeviceInterface* device = cmdlist->get_deivce();
		World* world = cache->get_world();
		HANDLE fence_event = CreateEvent(nullptr, false, false, nullptr);

		VTPageInfo* data = static_cast<VTPageInfo*>(
			check_cast<BufferInterface>(cache->require("vt_page_info_buffer"))->map(CpuAccessMode::Read, fence_event)
		);
		
		struct TextureCopyInfo
		{
			uint32_t submesh_id = 0;
			VTPage* page = nullptr;
			uint2 page_physical_pos;
			std::string* model_name = nullptr;
		};

		std::mutex info_mutex;
		std::vector<TextureCopyInfo> copy_infos;

		uint32_t page_info_count = CLIENT_WIDTH * CLIENT_HEIGHT;
		parallel::parallel_for(
			[&](uint64_t x, uint64_t y)
			{
				uint32_t offset = static_cast<uint32_t>(x + y * CLIENT_WIDTH);
				const auto& info = *(data + offset);

				uint2 page_id = uint2(
					((info.page_id_mip_level >> 12) & 0xf << 8) | (info.page_id_mip_level >> 24) & 0xff,
					((info.page_id_mip_level >> 8) & 0xf << 8) | (info.page_id_mip_level >> 16) & 0xff
				);
				uint32_t mip_level = info.page_id_mip_level & 0xff;
				uint32_t mesh_id = info.geometry_id >> 16;
				uint32_t submesh_id = info.geometry_id & 0xffff;


				bool res = world->each<std::string, Mesh, Material>(
					[&](Entity* entity, std::string* model_name, Mesh* mesh, Material* material) -> bool
					{
						if (mesh->mesh_id == mesh_id)
						{
							const auto& submesh = mesh->submeshes[submesh_id];
							
							ReturnIfFalse(world->each<MipmapLUT>(
								[&](Entity* entity, MipmapLUT* mipmap_lut) -> bool
								{
									bool found = false;
									if (mipmap_lut->get_mip0_resolution() == material->image_resolution)
									{
										found = true;

										VTPage* page = mipmap_lut->query_page(page_id, mip_level);
										
										uint2 page_physical_pos;
										{
											std::lock_guard lock(info_mutex);

											VTPage::LoadFlag flag = page->flag;
											page_physical_pos = _vt_physical_table.add_page(page);

											if (flag == VTPage::LoadFlag::Unload)
											{
												copy_infos.emplace_back(TextureCopyInfo{
													.submesh_id = submesh_id,
													.page = page,
													.page_physical_pos = page_physical_pos,
													.model_name = model_name
												});
											}
										}
										_vt_indirect_table.set_page(
											uint2(static_cast<uint32_t>(x), static_cast<uint32_t>(y)), 
											page_physical_pos
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
			}, 
			CLIENT_WIDTH,
			CLIENT_HEIGHT
		);

		for (const auto& info : copy_infos)
		{
			for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
			{
				TextureSlice dst_slice = TextureSlice{
					.x = info.page_physical_pos.x * vt_page_size,
					.y = info.page_physical_pos.y * vt_page_size,
					.width = vt_page_size,
					.height = vt_page_size
				};
				TextureSlice src_slice = TextureSlice{
					.x = info.page->bounds._lower.x,
					.y = info.page->bounds._lower.y,
					.width = info.page->bounds.width(),
					.height = info.page->bounds.height()
				};

				ReturnIfFalse(cmdlist->copy_texture(
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
				));
			}
		}

		const auto& desc = _vt_indirect_texture->get_desc();
		ReturnIfFalse(cmdlist->write_texture(
			_vt_indirect_texture.get(), 
			0, 
			0, 
			reinterpret_cast<uint8_t*>(_vt_indirect_table.get_data()), 
			get_format_info(desc.format).byte_size_per_pixel * desc.width
		));

		uint2 thread_group_num = {
			static_cast<uint32_t>((align(CLIENT_WIDTH, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
			static_cast<uint32_t>((align(CLIENT_HEIGHT, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
		};

		ReturnIfFalse(cmdlist->set_compute_state(_compute_state));
		ReturnIfFalse(cmdlist->dispatch(thread_group_num.x, thread_group_num.y));

		ReturnIfFalse(cmdlist->close());
        return true;
	}
}

