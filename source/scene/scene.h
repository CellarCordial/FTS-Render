#ifndef SCENE_H
#define SCENE_H
#include "geometry.h"
#include <unordered_set>
#include "distance_filed.h"
#include "surface_cache.h"
#include "virtual_mesh.h"

namespace fantasy
{

	class SceneSystem :
		public EntitySystemInterface,
		public EventSubscriber<event::OnModelLoad>,
		public EventSubscriber<event::OnModelTransform>,
		public EventSubscriber<event::OnComponentAssigned<Mesh>>,
		public EventSubscriber<event::OnComponentAssigned<Material>>,
		public EventSubscriber<event::OnComponentAssigned<SurfaceCache>>,
		public EventSubscriber<event::OnComponentAssigned<DistanceField>>,
		public EventSubscriber<event::OnComponentAssigned<VirtualMesh>>
	{
	public:
		bool initialize(World* world) override;
		bool destroy() override;

		bool tick(float delta) override;

		bool publish(World* world, const event::OnModelLoad& event) override;
		bool publish(World* world, const event::OnModelTransform& event) override;
		bool publish(World* world, const event::OnComponentAssigned<Mesh>& event) override;
		bool publish(World* world, const event::OnComponentAssigned<Material>& event) override;
		bool publish(World* world, const event::OnComponentAssigned<SurfaceCache>& event) override;
		bool publish(World* world, const event::OnComponentAssigned<DistanceField>& event) override;
		bool publish(World* world, const event::OnComponentAssigned<VirtualMesh>& event) override;

	private:
		World* _world = nullptr;
		Entity* _global_entity = nullptr;
		std::string _model_directory;
		std::string _sdf_data_path;
		std::string _surface_cache_path;

		std::unordered_set<std::string> _loaded_model_names;
	};
}


















#endif