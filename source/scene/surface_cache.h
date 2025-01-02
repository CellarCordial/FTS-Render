#ifndef SCENE_SURFACE_CACHE_H
#define SCENE_SURFACE_CACHE_H

#include "../dynamic_rhi/format.h"
#include "../core/tools/delegate.h"
#include "../core/tools/ecs.h"
#include <string>
#include <vector>
#include <array>


namespace fantasy 
{
	namespace event
	{
		DELCARE_DELEGATE_EVENT(GenerateSurfaceCache, Entity*);
	};

	struct SurfaceCache
	{
		struct Surface
		{
			std::vector<uint8_t> data;
			std::string surface_texture_name;
		};

		struct MeshSurfaceCache
		{
			enum SurfaceType : uint8_t
			{
				Color,
				normal,
				Depth,
				PBR,
				Emissve,
				Count
			};
			
			std::array<Surface, SurfaceType::Count> surfaces;
			std::string LightCache;
		};

		Format format = Format::RGBA8_UNORM;
		std::vector<MeshSurfaceCache> mesh_surface_caches;

		bool check_surface_cache_exist() const { return !mesh_surface_caches.empty() && !mesh_surface_caches[0].surfaces[0].data.empty(); }
	};

}


















#endif