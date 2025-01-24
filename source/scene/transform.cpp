#include "transform.h"
#include "scene.h"

namespace fantasy 
{
	bool SceneSystem::publish(World* world, const event::OnModelTransform& event)
	{
		Transform* transform = event.entity->get_component<Transform>();
		DistanceField* distance_field = event.entity->get_component<DistanceField>();

		uint32_t chunk_num_per_axis = GLOBAL_SDF_RESOLUTION / VOXEL_NUM_PER_CHUNK;
		float voxel_size = SCENE_GRID_SIZE / GLOBAL_SDF_RESOLUTION;
		float chunk_size = 1.0f * VOXEL_NUM_PER_CHUNK * voxel_size;

		SDFGrid* grid = _global_entity->get_component<SDFGrid>();

		auto func_mark = [&](const Bounds3F& box, bool insert_or_erase)
		{
			uint3 uniform_lower = uint3((box._lower + SCENE_GRID_SIZE / 2.0f) / chunk_size);
			uint3 uniform_upper = uint3((box._upper + SCENE_GRID_SIZE / 2.0f) / chunk_size);

			for (uint32_t ix = 0; ix < 3; ++ix)
			{
				if (uniform_lower[ix] != 0) uniform_lower[ix] -= 1;
				if (uniform_upper[ix] != chunk_num_per_axis - 1) uniform_upper[ix] += 1;
			}

			for (uint32_t z = uniform_lower.z; z <= uniform_upper.z; ++z)
			{
				for (uint32_t y = uniform_lower.y; y <= uniform_upper.y; ++y)
				{
					for (uint32_t x = uniform_lower.x; x <= uniform_upper.x; ++x)
					{
						uint32_t index = x + y * chunk_num_per_axis + z * chunk_num_per_axis * chunk_num_per_axis;
						grid->chunks[index].model_moved = true;
						if (insert_or_erase) 
							grid->chunks[index].model_entities.insert(event.entity);
						else				
							grid->chunks[index].model_entities.erase(event.entity);
					}
				}
			}

		};

		for (const auto& mesh_df : distance_field->mesh_distance_fields)
		{
			Bounds3F old_sdf_box = mesh_df.get_transformed(transform).sdf_box;
			Bounds3F new_sdf_box = mesh_df.get_transformed(&event.transform).sdf_box;

			func_mark(old_sdf_box, false);
			func_mark(new_sdf_box, true);
		}

		*transform = event.transform;

		Mesh* mesh = event.entity->get_component<Mesh>();
		float4x4 S = scale(transform->scale);
		float4x4 R = rotate(transform->rotation);
		float4x4 T = translate(transform->position);
		mesh->world_matrix = mul(mul(T, R), S);
		mesh->moved = true;

		return _world->each<event::UpdateGlobalSdf>(
			[](Entity* entity, event::UpdateGlobalSdf* event) -> bool
			{
				return event->broadcast();
			}
		);
	}
}