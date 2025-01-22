#ifndef SCENE_DISTANCE_FIELD_H
#define SCENE_DISTANCE_FIELD_H

#include "../core/tools/delegate.h"
#include "../core/math/bounds.h"
#include "../core/math/matrix.h"
#include "../core/math/bvh.h"
#include "../core/tools/ecs.h"
#include "transform.h"
#include <string>
#include <unordered_set>


namespace fantasy 
{
    namespace event
	{
		DELCARE_DELEGATE_EVENT(UpdateGlobalSdf);
		DELCARE_DELEGATE_EVENT(GenerateSdf, Entity*);
	};

	struct DistanceField
	{
		struct TransformData
		{
			float4x4 coord_matrix;
			Bounds3F sdf_box;
		};

		struct MeshDistanceField
		{
			std::string sdf_texture_name;
			Bounds3F sdf_box;

			std::vector<uint8_t> sdf_data;
			Bvh bvh;

			TransformData get_transformed(const Transform* transform) const;
		};

		std::vector<MeshDistanceField> mesh_distance_fields;

		bool check_sdf_cache_exist() const { return !mesh_distance_fields.empty() && !mesh_distance_fields[0].sdf_data.empty(); }
	};

	inline const float SCENE_GRID_SIZE = 64.0f;
	inline const uint32_t GLOBAL_SDF_RESOLUTION = 256u;
	inline const uint32_t VOXEL_NUM_PER_CHUNK = 32u;
	inline const uint32_t SDF_RESOLUTION = 64u;

	struct SceneGrid
	{
		struct Chunk
		{
			std::unordered_set<Entity*> model_entities;
			bool model_moved = false;
		};
		
		std::vector<Chunk> chunks;
		std::vector<Bounds3F> boxes;
		Bvh bvh;

		SceneGrid();
	};
}


















#endif