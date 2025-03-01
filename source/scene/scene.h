#ifndef SCENE_H
#define SCENE_H
#include "geometry.h"
#include <unordered_set>
#include "distance_field.h"
#include "surface_cache.h"
#include "virtual_mesh.h"

class aiScene;

namespace fantasy
{
	class SceneSystem :
		public EntitySystemInterface,
		public EventSubscriber<event::OnModelLoad>,
		public EventSubscriber<event::OnModelTransform>,
		public EventSubscriber<event::OnComponentAssigned<Mesh>>,
		public EventSubscriber<event::OnComponentAssigned<Material>>,
		public EventSubscriber<event::OnComponentAssigned<VirtualMesh>>,
		public EventSubscriber<event::OnComponentAssigned<SurfaceCache>>,
		public EventSubscriber<event::OnComponentAssigned<DistanceField>>
	{
	public:
		bool initialize(World* world) override;
		bool destroy() override;

		bool tick(float delta) override;

		bool publish(World* world, const event::OnModelLoad& event) override;
		bool publish(World* world, const event::OnModelTransform& event) override;
		bool publish(World* world, const event::OnComponentAssigned<Mesh>& event) override;
		bool publish(World* world, const event::OnComponentAssigned<Material>& event) override;
		bool publish(World* world, const event::OnComponentAssigned<VirtualMesh>& event) override;
		bool publish(World* world, const event::OnComponentAssigned<SurfaceCache>& event) override;
		bool publish(World* world, const event::OnComponentAssigned<DistanceField>& event) override;

		void confirm_init_models(const std::vector<std::string>& model_paths);

	private:
		bool load_init_scene();

		bool _init_scene_loaded = false;
		std::vector<std::string> _init_models;

	public:
		inline static uint32_t loaded_submesh_count = 0;
	
	private:
		World* _world = nullptr;
		Entity* _global_entity = nullptr;
		std::string _model_directory;
		std::string _sdf_data_path;
		std::string _surface_cache_path;
		uint32_t _current_submesh_count = 0;

		Sphere _scene_sphere;
		std::unordered_set<std::string> _loaded_model_names;

		const aiScene* assimp_scene = nullptr;
	};
}


















#endif