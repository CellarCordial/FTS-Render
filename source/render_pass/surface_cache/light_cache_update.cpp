// #include "light_cache_update.h"
// #include "../../scene/geometry.h"
// #include "../../scene/surface_cache.h"
// #include "../../core/tools/check_cast.h"
// #include "../../core/parallel/parallel.h"

// #include <mutex>


// namespace fantasy
// {
// #define THREAD_GROUP_SIZE_X 16
// #define THREAD_GROUP_SIZE_Y 16
 
// 	bool LightCacheUpdatePass::compile(DeviceInterface* device, RenderResourceCache* cache)
// 	{
//         ReturnIfFalse(_surface_lut.initialize(
//             SURFACE_ATLAS_RESOLUTION,
//             SURFACE_ATLAS_RESOLUTION,
//             SURFACE_RESOLUTION
//         ));

//         _surface_atlas_table.initialize(SURFACE_ATLAS_RESOLUTION, SURFACE_RESOLUTION);

//         // Texture
//         {
//             ReturnIfFalse(_surface_base_color_atlas_texture = std::shared_ptr<TextureInterface>(device->create_texture(
//                 TextureDesc::create_shader_resource(
//                     SURFACE_ATLAS_RESOLUTION, 
//                     SURFACE_ATLAS_RESOLUTION, 
//                     1, 
//                     Format::RGBA16_FLOAT,
//                     false,
//                     "surface_base_color_atlas_texture"
//                 )
//             )));
//             ReturnIfFalse(_surface_normal_atlas_texture = std::shared_ptr<TextureInterface>(device->create_texture(
//                 TextureDesc::create_shader_resource(
//                     SURFACE_ATLAS_RESOLUTION, 
//                     SURFACE_ATLAS_RESOLUTION, 
//                     1, 
//                     Format::RGB32_FLOAT,
//                     false,
//                     "surface_normal_atlas_texture"
//                 )
//             )));
//             ReturnIfFalse(_surface_pbr_atlas_texture = std::shared_ptr<TextureInterface>(device->create_texture(
//                 TextureDesc::create_shader_resource(
//                     SURFACE_ATLAS_RESOLUTION, 
//                     SURFACE_ATLAS_RESOLUTION, 
//                     1, 
//                     Format::RGBA8_UNORM,
//                     false,
//                     "surface_pbr_atlas_texture"
//                 )
//             )));
//             ReturnIfFalse(_surface_emissive_atlas_texture = std::shared_ptr<TextureInterface>(device->create_texture(
//                 TextureDesc::create_shader_resource(
//                     SURFACE_ATLAS_RESOLUTION, 
//                     SURFACE_ATLAS_RESOLUTION, 
//                     1, 
//                     Format::RGBA16_FLOAT,
//                     false,
//                     "surface_emissive_atlas_texture"
//                 )
//             )));
//             ReturnIfFalse(_surface_light_cache_atlas_texture = std::shared_ptr<TextureInterface>(device->create_texture(
//                 TextureDesc::create_shader_resource(
//                     SURFACE_ATLAS_RESOLUTION, 
//                     SURFACE_ATLAS_RESOLUTION, 
//                     1, 
//                     Format::RGBA16_FLOAT,
//                     false,
//                     "surface_light_cache_atlas_texture"
//                 )
//             )));
//             cache->collect(_surface_base_color_atlas_texture, ResourceType::Texture);
//             cache->collect(_surface_normal_atlas_texture, ResourceType::Texture);
//             cache->collect(_surface_pbr_atlas_texture, ResourceType::Texture);
//             cache->collect(_surface_emissive_atlas_texture, ResourceType::Texture);
//             cache->collect(_surface_light_cache_atlas_texture, ResourceType::Texture);
//         }

// 		return true;
// 	}

// 	bool LightCacheUpdatePass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
// 	{
// 		ReturnIfFalse(cmdlist->open());

// 		HANDLE fence_event = CreateEvent(nullptr, false, false, nullptr);

//         std::shared_ptr<BufferInterface> vt_page_buffer = 
// 			check_cast<BufferInterface>(cache->require("vt_page_buffer"));

// 		VTPageInfo* data = static_cast<VTPageInfo*>(vt_page_buffer->map(CpuAccessMode::Read, fence_event));

//         World* world = cache->get_world();
		
//         parallel::parallel_for(
// 			[&](uint64_t x, uint64_t y)
// 			{
// 				uint32_t offset = static_cast<uint32_t>(x + y * CLIENT_WIDTH);
// 				const auto& info = *(data + offset);

// 				uint2 page_id;
// 				uint32_t mip_level;

// 				uint32_t mesh_id;
// 				uint32_t submesh_id;
// 				info.get_data(mesh_id, submesh_id, page_id, mip_level);

//                 std::mutex info_mutex;

//                 bool res = world->each<Mesh, SurfaceCache>(
// 					[&](Entity* mesh_entity, Mesh* mesh, SurfaceCache* surface_cache) -> bool
// 					{
// 						if (mesh->mesh_id == mesh_id)
// 						{
//                             VTPage* page = _surface_lut.query_page(page_id, 0);
										
//                             uint2 page_physical_pos;

//                             {
//                                 std::lock_guard lock(info_mutex);

//                                 VTPage::LoadFlag flag = page->flag;
//                                 page_physical_pos = _surface_atlas_table.add_page(page) * SURFACE_RESOLUTION;

//                                 if (flag == VTPage::LoadFlag::Unload)
//                                 {
                                   
//                                 }
//                             }
//                         }
//                         return true;
//                     }
//                 );

//                 assert(res);
//             },
//             CLIENT_WIDTH,
//             CLIENT_HEIGHT
//         );

//         vt_page_buffer->unmap();

// 		ReturnIfFalse(cmdlist->close());
//         return true;
// 	}
// }