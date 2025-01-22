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

	inline const uint32_t CARD_RESOLUTION = 32u;
	inline const uint32_t SURFACE_RESOLUTION = 128u;

	struct SurfaceCache
	{
		struct Surface
		{
			std::vector<uint8_t> data;
			std::string surface_texture_name;
		};

		struct MeshSurfaceCache
		{
			enum
			{
				SurfaceType_BaseColor,
				SurfaceType_Normal,
				SurfaceType_Depth,
				SurfaceType_PBR,
				SurfaceType_Emissve,
				SurfaceType_Num
			};
			
			std::array<Surface, SurfaceType_Num> surfaces;
			std::string light_cache;
		};

		Format format = Format::RGBA8_UNORM;
		std::vector<MeshSurfaceCache> mesh_surface_caches;

		bool check_surface_cache_exist() const { return !mesh_surface_caches.empty() && !mesh_surface_caches[0].surfaces[0].data.empty(); }
	};

}


















#endif