#include "surface_cache.h"
#include "../core/tools/file.h"
#include "../gui/gui_panel.h"
#include "scene.h"

namespace fantasy 
{

	bool SceneSystem::publish(World* world, const event::OnComponentAssigned<SurfaceCache>& event)
	{
		SurfaceCache* surface_cache = event.component;
		Mesh* mesh = event.entity->get_component<Mesh>();
		surface_cache->mesh_surface_caches.resize(mesh->submeshes.size());

		bool load_from_file = false;
		if (is_file_exist(_surface_cache_path.c_str()))
		{
			serialization::BinaryInput input(_surface_cache_path);
			uint32_t card_resolution = 0;
			uint32_t surface_resolution = 0;
			input(card_resolution);
			input(surface_resolution);

			if (card_resolution == CARD_RESOLUTION && surface_resolution == SURFACE_RESOLUTION)
			{
				FormatInfo FormaInfo = get_format_info(surface_cache->format);
				uint64_t data_size = static_cast<uint64_t>(SURFACE_RESOLUTION) * SURFACE_RESOLUTION * FormaInfo.byte_size_per_pixel;

				for (uint32_t ix = 0; ix < surface_cache->mesh_surface_caches.size(); ++ix)
				{
					auto& mesh_surface_cache = surface_cache->mesh_surface_caches[ix];
					for (uint32_t ix = 0; ix < SurfaceCache::MeshSurfaceCache::SurfaceType_Num; ++ix)
					{
						mesh_surface_cache.surfaces[ix].surface_texture_name = 
							*event.entity->get_component<std::string>() + "surface_texture" + std::to_string(ix);
						mesh_surface_cache.surfaces[ix].data.resize(data_size);
						input.load_binary_data(mesh_surface_cache.surfaces[ix].data.data(), data_size);
					}
				}
				gui::notify_message(gui::ENotifyType::Info, "Loaded " + _sdf_data_path.substr(_sdf_data_path.find("asset")));
				load_from_file = true;
			}
		}

		if (!load_from_file)
		{
			std::string model_name = *event.entity->get_component<std::string>();
			for (uint32_t ix = 0; ix < surface_cache->mesh_surface_caches.size(); ++ix)
			{
				std::string submesh_index = std::to_string(ix);
				auto& mesh_surface_cache = surface_cache->mesh_surface_caches[ix];
				mesh_surface_cache.surfaces[0].surface_texture_name = model_name + "_surface_base_color_texture" + submesh_index;
				mesh_surface_cache.surfaces[1].surface_texture_name = model_name + "_surface_normal_texture" + submesh_index;
				mesh_surface_cache.surfaces[2].surface_texture_name = model_name + "_surface_pbr_texture" + submesh_index;
				mesh_surface_cache.surfaces[3].surface_texture_name = model_name + "_surface_emissve_texture" + submesh_index;
				mesh_surface_cache.light_cache = model_name + "_surface_light_texture" + submesh_index;
			}
		}

		return true;
	}
}