#ifndef SCENE_H
#define SCENE_H
#include "Camera.h"
#include "Geometry.h"
#include "Image.h"
#include "Light.h"

namespace FTS
{
	namespace Event
	{
		struct OnGeometryLoad
		{
			FEntity* pEntity = nullptr;
			std::string strModelPath;
		};
	};
	
	struct FTransform
	{
		FVector3F Translation = { 0.0f, 0.0f, 0.0f };
		FVector3F Rotation = { 0.0f, 0.0f, 0.0f };
		FVector3F Scale = { 1.0f, 1.0f, 1.0f };
	};

	inline const UINT32 gdwVoxelNumPerChunk = 32;

	struct FSceneGrid
	{
		struct Chunk
		{
			std::vector<FEntity*> pModelEntities;
		};
		
		std::vector<Chunk> Chunks;
	};

	class FSceneSystem :
		public IEntitySystem,
		public TEventSubscriber<Event::OnGeometryLoad>,
		public TEventSubscriber<Event::OnComponentAssigned<FMesh>>,
		public TEventSubscriber<Event::OnComponentAssigned<FCamera>>,
		public TEventSubscriber<Event::OnComponentAssigned<FMaterial>>,
		public TEventSubscriber<Event::OnComponentAssigned<std::string>>,
		public TEventSubscriber<Event::OnComponentAssigned<FDistanceField>>
	{
	public:
		BOOL Initialize(FWorld* pWorld) override;
		BOOL Destroy() override;

		void Tick(FWorld* world, FLOAT fDelta) override;

		BOOL Publish(FWorld* pWorld, const Event::OnGeometryLoad& crEvent) override;
		BOOL Publish(FWorld* pWorld, const Event::OnComponentAssigned<FMesh>& crEvent) override;
		BOOL Publish(FWorld* pWorld, const Event::OnComponentAssigned<FCamera>& crEvent) override;
		BOOL Publish(FWorld* pWorld, const Event::OnComponentAssigned<FMaterial>& crEvent) override;
		BOOL Publish(FWorld* pWorld, const Event::OnComponentAssigned<std::string>& crEvent) override;
		BOOL Publish(FWorld* pWorld, const Event::OnComponentAssigned<FDistanceField>& crEvent) override;

	private:
		FWorld* m_pWorld = nullptr;
		std::string m_strModelDirectory;
		std::string m_strSdfDataPath;
	};
}


















#endif