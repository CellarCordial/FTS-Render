#ifndef SCENE_H
#define SCENE_H
#include "Geometry.h"
#include "../../Core/include/Delegate.h"
#include <unordered_set>
#include "../../Math/include/Bounds.h"
#include "../../Math/include/Bvh.h"
#include "../../Core/include/Entity.h"

namespace FTS
{
	struct FDistanceField;
	struct FSurfaceCache;

	struct FTransform
	{
		FVector3F Position = { 0.0f, 0.0f, 0.0f };
		FVector3F Rotation = { 0.0f, 0.0f, 0.0f };
		FVector3F Scale = { 1.0f, 1.0f, 1.0f };

		FMatrix4x4 WorldMatrix() const
		{
			return FMatrix4x4(Mul(Translate(Position), Mul(Rotate(Rotation), ::FTS::Scale(Scale))));
		}
	};

	namespace Event
	{
		struct OnModelLoad
		{
			FEntity* pEntity = nullptr;
			std::string strModelPath;
		};

		struct OnModelTransform
		{
			FEntity* pEntity = nullptr;
			FTransform Trans;
		};

		DeclareDelegateEvent(UpdateGlobalSdf);
		DeclareDelegateEvent(GenerateSdf, FEntity*);
		DeclareDelegateEvent(GenerateSurfaceCache, FEntity*);
	};

	struct FDistanceField
	{
		struct TransformData
		{
			FMatrix4x4 CoordMatrix;
			FBounds3F SdfBox;
		};

		struct MeshDistanceField
		{
			std::string strSdfTextureName;
			FBounds3F SdfBox;

			std::vector<UINT8> SdfData;
			FBvh Bvh;

			TransformData GetTransformed(const FTransform* cpTransform) const;
		};

		std::vector<MeshDistanceField> MeshDistanceFields;

		BOOL CheckSdfFileExist() const { return !MeshDistanceFields.empty() && !MeshDistanceFields[0].SdfData.empty(); }
	};

	inline const FLOAT gfSceneGridSize = 64.0f;
	inline const UINT32 gdwGlobalSdfResolution = 256u;
	inline const UINT32 gdwVoxelNumPerChunk = 32u;
	inline const UINT32 gdwSdfResolution = 64u;

	inline const UINT32 gdwCardResolution = 32u;
	inline const UINT32 gdwSurfaceResolution = 128u;

	struct FSurfaceCache
	{
		struct Surface
		{
			std::vector<UINT8> Data;
			std::string strSurfaceTextureName;
		};

		struct MeshSurfaceCache
		{
			enum SurfaceType : UINT8
			{
				Color,
				Normal,
				PBR,
				Emissve,
				Count
			};
			
			std::array<Surface, SurfaceType::Count> Surfaces;
			std::string LightCache;
		};

		EFormat Format = EFormat::RGBA8_UNORM;
		std::vector<MeshSurfaceCache> MeshSurfaceCaches;

		BOOL CheckSurfaceCacheExist() const { return !MeshSurfaceCaches.empty() && !MeshSurfaceCaches[0].Surfaces[0].Data.empty(); }
	};

	struct FSceneGrid
	{
		struct Chunk
		{
			std::unordered_set<FEntity*> pModelEntities;
			BOOL bModelMoved = false;
		};
		
		std::vector<Chunk> Chunks;
		std::vector<FBounds3F> Boxes;
		FBvh Bvh;

		FSceneGrid();
	};

	class FSceneSystem :
		public IEntitySystem,
		public TEventSubscriber<Event::OnModelLoad>,
		public TEventSubscriber<Event::OnModelTransform>,
		public TEventSubscriber<Event::OnComponentAssigned<FMesh>>,
		public TEventSubscriber<Event::OnComponentAssigned<FMaterial>>,
		public TEventSubscriber<Event::OnComponentAssigned<FSurfaceCache>>,
		public TEventSubscriber<Event::OnComponentAssigned<FDistanceField>>
	{
	public:
		BOOL Initialize(FWorld* pWorld) override;
		BOOL Destroy() override;

		BOOL Tick(FWorld* world, FLOAT fDelta) override;

		BOOL Publish(FWorld* pWorld, const Event::OnModelLoad& crEvent) override;
		BOOL Publish(FWorld* pWorld, const Event::OnModelTransform& crEvent) override;
		BOOL Publish(FWorld* pWorld, const Event::OnComponentAssigned<FMesh>& crEvent) override;
		BOOL Publish(FWorld* pWorld, const Event::OnComponentAssigned<FMaterial>& crEvent) override;
		BOOL Publish(FWorld* pWorld, const Event::OnComponentAssigned<FSurfaceCache>& crEvent) override;
		BOOL Publish(FWorld* pWorld, const Event::OnComponentAssigned<FDistanceField>& crEvent) override;

	private:
		FWorld* m_pWorld = nullptr;
		FEntity* m_pGlobalEntity = nullptr;
		std::string m_strModelDirectory;
		std::string m_strSdfDataPath;
		std::string m_strSurfaceCachePath;

		std::unordered_set<std::string> m_LoadedModelNames;
	};
}


















#endif