#ifndef SCENE_H
#define SCENE_H
#include "Camera.h"
#include "Geometry.h"
#include "Image.h"
#include "Light.h"
#include "../../Core/include/Delegate.h"
#include <unordered_set>

namespace FTS
{
	struct FDistanceField;

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
		};

		DeclareDelegateEvent(GenerateSdf, FDistanceField*);
		DeclareDelegateEvent(UpdateSceneGrid);
	};
	
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

	struct FDistanceField
	{
		std::string strSdfTextureName;
		
		FMatrix4x4 LocalMatrix;
		FMatrix4x4 WorldMatrix;
		FMatrix4x4 CoordMatrix;
		FBounds3F SdfBox;
		
		std::vector<UINT8> SdfData;
		FBvh Bvh;

		void TransformUpdate(const FTransform* cpTransform);
		BOOL CheckSdfFileExist() const { return !SdfData.empty(); }
	};

	inline const UINT32 gdwGlobalSdfResolution = 256;
	inline const UINT32 gdwVoxelNumPerChunk = 16;
	inline const UINT32 gdwMaxGIDistance = 512;
	inline const UINT32 gdwSdfResolution = 64;

	struct FSceneGrid
	{
		struct Chunk
		{
			std::unordered_set<FEntity*> pModelEntities;
			BOOL bModelMoved = true;
		};
		
		std::vector<Chunk> Chunks;
	};

	class FSceneSystem :
		public IEntitySystem,
		public TEventSubscriber<Event::OnModelLoad>,
		public TEventSubscriber<Event::OnModelTransform>,
		public TEventSubscriber<Event::OnComponentAssigned<FMesh>>,
		public TEventSubscriber<Event::OnComponentAssigned<FMaterial>>,
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
		BOOL Publish(FWorld* pWorld, const Event::OnComponentAssigned<FDistanceField>& crEvent) override;

	private:
		FWorld* m_pWorld = nullptr;
		std::string m_strModelDirectory;
		std::string m_strSdfDataPath;
	};
}


















#endif