#include "scene.h"

#include <assimp/scene.h>
#include "light.h"
#include "camera.h"
#include "../core/parallel/parallel.h"
#include "../core/tools/file.h"
#include "../gui/gui_panel.h"
#include "virtual_texture.h"
#include "../core/tools/file.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

namespace fantasy
{

	bool SceneSystem::initialize(World* world)
	{
		_world = world;
		_world->subscribe<event::OnModelLoad>(this);
		_world->subscribe<event::OnModelTransform>(this);
		_world->subscribe<event::OnComponentAssigned<Mesh>>(this);
		_world->subscribe<event::OnComponentAssigned<Material>>(this);
		_world->subscribe<event::OnComponentAssigned<VirtualMesh>>(this);
		// _world->subscribe<event::OnComponentAssigned<SurfaceCache>>(this);
		// _world->subscribe<event::OnComponentAssigned<DistanceField>>(this);

		_global_entity = _world->get_global_entity();

		// _global_entity->assign<SDFGrid>();
		_global_entity->assign<event::AddModel>();
		// _global_entity->assign<event::GenerateSdf>();
		// _global_entity->assign<event::AddSpotLight>();
		// _global_entity->assign<event::AddPointLight>();
		_global_entity->assign<event::GenerateMipmap>();
		// _global_entity->assign<event::UpdateGlobalSdf>();
		// _global_entity->assign<event::GenerateSurfaceCache>();
		_global_entity->assign<DirectionalLight>()->update_direction_view_proj();

		return true;
	}

	bool SceneSystem::destroy()
	{
		_world->unsubscribe_all(this);
		return true;
	}

	bool SceneSystem::tick(float delta)
	{
		_world->get_global_entity()->get_component<Camera>()->handle_input(delta);

		static uint64_t thread_id = INVALID_SIZE_64;
		static Entity* model_entity = nullptr;

		if (gui::has_file_selected())
		{
			if (model_entity == nullptr)
			{
				std::string file_path = gui::get_selected_file_path();
				std::string model_name = file_path.substr(file_path.find("asset"));
				replace_back_slashes(model_name);

				if (!_loaded_model_names.contains(model_name))
				{
					model_entity = _world->create_entity_delay();
					thread_id = parallel::begin_thread(
						[this, model_name]() -> bool
						{
							return _world->broadcast(event::OnModelLoad{
								.entity = model_entity,
								.model_path = model_name
							});
						}
					);
				}
				else
				{
					gui::notify_message(gui::ENotifyType::Info, model_name + " has already been loaded.");
				}
			}
			else
			{
				gui::notify_message(gui::ENotifyType::Info, "There is already a model loading.");
			}
		}

		if (model_entity && parallel::thread_finished(thread_id) && parallel::thread_success(thread_id))
		{
			_world->add_delay_entity(model_entity);
			ReturnIfFalse(_global_entity->get_component<event::AddModel>()->broadcast());

			loaded_submesh_count = _current_submesh_count;
			thread_id = INVALID_SIZE_64;
			model_entity = nullptr;
		}
		return true;
	}

	bool SceneSystem::publish(World* world, const event::OnModelLoad& event)
	{
		std::string proj_dir = PROJ_DIR;

		Assimp::Importer assimp_importer;

        assimp_scene = assimp_importer.ReadFile(
			proj_dir + event.model_path, 
			aiProcess_Triangulate | 
			aiProcess_GenSmoothNormals | 
			aiProcess_FlipUVs |
			aiProcess_CalcTangentSpace
		);

        if(!assimp_scene || assimp_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !assimp_scene->mRootNode)
        {
			LOG_ERROR("Failed to load model.");
			LOG_ERROR(assimp_importer.GetErrorString());
			return false;
        }

		std::string model_name = remove_file_extension(event.model_path.c_str());

		_sdf_data_path = proj_dir + "asset/cache/distance_field/" + model_name + ".sdf";
		_surface_cache_path = proj_dir + "asset/cache/surface_cache/" + model_name + ".sc";
		_model_directory = event.model_path.substr(0, event.model_path.find_last_of('/') + 1);

		event.entity->assign<std::string>(model_name);
		event.entity->assign<Mesh>();
		event.entity->assign<Material>();
		event.entity->assign<Transform>();
		event.entity->assign<VirtualMesh>();
		// event.entity->assign<SurfaceCache>();
		// event.entity->assign<DistanceField>();
		
		_loaded_model_names.insert(event.model_path);
		
		uint32_t* available_task_num = event.entity->assign<uint32_t>(1);
		// ReturnIfFalse(_global_entity->get_component<event::GenerateSdf>()->broadcast(event.entity));
		ReturnIfFalse(_global_entity->get_component<event::GenerateMipmap>()->broadcast(event.entity));
		// ReturnIfFalse(_global_entity->get_component<event::GenerateSurfaceCache>()->broadcast(event.entity));

		while (*available_task_num > 0) std::this_thread::yield();
		gui::notify_message(gui::ENotifyType::Info, "Loaded " + event.model_path);

		// Entity* tmp_model_entity = event.entity;
		// gui::add(
		// 	[tmp_model_entity, this]()
		// 	{
		// 		std::string model_name = *tmp_model_entity->get_component<std::string>();
		// 		model_name += " Transform";
		// 		if (ImGui::TreeNode(model_name.c_str()))
		// 		{
		// 			bool changed = false;

		// 			Transform tmp_trans = *tmp_model_entity->get_component<Transform>();
		// 			changed |= ImGui::SliderFloat3("position", reinterpret_cast<float*>(&tmp_trans.position), -32.0f, 32.0f);		
		// 			changed |= ImGui::SliderFloat3("rotation", reinterpret_cast<float*>(&tmp_trans.rotation), -180.0f, 180.0f);
		// 			changed |= ImGui::SliderFloat3("scale", reinterpret_cast<float*>(&tmp_trans.scale), 0.1f, 8.0f);
		// 			if (changed)
		// 			{
		// 				_world->broadcast(event::OnModelTransform{ .entity = tmp_model_entity, .transform = tmp_trans });
		// 			}

		// 			ImGui::TreePop();
		// 		}
		// 	}
		// );

		return true;
	}

	bool SceneSystem::publish(World* world, const event::OnModelTransform& event)
	{
		Transform* transform = event.entity->get_component<Transform>();
		DistanceField* distance_field = event.entity->get_component<DistanceField>();

		uint32_t chunk_num_per_axis = GLOBAL_SDF_RESOLUTION / VOXEL_NUM_PER_CHUNK;
		float voxel_size = SDF_SCENE_GRID_SIZE / GLOBAL_SDF_RESOLUTION;
		float chunk_size = 1.0f * VOXEL_NUM_PER_CHUNK * voxel_size;

		SDFGrid* grid = _global_entity->get_component<SDFGrid>();

		auto func_mark = [&](const Bounds3F& box, bool insert_or_erase)
		{
			uint3 uniform_lower = uint3((box._lower + SDF_SCENE_GRID_SIZE / 2.0f) / chunk_size);
			uint3 uniform_upper = uint3((box._upper + SDF_SCENE_GRID_SIZE / 2.0f) / chunk_size);

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