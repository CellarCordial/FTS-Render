#include "scene.h"

#include "light.h"
#include "camera.h"
#include "../core/parallel/parallel.h"
#include "../core/tools/file.h"
#include "../gui/gui_panel.h"
#include "virtual_texture.h"


namespace fantasy
{

	bool SceneSystem::initialize(World* world)
	{
		_world = world;
		_world->subscribe<event::OnModelLoad>(this);
		_world->subscribe<event::OnModelTransform>(this);
		_world->subscribe<event::OnComponentAssigned<Mesh>>(this);
		_world->subscribe<event::OnComponentAssigned<Material>>(this);
		// _world->subscribe<event::OnComponentAssigned<SurfaceCache>>(this);
		// _world->subscribe<event::OnComponentAssigned<DistanceField>>(this);
		// _world->subscribe<event::OnComponentAssigned<VirtualMesh>>(this);

		_global_entity = _world->get_global_entity();

		// _global_entity->assign<SDFGrid>();
		// _global_entity->assign<event::GenerateSdf>();
		// _global_entity->assign<event::ModelLoaded>();
		// _global_entity->assign<event::AddSpotLight>();
		// _global_entity->assign<event::AddPointLight>();
		// _global_entity->assign<event::UpdateGlobalSdf>();
		// _global_entity->assign<event::GenerateSurfaceCache>();

		
		// uint32_t current_resolution = lowest_texture_resolution;
		// while (current_resolution < highest_texture_resolution)
		// {
		// 	_world->create_entity()->assign<MipmapLUT>()->initialize(current_resolution);
		// 	current_resolution <<= 1;
		// }

		return true;
	}

	bool SceneSystem::destroy()
	{
		_world->unsubscribe_all(this);
		return true;
	}

	bool SceneSystem::tick(float delta)
	{
		ReturnIfFalse(_world->each<Camera>(
			[delta](Entity* entity, Camera* camera) -> bool
			{
				camera->handle_input(delta);
				return true;
			}
		));

		// Load Model.
		{
			static uint64_t thread_id = INVALID_SIZE_64;
			static Entity* model_entity = nullptr;

			if (gui::has_file_selected() && model_entity == nullptr)
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

			if (model_entity && parallel::thread_finished(thread_id) && parallel::thread_success(thread_id))
			{
				_world->add_delay_entity(model_entity);
				// ReturnIfFalse(_global_entity->get_component<event::ModelLoaded>()->broadcast());

				thread_id = INVALID_SIZE_64;
				model_entity = nullptr;
			}
		}

		return true;
	}

}