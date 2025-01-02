#ifndef SCENE_H
#define SCENE_H
#include "geometry.h"
#include <unordered_set>
#include "../core/tools/delegate.h"
#include "../core/math/bounds.h"
#include "../core/math/bvh.h"
#include "../core/tools/ecs.h"

namespace fantasy
{
	struct DistanceField;
	struct SurfaceCache;

	struct Transform
	{
		float3 position = { 0.0f, 0.0f, 0.0f };
		float3 rotation = { 0.0f, 0.0f, 0.0f };
		float3 scale = { 1.0f, 1.0f, 1.0f };

		float4x4 world_matrix() const
		{
			return float4x4(mul(translate(position), mul(rotate(rotation), ::fantasy::scale(scale))));
		}
	};

	namespace event
	{
		struct OnModelLoad
		{
			Entity* entity = nullptr;
			std::string model_path;
		};

		struct OnModelTransform
		{
			Entity* entity = nullptr;
			Transform transform;
		};

		DELCARE_DELEGATE_EVENT(UpdateGlobalSdf);
		DELCARE_MULTI_DELEGATE_EVENT(ModelLoaded);
		DELCARE_DELEGATE_EVENT(GenerateSdf, Entity*);
		DELCARE_DELEGATE_EVENT(GenerateSurfaceCache, Entity*);
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

	class SceneSystem :
		public EntitySystemInterface,
		public EventSubscriber<event::OnModelLoad>,
		public EventSubscriber<event::OnModelTransform>,
		public EventSubscriber<event::OnComponentAssigned<Mesh>>,
		public EventSubscriber<event::OnComponentAssigned<Material>>,
		public EventSubscriber<event::OnComponentAssigned<SurfaceCache>>,
		public EventSubscriber<event::OnComponentAssigned<DistanceField>>,
		public EventSubscriber<event::OnComponentAssigned<VirtualGeometry>>
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
		bool publish(World* world, const event::OnComponentAssigned<VirtualGeometry>& event) override;

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