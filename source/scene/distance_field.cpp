#include "distance_field.h"
#include "../core/tools/file.h"
#include "../gui/gui_panel.h"
#include "scene.h"

namespace fantasy 
{
    SDFGrid::SDFGrid()
	{
		uint32_t chunk_num_per_axis = GLOBAL_SDF_RESOLUTION / VOXEL_NUM_PER_CHUNK;
		float voxel_size = SCENE_GRID_SIZE / GLOBAL_SDF_RESOLUTION;
		float chunk_size = VOXEL_NUM_PER_CHUNK * voxel_size;

		chunks.resize(chunk_num_per_axis * chunk_num_per_axis * chunk_num_per_axis);
		std::vector<Bounds3F> boxes(chunk_num_per_axis * chunk_num_per_axis * chunk_num_per_axis);
		for (uint32_t z = 0; z < chunk_num_per_axis; ++z)
			for (uint32_t y = 0; y < chunk_num_per_axis; ++y)
				for (uint32_t x = 0; x < chunk_num_per_axis; ++x)
				{
					float3 Lower = {
						-SCENE_GRID_SIZE * 0.5f + x * chunk_size,
						-SCENE_GRID_SIZE * 0.5f + y * chunk_size,
						-SCENE_GRID_SIZE * 0.5f + z * chunk_size
					};
					boxes[x + y * chunk_num_per_axis + z * chunk_num_per_axis * chunk_num_per_axis] = Bounds3F(Lower, Lower + chunk_size);
				}
		Bounds3F global_box(float3(-SCENE_GRID_SIZE * 0.5f), float3(SCENE_GRID_SIZE * 0.5f));
		bvh.build(boxes, global_box);
	}

	DistanceField::TransformData DistanceField::MeshDistanceField::get_transformed(const Transform* transform) const
	{
		TransformData ret;
		ret.sdf_box = sdf_box;

		float4x4 S = scale(transform->scale);
		ret.sdf_box._lower = float3(mul(float4(ret.sdf_box._lower, 1.0f), S));
		ret.sdf_box._upper = float3(mul(float4(ret.sdf_box._upper, 1.0f), S));

		float4x4 R = rotate(transform->rotation);
		std::array<float3, 8> box_vertices;
		box_vertices[0] = ret.sdf_box._lower;
		box_vertices[1] = float3(ret.sdf_box._lower.x, ret.sdf_box._upper.y, ret.sdf_box._lower.z);
		box_vertices[2] = float3(ret.sdf_box._upper.x, ret.sdf_box._upper.y, ret.sdf_box._lower.z);
		box_vertices[3] = float3(ret.sdf_box._upper.x, ret.sdf_box._lower.y, ret.sdf_box._lower.z);
		box_vertices[4] = ret.sdf_box._upper;
		box_vertices[7] = float3(ret.sdf_box._upper.x, ret.sdf_box._lower.y, ret.sdf_box._upper.z);
		box_vertices[5] = float3(ret.sdf_box._lower.x, ret.sdf_box._lower.y, ret.sdf_box._upper.z);
		box_vertices[6] = float3(ret.sdf_box._lower.x, ret.sdf_box._upper.y, ret.sdf_box._upper.z);

		Bounds3F blank_box(0.0f, 0.0f);
		for (const auto& crVertex : box_vertices)
		{
			blank_box = merge(blank_box, float3(mul(float4(crVertex, 1.0f), R)));
		}

		ret.sdf_box = blank_box;

		float4x4 T = translate(transform->position);
		ret.sdf_box._lower = float3(mul(float4(ret.sdf_box._lower, 1.0f), T));
		ret.sdf_box._upper = float3(mul(float4(ret.sdf_box._upper, 1.0f), T));

		float3 sdf_extent = sdf_box._upper - sdf_box._lower;
		ret.coord_matrix = mul(
			inverse(mul(mul(S, R), T)),		// Local Matrix.
			float4x4(
				1.0f / sdf_extent.x, 0.0f, 0.0f, 0.0f,
				0.0f, -1.0f / sdf_extent.y, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f / sdf_extent.z, 0.0f,
				-sdf_box._lower.x / sdf_extent.x,
				sdf_box._upper.y / sdf_extent.y,
				-sdf_box._lower.z / sdf_extent.z,
				1.0f
			)
		);
		return ret;
	}

	bool SceneSystem::publish(World* world, const event::OnComponentAssigned<DistanceField>& event)
	{
		DistanceField* distance_field = event.component;
		Mesh* mesh = event.entity->get_component<Mesh>();
		distance_field->mesh_distance_fields.resize(mesh->submeshes.size());

		bool load_from_file = false;
		if (is_file_exist(_sdf_data_path.c_str()))
		{
			serialization::BinaryInput input(_sdf_data_path);
			uint32_t mesh_sdf_resolution = 0;
			input(mesh_sdf_resolution);
			
			if (mesh_sdf_resolution == SDF_RESOLUTION)
			{
				for (uint32_t ix = 0; ix < distance_field->mesh_distance_fields.size(); ++ix)
				{
					auto& mesh_df = distance_field->mesh_distance_fields[ix];
					mesh_df.sdf_texture_name = *event.entity->get_component<std::string>() + "SdfTexture" + std::to_string(ix);

					input(
						mesh_df.sdf_box._lower.x,
						mesh_df.sdf_box._lower.y,
						mesh_df.sdf_box._lower.z,
						mesh_df.sdf_box._upper.x,
						mesh_df.sdf_box._upper.y,
						mesh_df.sdf_box._upper.z
					);

					uint64_t data_size = static_cast<uint64_t>(SDF_RESOLUTION) * SDF_RESOLUTION * SDF_RESOLUTION * sizeof(float);
					mesh_df.sdf_data.resize(data_size);
					input.load_binary_data(mesh_df.sdf_data.data(), data_size);
				}
				gui::notify_message(gui::ENotifyType::Info, "Loaded " + _sdf_data_path.substr(_sdf_data_path.find("asset")));
				load_from_file = true;
			}
		}

		if (!load_from_file)
		{
			std::string model_name = *event.entity->get_component<std::string>();
			for (uint32_t ix = 0; ix < distance_field->mesh_distance_fields.size(); ++ix)
			{
				auto& mesh_df = distance_field->mesh_distance_fields[ix];
				const auto& submesh = mesh->submeshes[ix];
				mesh_df.sdf_texture_name = model_name + "SdfTexture" + std::to_string(ix);

				uint64_t jx = 0;
				std::vector<Bvh::Vertex> BvhVertices(submesh.indices.size());
				for (auto VertexIndex : submesh.indices)
				{
					BvhVertices[jx++] = {
						float3(mul(float4(submesh.vertices[VertexIndex].position, 1.0f), submesh.world_matrix)),
						float3(mul(float4(submesh.vertices[VertexIndex].normal, 1.0f), transpose(inverse(submesh.world_matrix))))
					};
				}

				mesh_df.bvh.build(BvhVertices, static_cast<uint32_t>(submesh.indices.size() / 3));
				mesh_df.sdf_box = mesh_df.bvh.global_box;
			}
		}

		SDFGrid* grid = _global_entity->get_component<SDFGrid>();

		for (const auto& mesh_df : distance_field->mesh_distance_fields)
		{
			DistanceField::TransformData data = mesh_df.get_transformed(event.entity->get_component<Transform>());
			uint32_t chunk_num_per_axis = GLOBAL_SDF_RESOLUTION / VOXEL_NUM_PER_CHUNK;
			float voxel_size = SCENE_GRID_SIZE / GLOBAL_SDF_RESOLUTION;
			float chunk_size = VOXEL_NUM_PER_CHUNK * voxel_size;
			float grid_size = chunk_size * chunk_num_per_axis;

			uint3 uniform_lower = uint3((data.sdf_box._lower + grid_size / 2.0f) / chunk_size);
			uint3 uniform_upper = uint3((data.sdf_box._upper + grid_size / 2.0f) / chunk_size);

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
						grid->chunks[index].model_entities.insert(event.entity);
					}
				}
			}
		}
		
		return true;
	}


}