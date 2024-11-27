#include "../include/Scene.h"
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>
#include <meshoptimizer.h>
#include <json.hpp>
#include "../../Math/include/Quaternion.h"
#include "../../Parallel/include/Parallel.h"
#include "../include/Image.h"
#include "../include/Camera.h"
#include "../../Core/include/ComRoot.h"
#include "../../Core/include/ComIntf.h"
#include "../../Core/include/File.h"
#include "../../Gui/include/GuiPanel.h"


namespace FTS
{
	FDistanceField::TransformData FDistanceField::MeshDistanceField::GetTransformed(const FTransform* cpTransform) const
	{
		TransformData Ret;
		Ret.SdfBox = SdfBox;

		FMatrix4x4 S = Scale(cpTransform->Scale);
		Ret.SdfBox.m_Lower = FVector3F(Mul(FVector4F(Ret.SdfBox.m_Lower, 1.0f), S));
		Ret.SdfBox.m_Upper = FVector3F(Mul(FVector4F(Ret.SdfBox.m_Upper, 1.0f), S));

		FMatrix4x4 R = Rotate(cpTransform->Rotation);
		std::array<FVector3F, 8> BoxVertices;
		BoxVertices[0] = Ret.SdfBox.m_Lower;
		BoxVertices[1] = FVector3F(Ret.SdfBox.m_Lower.x, Ret.SdfBox.m_Upper.y, Ret.SdfBox.m_Lower.z);
		BoxVertices[2] = FVector3F(Ret.SdfBox.m_Upper.x, Ret.SdfBox.m_Upper.y, Ret.SdfBox.m_Lower.z);
		BoxVertices[3] = FVector3F(Ret.SdfBox.m_Upper.x, Ret.SdfBox.m_Lower.y, Ret.SdfBox.m_Lower.z);
		BoxVertices[4] = Ret.SdfBox.m_Upper;
		BoxVertices[7] = FVector3F(Ret.SdfBox.m_Upper.x, Ret.SdfBox.m_Lower.y, Ret.SdfBox.m_Upper.z);
		BoxVertices[5] = FVector3F(Ret.SdfBox.m_Lower.x, Ret.SdfBox.m_Lower.y, Ret.SdfBox.m_Upper.z);
		BoxVertices[6] = FVector3F(Ret.SdfBox.m_Lower.x, Ret.SdfBox.m_Upper.y, Ret.SdfBox.m_Upper.z);

		FBounds3F BlankBox(0.0f, 0.0f);
		for (const auto& crVertex : BoxVertices)
		{
			BlankBox = Union(BlankBox, FVector3F(Mul(FVector4F(crVertex, 1.0f), R)));
		}

		Ret.SdfBox = BlankBox;

		FMatrix4x4 T = Translate(cpTransform->Position);
		Ret.SdfBox.m_Lower = FVector3F(Mul(FVector4F(Ret.SdfBox.m_Lower, 1.0f), T));
		Ret.SdfBox.m_Upper = FVector3F(Mul(FVector4F(Ret.SdfBox.m_Upper, 1.0f), T));

		FVector3F SdfExtent = SdfBox.m_Upper - SdfBox.m_Lower;
		Ret.CoordMatrix = Mul(
			Inverse(Mul(Mul(S, R), T)),		// Local Matrix.
			FMatrix4x4(
				1.0f / SdfExtent.x, 0.0f, 0.0f, 0.0f,
				0.0f, -1.0f / SdfExtent.y, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f / SdfExtent.z, 0.0f,
				-SdfBox.m_Lower.x / SdfExtent.x,
				SdfBox.m_Upper.y / SdfExtent.y,
				-SdfBox.m_Lower.z / SdfExtent.z,
				1.0f
			)
		);
		return Ret;
	}

	FSceneGrid::FSceneGrid()
	{
		UINT32 dwChunkNumPerAxis = gdwGlobalSdfResolution / gdwVoxelNumPerChunk;
		FLOAT fVoxelSize = gfSceneGridSize / gdwGlobalSdfResolution;
		FLOAT fChunkSize = gdwVoxelNumPerChunk * fVoxelSize;

		Chunks.resize(dwChunkNumPerAxis * dwChunkNumPerAxis * dwChunkNumPerAxis);
		std::vector<FBounds3F> Boxes(dwChunkNumPerAxis * dwChunkNumPerAxis * dwChunkNumPerAxis);
		for (UINT32 z = 0; z < dwChunkNumPerAxis; ++z)
			for (UINT32 y = 0; y < dwChunkNumPerAxis; ++y)
				for (UINT32 x = 0; x < dwChunkNumPerAxis; ++x)
				{
					FVector3F Lower = {
						-gfSceneGridSize * 0.5f + x * fChunkSize,
						-gfSceneGridSize * 0.5f + y * fChunkSize,
						-gfSceneGridSize * 0.5f + z * fChunkSize
					};
					Boxes[x + y * dwChunkNumPerAxis + z * dwChunkNumPerAxis * dwChunkNumPerAxis] = FBounds3F(Lower, Lower + fChunkSize);
				}
		FBounds3F GlobalBox(FVector3F(-gfSceneGridSize * 0.5f), FVector3F(gfSceneGridSize * 0.5f));
		Bvh.Build(Boxes, GlobalBox);
	}

	static std::unique_ptr<tinygltf::TinyGLTF> gpGLTFLoader;
	static std::unique_ptr<tinygltf::Model> gpGLTFModel;

	FMatrix4x4 GetMatrixFromGLTFNode(const tinygltf::Node& crGLTFNode);
	void LoadIndicesFromGLTFPrimitive(const tinygltf::Primitive* cpGLTFPrimitive, FMesh::Submesh& rSubmesh);
	void LoadVerticesBoxFromGLTFPrimitive(const tinygltf::Primitive* cpGLTFPrimitive, FMesh::Submesh& rSubmesh);


	BOOL FSceneSystem::Initialize(FWorld* pWorld)
	{
		if (!gpGLTFLoader) gpGLTFLoader = std::make_unique<tinygltf::TinyGLTF>();

		m_pWorld = pWorld;

		pWorld->Subscribe<Event::OnModelLoad>(this);
		pWorld->Subscribe<Event::OnModelTransform>(this);
		pWorld->Subscribe<Event::OnComponentAssigned<FMesh>>(this);
		pWorld->Subscribe<Event::OnComponentAssigned<FMaterial>>(this);
		pWorld->Subscribe<Event::OnComponentAssigned<FSurfaceCache>>(this);
		pWorld->Subscribe<Event::OnComponentAssigned<FDistanceField>>(this);

		m_pGlobalEntity = pWorld->GetGlobalEntity();

		m_pGlobalEntity->Assign<FSceneGrid>();
		m_pGlobalEntity->Assign<Event::GenerateSdf>();
		m_pGlobalEntity->Assign<Event::UpdateGlobalSdf>();
		m_pGlobalEntity->Assign<Event::GenerateSurfaceCache>();

		return true;
	}

	BOOL FSceneSystem::Destroy()
	{
		if (gpGLTFLoader) gpGLTFLoader.reset();

		m_pWorld->UnsubscribeAll(this);
		return true;
	}

	BOOL FSceneSystem::Tick(FWorld* world, FLOAT fDelta)
	{
		ReturnIfFalse(world->Each<FCamera>(
			[fDelta](FEntity* pEntity, FCamera* pCamera) -> BOOL
			{
				pCamera->HandleInput(fDelta);
				return true;
			}
		));

		// Load Model.
		{
			static UINT64 stThreadID = INVALID_SIZE_64;
			static FEntity* pModelEntity = nullptr;

			if (Gui::HasFileSelected() && pModelEntity == nullptr)
			{
				std::string strFilePath = Gui::GetSelectedFilePath();
				std::string strModelName = strFilePath.substr(strFilePath.find("Asset"));
				ReplaceBackSlashes(strModelName);

				if (!m_LoadedModelNames.contains(strModelName))
				{
					pModelEntity = m_pWorld->CreateEntity();
					stThreadID = Parallel::BeginThread(
						[this, strModelName]()
						{
							return m_pWorld->Boardcast(Event::OnModelLoad{
								.pEntity = pModelEntity,
								.strModelPath = strModelName
							});
						}
					);
				}
				else
				{
					Gui::NotifyMessage(Gui::ENotifyType::Info, strModelName + " has already been loaded.");
				}
			}

			if (pModelEntity && Parallel::ThreadFinished(stThreadID) && Parallel::ThreadSuccess(stThreadID))
			{
				ReturnIfFalse(m_pGlobalEntity->GetComponent<Event::GenerateSdf>()->Broadcast(pModelEntity));
				// ReturnIfFalse(m_pGlobalEntity->GetComponent<Event::GenerateSurfaceCache>()->Broadcast(pModelEntity));

				stThreadID = INVALID_SIZE_64;
				pModelEntity = nullptr;
			}
		}

		return true;
	}


	BOOL FSceneSystem::Publish(FWorld* pWorld, const Event::OnModelLoad& crEvent)
	{
		ReturnIfFalse(gpGLTFLoader != nullptr && crEvent.pEntity != nullptr);


		gpGLTFModel.reset();
		gpGLTFModel = std::make_unique<tinygltf::Model>();

		std::string Error;
		std::string Warn;
		std::string strProjDir = PROJ_DIR;

		if (!gpGLTFLoader->LoadASCIIFromFile(gpGLTFModel.get(), &Error, &Warn, strProjDir + crEvent.strModelPath))
		{
			LOG_ERROR("Failed to load model.");
			if (!Error.empty() || !Warn.empty())
			{
				LOG_ERROR(std::string(Error + Warn).c_str());
			}
			return false;
		}

		std::string strModelName = RemoveFileExtension(crEvent.strModelPath.c_str());

		Gui::NotifyMessage(Gui::ENotifyType::Info, "Loaded " + crEvent.strModelPath);

		m_strModelDirectory = crEvent.strModelPath.substr(0, crEvent.strModelPath.find_last_of('/') + 1);
		m_strSdfDataPath = strProjDir + "Asset/SDF/" + strModelName + ".sdf";
		m_strSurfaceCachePath = strProjDir + "Asset/SurfaceCache/" + strModelName + ".sc";

		crEvent.pEntity->Assign<std::string>(strModelName);
		crEvent.pEntity->Assign<FMesh>();
		crEvent.pEntity->Assign<FMaterial>();
		crEvent.pEntity->Assign<FTransform>();
		crEvent.pEntity->Assign<FSurfaceCache>();
		crEvent.pEntity->Assign<FDistanceField>();

		gpGLTFModel.reset();
		m_strSdfDataPath.clear();
		m_strModelDirectory.clear();

		m_LoadedModelNames.insert(crEvent.strModelPath);

		
		FEntity* pTmpModelEntity = crEvent.pEntity;
		Gui::Add(
			[pTmpModelEntity, this]()
			{
				std::string strModelName = *pTmpModelEntity->GetComponent<std::string>();
				strModelName += " Transform";
				if (ImGui::TreeNode(strModelName.c_str()))
				{
					BOOL bChanged = false;

					FTransform TmpTransform = *pTmpModelEntity->GetComponent<FTransform>();
					bChanged |= ImGui::SliderFloat3("Position", reinterpret_cast<FLOAT*>(&TmpTransform.Position), -32.0f, 32.0f);		
					bChanged |= ImGui::SliderFloat3("Rotation", reinterpret_cast<FLOAT*>(&TmpTransform.Rotation), -180.0f, 180.0f);
					bChanged |= ImGui::SliderFloat3("Scale", reinterpret_cast<FLOAT*>(&TmpTransform.Scale), 0.1f, 8.0f);
					if (bChanged) m_pWorld->Boardcast(Event::OnModelTransform{ .pEntity = pTmpModelEntity, .Trans = TmpTransform });

					ImGui::TreePop();
				}
			}
		);

		return true;
	}

	BOOL FSceneSystem::Publish(FWorld* pWorld, const Event::OnComponentAssigned<FMesh>& crEvent)
	{
		const auto& GLTFScene = gpGLTFModel->scenes[gpGLTFModel->defaultScene];

		std::vector<FMatrix4x4> WorldMatrixs;
		std::vector<const tinygltf::Primitive*> cpPrimitives;

		std::function<void(const tinygltf::Node&, const FMatrix4x4&)> Func;
		Func = [&](const tinygltf::Node& crGLTFNode, const FMatrix4x4& crParentMatrix) -> void
			{
				FMatrix4x4 WorldMatrix = Mul(crParentMatrix, GetMatrixFromGLTFNode(crGLTFNode));

				if (crGLTFNode.mesh >= 0)
				{
					const auto& crPrimitives = gpGLTFModel->meshes[crGLTFNode.mesh].primitives;
					for (const auto& crPrimitive : crPrimitives)
					{
						cpPrimitives.emplace_back(&crPrimitive);
						WorldMatrixs.push_back(WorldMatrix);
					}
				}

				for (int dwChildNodeIndex : crGLTFNode.children)
				{
					Func(gpGLTFModel->nodes[dwChildNodeIndex], WorldMatrixs.back());
				}
			};

		for (UINT32 ix = 0; ix < GLTFScene.nodes.size(); ++ix)
		{
			Func(gpGLTFModel->nodes[GLTFScene.nodes[ix]], crEvent.pComponent->WorldMatrix);
		}

		FMesh* pMesh = crEvent.pComponent;
		auto& rSubmeshes = pMesh->Submeshes;
		rSubmeshes.resize(cpPrimitives.size());

		Parallel::ParallelFor(
			[&](UINT64 ix)
			{
				auto& rSubmesh = rSubmeshes[ix];
				rSubmesh.WorldMatrix = WorldMatrixs[ix];
				rSubmesh.dwMaterialIndex = cpPrimitives[ix]->material;

				LoadIndicesFromGLTFPrimitive(cpPrimitives[ix], rSubmesh);
				LoadVerticesBoxFromGLTFPrimitive(cpPrimitives[ix], rSubmesh);
			},
			cpPrimitives.size()
		);
		return true;
	}

	BOOL FSceneSystem::Publish(FWorld* pWorld, const Event::OnComponentAssigned<FMaterial>& crEvent)
	{
		std::string strFilePath = PROJ_DIR + m_strModelDirectory;
		
		auto& rpMaterial = crEvent.pComponent;

		if (!gpGLTFModel)
		{
			LOG_ERROR("You should load a model first.");
			return false;
		}

		rpMaterial->SubMaterials.resize(gpGLTFModel->materials.size());

		Parallel::ParallelFor(
			[&rpMaterial, &strFilePath](UINT64 ix)
			{
				auto& rSubMaterial = rpMaterial->SubMaterials[ix];

				const auto& crGLTFMaterial = gpGLTFModel->materials[ix];
				rSubMaterial.fDiffuseFactor[0] = static_cast<FLOAT>(crGLTFMaterial.pbrMetallicRoughness.baseColorFactor[0]);
				rSubMaterial.fDiffuseFactor[1] = static_cast<FLOAT>(crGLTFMaterial.pbrMetallicRoughness.baseColorFactor[1]);
				rSubMaterial.fDiffuseFactor[2] = static_cast<FLOAT>(crGLTFMaterial.pbrMetallicRoughness.baseColorFactor[2]);
				rSubMaterial.fDiffuseFactor[3] = static_cast<FLOAT>(crGLTFMaterial.pbrMetallicRoughness.baseColorFactor[3]);
				rSubMaterial.fRoughnessFactor = static_cast<FLOAT>(crGLTFMaterial.pbrMetallicRoughness.roughnessFactor);
				rSubMaterial.fMetallicFactor = static_cast<FLOAT>(crGLTFMaterial.pbrMetallicRoughness.metallicFactor);
				rSubMaterial.fOcclusionFactor = static_cast<FLOAT>(crGLTFMaterial.occlusionTexture.strength);
				rSubMaterial.fEmissiveFactor[0] = static_cast<FLOAT>(crGLTFMaterial.emissiveFactor[0]);
				rSubMaterial.fEmissiveFactor[1] = static_cast<FLOAT>(crGLTFMaterial.emissiveFactor[1]);
				rSubMaterial.fEmissiveFactor[2] = static_cast<FLOAT>(crGLTFMaterial.emissiveFactor[2]);


				if (crGLTFMaterial.pbrMetallicRoughness.baseColorTexture.index >= 0)
				{
					const auto& GLTFTexture = gpGLTFModel->textures[crGLTFMaterial.pbrMetallicRoughness.baseColorTexture.index];
					const auto& GLTFImage = gpGLTFModel->images[GLTFTexture.source];

					rSubMaterial.Images[FMaterial::TextureType_Diffuse] = Image::LoadImageFromFile((strFilePath + GLTFImage.uri).c_str());
				}
				if (crGLTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
				{
					const auto& GLTFTexture = gpGLTFModel->textures[crGLTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index];
					const auto& GLTFImage = gpGLTFModel->images[GLTFTexture.source];

					rSubMaterial.Images[FMaterial::TextureType_MetallicRoughness] = Image::LoadImageFromFile((strFilePath + GLTFImage.uri).c_str());
				}
				if (crGLTFMaterial.normalTexture.index >= 0)
				{
					const auto& GLTFTexture = gpGLTFModel->textures[crGLTFMaterial.normalTexture.index];
					const auto& GLTFImage = gpGLTFModel->images[GLTFTexture.source];

					rSubMaterial.Images[FMaterial::TextureType_Normal] = Image::LoadImageFromFile((strFilePath + GLTFImage.uri).c_str());
				}
				if (crGLTFMaterial.occlusionTexture.index >= 0)
				{
					const auto& GLTFTexture = gpGLTFModel->textures[crGLTFMaterial.occlusionTexture.index];
					const auto& GLTFImage = gpGLTFModel->images[GLTFTexture.source];

					rSubMaterial.Images[FMaterial::TextureType_Occlusion] = Image::LoadImageFromFile((strFilePath + GLTFImage.uri).c_str());
				}
				if (crGLTFMaterial.emissiveTexture.index >= 0)
				{
					const auto& GLTFTexture = gpGLTFModel->textures[crGLTFMaterial.emissiveTexture.index];
					const auto& GLTFImage = gpGLTFModel->images[GLTFTexture.source];

					rSubMaterial.Images[FMaterial::TextureType_Emissive] = Image::LoadImageFromFile((strFilePath + GLTFImage.uri).c_str());
				}
			},
			rpMaterial->SubMaterials.size()
		);
		return true;
	}

	BOOL FSceneSystem::Publish(FWorld* pWorld, const Event::OnComponentAssigned<FSurfaceCache>& crEvent)
	{
		FSurfaceCache* pSurfaceCache = crEvent.pComponent;
		FMesh* pMesh = crEvent.pEntity->GetComponent<FMesh>();
		pSurfaceCache->MeshSurfaceCaches.resize(pMesh->Submeshes.size());

		BOOL bLoadFromFile = false;
		if (IsFileExist(m_strSurfaceCachePath.c_str()))
		{
			Serialization::BinaryInput Input(m_strSurfaceCachePath);
			UINT32 dwCardResolution = 0;
			UINT32 dwSurfaceResolution = 0;
			Input(dwCardResolution);
			Input(dwSurfaceResolution);

			if (dwCardResolution == gdwCardResolution && dwSurfaceResolution == gdwSurfaceResolution)
			{
				FFormatInfo FormaInfo = GetFormatInfo(pSurfaceCache->Format);
				UINT64 stDataSize = static_cast<UINT64>(gdwSurfaceResolution) * gdwSurfaceResolution * FormaInfo.btBytesPerBlock;

				for (UINT32 ix = 0; ix < pSurfaceCache->MeshSurfaceCaches.size(); ++ix)
				{
					auto& rMeshSC = pSurfaceCache->MeshSurfaceCaches[ix];
					for (UINT32 ix = 0; ix < FSurfaceCache::MeshSurfaceCache::SurfaceType::Count; ++ix)
					{
						rMeshSC.Surfaces[ix].strSurfaceTextureName = *crEvent.pEntity->GetComponent<std::string>() + "SurfaceTexture" + std::to_string(ix);
						rMeshSC.Surfaces[ix].Data.resize(stDataSize);
						Input.LoadBinaryData(rMeshSC.Surfaces[ix].Data.data(), stDataSize);
					}
				}
				Gui::NotifyMessage(Gui::ENotifyType::Info, "Loaded " + m_strSdfDataPath.substr(m_strSdfDataPath.find("Asset")));
				bLoadFromFile = true;
			}
		}

		if (!bLoadFromFile)
		{
			std::string strModelName = *crEvent.pEntity->GetComponent<std::string>();
			for (UINT32 ix = 0; ix < pSurfaceCache->MeshSurfaceCaches.size(); ++ix)
			{
				std::string strMeshIndex = std::to_string(ix);
				auto& rMeshSC = pSurfaceCache->MeshSurfaceCaches[ix];
				rMeshSC.Surfaces[0].strSurfaceTextureName = strModelName + "SurfaceColorTexture" + strMeshIndex;
				rMeshSC.Surfaces[1].strSurfaceTextureName = strModelName + "SurfaceNormalTexture" + strMeshIndex;
				rMeshSC.Surfaces[2].strSurfaceTextureName = strModelName + "SurfacePBRTexture" + strMeshIndex;
				rMeshSC.Surfaces[3].strSurfaceTextureName = strModelName + "SurfaceEmissveTexture" + strMeshIndex;
				rMeshSC.LightCache = strModelName + "SurfaceLightTexture" + strMeshIndex;
			}
		}

		return true;
	}

	BOOL FSceneSystem::Publish(FWorld* pWorld, const Event::OnComponentAssigned<FDistanceField>& crEvent)
	{
		FDistanceField* pDistanceField = crEvent.pComponent;
		FMesh* pMesh = crEvent.pEntity->GetComponent<FMesh>();
		pDistanceField->MeshDistanceFields.resize(pMesh->Submeshes.size());

		BOOL bLoadFromFile = false;
		if (IsFileExist(m_strSdfDataPath.c_str()))
		{
			Serialization::BinaryInput Input(m_strSdfDataPath);
			UINT32 dwMeshSdfResolution = 0;
			Input(dwMeshSdfResolution);
			
			if (dwMeshSdfResolution == gdwSdfResolution)
			{
				for (UINT32 ix = 0; ix < pDistanceField->MeshDistanceFields.size(); ++ix)
				{
					auto& rMeshDF = pDistanceField->MeshDistanceFields[ix];
					rMeshDF.strSdfTextureName = *crEvent.pEntity->GetComponent<std::string>() + "SdfTexture" + std::to_string(ix);

					Input(
						rMeshDF.SdfBox.m_Lower.x,
						rMeshDF.SdfBox.m_Lower.y,
						rMeshDF.SdfBox.m_Lower.z,
						rMeshDF.SdfBox.m_Upper.x,
						rMeshDF.SdfBox.m_Upper.y,
						rMeshDF.SdfBox.m_Upper.z
					);

					UINT64 stDataSize = static_cast<UINT64>(gdwSdfResolution) * gdwSdfResolution * gdwSdfResolution * sizeof(FLOAT);
					rMeshDF.SdfData.resize(stDataSize);
					Input.LoadBinaryData(rMeshDF.SdfData.data(), stDataSize);
				}
				Gui::NotifyMessage(Gui::ENotifyType::Info, "Loaded " + m_strSdfDataPath.substr(m_strSdfDataPath.find("Asset")));
				bLoadFromFile = true;
			}
		}

		if (!bLoadFromFile)
		{
			std::string strModelName = *crEvent.pEntity->GetComponent<std::string>();
			for (UINT32 ix = 0; ix < pDistanceField->MeshDistanceFields.size(); ++ix)
			{
				auto& rMeshDF = pDistanceField->MeshDistanceFields[ix];
				const auto& crSubmesh = pMesh->Submeshes[ix];
				rMeshDF.strSdfTextureName = strModelName + "SdfTexture" + std::to_string(ix);

				UINT64 jx = 0;
				std::vector<FBvh::Vertex> BvhVertices(crSubmesh.Indices.size());
				for (auto VertexIndex : crSubmesh.Indices)
				{
					BvhVertices[jx++] = {
						FVector3F(Mul(FVector4F(crSubmesh.Vertices[VertexIndex].Position, 1.0f), crSubmesh.WorldMatrix)),
						FVector3F(Mul(FVector4F(crSubmesh.Vertices[VertexIndex].Normal, 1.0f), Transpose(Inverse(crSubmesh.WorldMatrix))))
					};
				}

				rMeshDF.Bvh.Build(BvhVertices, static_cast<UINT32>(crSubmesh.Indices.size() / 3));
				rMeshDF.SdfBox = rMeshDF.Bvh.GlobalBox;
			}
		}

		FSceneGrid* pGrid = m_pGlobalEntity->GetComponent<FSceneGrid>();

		for (const auto& crMeshDF : pDistanceField->MeshDistanceFields)
		{
			FDistanceField::TransformData Data = crMeshDF.GetTransformed(crEvent.pEntity->GetComponent<FTransform>());
			UINT32 dwChunkNumPerAxis = gdwGlobalSdfResolution / gdwVoxelNumPerChunk;
			FLOAT fVoxelSize = gfSceneGridSize / gdwGlobalSdfResolution;
			FLOAT fChunkSize = gdwVoxelNumPerChunk * fVoxelSize;
			FLOAT fGridSize = fChunkSize * dwChunkNumPerAxis;

			FVector3I UniformLower = FVector3I((Data.SdfBox.m_Lower + fGridSize / 2.0f) / fChunkSize);
			FVector3I UniformUpper = FVector3I((Data.SdfBox.m_Upper + fGridSize / 2.0f) / fChunkSize);

			for (UINT32 ix = 0; ix < 3; ++ix)
			{
				if (UniformLower[ix] != 0) UniformLower[ix] -= 1;
				if (UniformUpper[ix] != dwChunkNumPerAxis - 1) UniformUpper[ix] += 1;
			}

			for (UINT32 z = UniformLower.z; z <= UniformUpper.z; ++z)
			{
				for (UINT32 y = UniformLower.y; y <= UniformUpper.y; ++y)
				{
					for (UINT32 x = UniformLower.x; x <= UniformUpper.x; ++x)
					{
						UINT32 dwIndex = x + y * dwChunkNumPerAxis + z * dwChunkNumPerAxis * dwChunkNumPerAxis;
						pGrid->Chunks[dwIndex].bModelMoved = true;
						pGrid->Chunks[dwIndex].pModelEntities.insert(crEvent.pEntity);
					}
				}
			}
		}
		
		return true;
	}

	BOOL FSceneSystem::Publish(FWorld* pWorld, const Event::OnModelTransform& crEvent)
	{
		FTransform* pTransform = crEvent.pEntity->GetComponent<FTransform>();
		FDistanceField* pDistanceField = crEvent.pEntity->GetComponent<FDistanceField>();

		UINT32 dwChunkNumPerAxis = gdwGlobalSdfResolution / gdwVoxelNumPerChunk;
		FLOAT fVoxelSize = gfSceneGridSize / gdwGlobalSdfResolution;
		FLOAT fChunkSize = 1.0f * gdwVoxelNumPerChunk * fVoxelSize;

		FSceneGrid* pGrid = m_pGlobalEntity->GetComponent<FSceneGrid>();

		auto FuncMark = [&](const FBounds3F& crBox, BOOL bInsertOrErase)
		{
			FVector3I UniformLower = FVector3I((crBox.m_Lower + gfSceneGridSize / 2.0f) / fChunkSize);
			FVector3I UniformUpper = FVector3I((crBox.m_Upper + gfSceneGridSize / 2.0f) / fChunkSize);

			for (UINT32 ix = 0; ix < 3; ++ix)
			{
				if (UniformLower[ix] != 0) UniformLower[ix] -= 1;
				if (UniformUpper[ix] != dwChunkNumPerAxis - 1) UniformUpper[ix] += 1;
			}

			for (UINT32 z = UniformLower.z; z <= UniformUpper.z; ++z)
			{
				for (UINT32 y = UniformLower.y; y <= UniformUpper.y; ++y)
				{
					for (UINT32 x = UniformLower.x; x <= UniformUpper.x; ++x)
					{
						UINT32 dwIndex = x + y * dwChunkNumPerAxis + z * dwChunkNumPerAxis * dwChunkNumPerAxis;
						pGrid->Chunks[dwIndex].bModelMoved = true;
						if (bInsertOrErase) 
							pGrid->Chunks[dwIndex].pModelEntities.insert(crEvent.pEntity);
						else				
							pGrid->Chunks[dwIndex].pModelEntities.erase(crEvent.pEntity);
					}
				}
			}

		};

		for (const auto& crMeshDF : pDistanceField->MeshDistanceFields)
		{
			FBounds3F OldSdfBox = crMeshDF.GetTransformed(pTransform).SdfBox;
			FBounds3F NewSdfBox = crMeshDF.GetTransformed(&crEvent.Trans).SdfBox;

			FuncMark(OldSdfBox, false);
			FuncMark(NewSdfBox, true);
		}

		*pTransform = crEvent.Trans;

		return pWorld->Each<Event::UpdateGlobalSdf>(
			[](FEntity* pEntity, Event::UpdateGlobalSdf* pEvent) -> BOOL
			{
				return pEvent->Broadcast();
			}
		);
	}

	FMatrix4x4 GetMatrixFromGLTFNode(const tinygltf::Node& crGLTFNode)
	{
		struct WorldTransform
		{
			FVector3F TranslationVector = { 0.0f, 0.0f, 0.0f };
			FVector3F ScaleVector = { 1.0f, 1.0f, 1.0f };
			FQuaternion RotationVector;
			FMatrix4x4 WorldMatirx;

			void CreateWorldMatrix()
			{
				WorldMatirx = Mul(Mul(Translate(TranslationVector), Scale(ScaleVector)), RotationVector.ToMatrix());
			}
		};

		WorldTransform Transform;

		if (!crGLTFNode.matrix.empty())
		{
			Transform.WorldMatirx = {
					static_cast<FLOAT>(crGLTFNode.matrix[0]),
					static_cast<FLOAT>(crGLTFNode.matrix[1]),
					static_cast<FLOAT>(crGLTFNode.matrix[2]),
					static_cast<FLOAT>(crGLTFNode.matrix[3]),
					static_cast<FLOAT>(crGLTFNode.matrix[4]),
					static_cast<FLOAT>(crGLTFNode.matrix[5]),
					static_cast<FLOAT>(crGLTFNode.matrix[6]),
					static_cast<FLOAT>(crGLTFNode.matrix[7]),
					static_cast<FLOAT>(crGLTFNode.matrix[8]),
					static_cast<FLOAT>(crGLTFNode.matrix[9]),
					static_cast<FLOAT>(crGLTFNode.matrix[10]),
					static_cast<FLOAT>(crGLTFNode.matrix[11]),
					static_cast<FLOAT>(crGLTFNode.matrix[12]),
					static_cast<FLOAT>(crGLTFNode.matrix[13]),
					static_cast<FLOAT>(crGLTFNode.matrix[14]),
					static_cast<FLOAT>(crGLTFNode.matrix[15])
			};
		}
		else
		{
			if (!crGLTFNode.translation.empty()) Transform.TranslationVector = { (FLOAT)crGLTFNode.translation[0], (FLOAT)crGLTFNode.translation[1], (FLOAT)crGLTFNode.translation[2] };
			if (!crGLTFNode.scale.empty()) Transform.ScaleVector = { (FLOAT)crGLTFNode.scale[0], (FLOAT)crGLTFNode.scale[1], (FLOAT)crGLTFNode.scale[2] };
			if (!crGLTFNode.rotation.empty()) Transform.RotationVector = { (FLOAT)crGLTFNode.rotation[0], (FLOAT)crGLTFNode.rotation[1], (FLOAT)crGLTFNode.rotation[2], (FLOAT)crGLTFNode.rotation[3] };

			Transform.CreateWorldMatrix();
		}

		return Transform.WorldMatirx;
	}

	void LoadIndicesFromGLTFPrimitive(const tinygltf::Primitive* cpGLTFPrimitive, FMesh::Submesh& rSubmesh)
	{
		const auto& GLTFIndicesAccessor = gpGLTFModel->accessors[cpGLTFPrimitive->indices];
		const auto& GLTFIndicesBufferView = gpGLTFModel->bufferViews[GLTFIndicesAccessor.bufferView];
		const auto& GLTFIndicesBuffer = gpGLTFModel->buffers[GLTFIndicesBufferView.buffer];

		rSubmesh.Indices.reserve(GLTFIndicesAccessor.count);

		auto AddIndices = [&]<typename T>()
		{
			const T* IndexData = reinterpret_cast<const T*>(GLTFIndicesBuffer.data.data() + GLTFIndicesBufferView.byteOffset + GLTFIndicesAccessor.byteOffset);
			for (UINT64 ix = 0; ix < GLTFIndicesAccessor.count; ix += 3)
			{
				// Ĭ��Ϊ˳ʱ����ת
				rSubmesh.Indices.push_back(IndexData[ix + 0]);
				rSubmesh.Indices.push_back(IndexData[ix + 1]);
				rSubmesh.Indices.push_back(IndexData[ix + 2]);
			}
		};

		const UINT32 IndexStride = GLTFIndicesAccessor.ByteStride(GLTFIndicesBufferView);
		switch (IndexStride)
		{
		case 1: AddIndices.operator() < UINT8 > (); break;
		case 2: AddIndices.operator() < UINT16 > (); break;
		case 4: AddIndices.operator() < UINT32 > (); break;
		default:
			assert(!"Doesn't support such stride.");
		}
	}

	void LoadVerticesBoxFromGLTFPrimitive(const tinygltf::Primitive* cpGLTFPrimitive, FMesh::Submesh& rSubmesh)
	{
		UINT32 TempCounter = 0;
		UINT64 stVertexCount = 0;
		UINT32 AttributeStride[4] = { 0 };
		auto FunctionLoadAttribute = [&](const std::string& InAttributeName, BOOL& bExist)
			{
				if (!bExist) return SIZE_T(0);
				const auto Iterator = cpGLTFPrimitive->attributes.find(InAttributeName);
				if (Iterator == cpGLTFPrimitive->attributes.end())
				{
					bExist = false;
					return SIZE_T(0);
				}
				const auto& GLTFAttributeAccessor = gpGLTFModel->accessors[Iterator->second];
				const auto& GLTFAttributeBufferView = gpGLTFModel->bufferViews[GLTFAttributeAccessor.bufferView];
				const auto& GLTFAttributeBuffer = gpGLTFModel->buffers[GLTFAttributeBufferView.buffer];

				stVertexCount = GLTFAttributeAccessor.count;
				AttributeStride[TempCounter++] = GLTFAttributeAccessor.ByteStride(GLTFAttributeBufferView);

				if (TempCounter >= 5)
				{
					LOG_ERROR("LoadGLTFNode() calling has a mistake which is Model's attribute is too more.");
					bExist = false;
					return SIZE_T(0);
				}
				bExist = true;
				return reinterpret_cast<SIZE_T>(GLTFAttributeBuffer.data.data() + GLTFAttributeBufferView.byteOffset + GLTFAttributeAccessor.byteOffset);
			};


		BOOL bPos = true, bNor = true, bTan = true, bUV = true;
		SIZE_T PositionData = FunctionLoadAttribute("POSITION", bPos);
		SIZE_T NormalData = FunctionLoadAttribute("NORMAL", bNor);
		SIZE_T TangentData = FunctionLoadAttribute("TANGENT", bTan);
		SIZE_T UVData = FunctionLoadAttribute("TEXCOORD_0", bUV);

		std::vector<FVector3F> PositionStream;
		std::vector<FVector3F> NormalStream;
		std::vector<FVector4F> TangentStream;
		std::vector<FVector2F> UVStream;

		std::vector<UINT32> remap(stVertexCount);

		if (bPos)
		{
			PositionStream.resize(stVertexCount);
			for (UINT32 ix = 0; ix < stVertexCount; ++ix) PositionStream[ix] = *reinterpret_cast<FVector3F*>(PositionData + ix * AttributeStride[0]);

			meshopt_optimizeVertexCache(rSubmesh.Indices.data(), rSubmesh.Indices.data(), rSubmesh.Indices.size(), stVertexCount);
			meshopt_optimizeOverdraw(
				rSubmesh.Indices.data(),
				rSubmesh.Indices.data(),
				rSubmesh.Indices.size(),
				&PositionStream[0].x,
				stVertexCount,
				sizeof(FVector3F),
				1.05f
			);

			meshopt_optimizeVertexFetchRemap(&remap[0], rSubmesh.Indices.data(), rSubmesh.Indices.size(), stVertexCount);
			meshopt_remapIndexBuffer(rSubmesh.Indices.data(), rSubmesh.Indices.data(), rSubmesh.Indices.size(), &remap[0]);
			meshopt_remapVertexBuffer(PositionStream.data(), PositionStream.data(), stVertexCount, sizeof(FVector3F), &remap[0]);
		}
		if (bNor)
		{
			NormalStream.resize(stVertexCount);
			for (UINT32 ix = 0; ix < stVertexCount; ++ix) NormalStream[ix] = *reinterpret_cast<FVector3F*>(NormalData + ix * AttributeStride[1]);
			meshopt_remapVertexBuffer(NormalStream.data(), NormalStream.data(), NormalStream.size(), sizeof(FVector3F), &remap[0]);
		}
		if (bTan)
		{
			TangentStream.resize(stVertexCount);
			for (UINT32 ix = 0; ix < stVertexCount; ++ix) TangentStream[ix] = *reinterpret_cast<FVector4F*>(TangentData + ix * AttributeStride[2]);
			meshopt_remapVertexBuffer(TangentStream.data(), TangentStream.data(), TangentStream.size(), sizeof(FVector4F), &remap[0]);
		}
		if (bUV)
		{
			UVStream.resize(stVertexCount);
			for (UINT32 ix = 0; ix < stVertexCount; ++ix) UVStream[ix] = *reinterpret_cast<FVector2F*>(UVData + ix * AttributeStride[3]);
			meshopt_remapVertexBuffer(UVStream.data(), UVStream.data(), UVStream.size(), sizeof(FVector2F), &remap[0]);
		}


		rSubmesh.Vertices.resize(stVertexCount);
		for (UINT32 ix = 0; ix < stVertexCount; ++ix)
		{
			if (bPos) rSubmesh.Vertices[ix].Position = PositionStream[ix];
			if (bNor) rSubmesh.Vertices[ix].Normal = NormalStream[ix];
			if (bTan) rSubmesh.Vertices[ix].Tangent = TangentStream[ix];
			if (bUV) rSubmesh.Vertices[ix].UV = UVStream[ix];
		}
	}

}