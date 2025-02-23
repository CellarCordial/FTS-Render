#include "virtual_texture_update.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../core/parallel/parallel.h"
// #include "../../scene/light.h"
#include "../../scene/scene.h"
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16
#define THREAD_GROUP_SIZE_Y 16
 
	bool VirtualTextureUpdatePass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		cache->get_world()->get_global_entity()->get_component<event::AddModel>()->add_event(
			[this]() -> bool
			{
				_update_submesh_name_cache = true;
				return true;
			}
		);
		
		_vt_physical_tile_lru_cache_read_back_buffer = 
			check_cast<BufferInterface>(cache->require("vt_physical_tile_lru_cache_read_back_buffer"));

		ReturnIfFalse(cache->collect_constants("physical_tile_lru_cache", &_physical_tile_lru_cache));

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
		}

		// Texture.
		{	

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
						get_vt_physical_texture_name(ix)
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
			binding_set_items[2] = BindingSetItem::create_texture_srv(1, check_cast<TextureInterface>(cache->require("vt_indirect_texture")));
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
		if (_update_submesh_name_cache)
		{
			cache->get_world()->each<std::string, Mesh>(
				[this](Entity* entity, std::string* name, Mesh* mesh) -> bool
				{
					for (uint32_t ix = 0; ix < mesh->submeshes.size(); ++ix)
					{
						_submesh_name_cache.emplace_back(std::make_pair(name, ix));
					}
					return true;
				}
			);
			_update_submesh_name_cache = false;
		}


		ReturnIfFalse(cmdlist->open());
		
        if (SceneSystem::loaded_submesh_count != 0)
		{
			PhysicalTileLruCache* lru_cache = 
				static_cast<PhysicalTileLruCache*>(_vt_physical_tile_lru_cache_read_back_buffer->map(CpuAccessMode::Read));
			for (uint32_t ix = 0; ix < lru_cache->new_tile_count; ++ix)
			{
				const PhysicalTile& tile = lru_cache->tiles[lru_cache->new_tiles[ix]];

				const auto& [submesh_name, submesh_id] = _submesh_name_cache[tile.geometry_id];
				uint2 page_id = uint2(
					(tile.vt_page_id_mip_level >> 20) & 0xfff,
					(tile.vt_page_id_mip_level >> 8) & 0xfff
				);
				uint32_t mip_level = tile.vt_page_id_mip_level & 0xff;

        		uint2 texture_position = page_id / std::pow(2, mip_level);
				TextureSlice dst_slice = TextureSlice{
					.x = tile.physical_postion.x * VT_PAGE_SIZE,
					.y = tile.physical_postion.y * VT_PAGE_SIZE,
					.width = VT_PAGE_SIZE,
					.height = VT_PAGE_SIZE
				};
				TextureSlice src_slice = TextureSlice{
					.x = texture_position.x * VT_PAGE_SIZE,
					.y = texture_position.y * VT_PAGE_SIZE,
					.width = VT_PAGE_SIZE,
					.height = VT_PAGE_SIZE
				};

				for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
				{
					auto texture = check_cast<TextureInterface>(
						cache->require(get_geometry_texture_name(submesh_id, ix, mip_level, *submesh_name).c_str())
					);

					if (texture) 
					{
						// 若为 nullptr, 相应的 factor 会是 0, 不必担心会因为没有覆盖 physical texture 中的 page 而担心. 
						cmdlist->copy_texture(_vt_physical_textures[ix].get(), dst_slice, texture.get(), src_slice);
					}
				}
			}
			
			uint2 thread_group_num = {
				static_cast<uint32_t>((align(CLIENT_WIDTH, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
				static_cast<uint32_t>((align(CLIENT_HEIGHT, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
			};

			ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y));


			_finish_pass_thread_id = parallel::begin_thread(
				[this, lru_cache]() -> bool
				{					
					_physical_tile_lru_cache->update(lru_cache);
					_vt_physical_tile_lru_cache_read_back_buffer->unmap();
					return true;
				}
			);
		}

		ReturnIfFalse(cmdlist->close());
		
        return true;
	}

	bool VirtualTextureUpdatePass::finish_pass(RenderResourceCache* cache)
	{
		if (SceneSystem::loaded_submesh_count != 0)
		{
			if (_finish_pass_thread_id != INVALID_SIZE_64)
			{
				if (!parallel::thread_finished(_finish_pass_thread_id)) std::this_thread::yield();
				ReturnIfFalse(parallel::thread_success(_finish_pass_thread_id));
			}
		}
		return true;
	}
}
