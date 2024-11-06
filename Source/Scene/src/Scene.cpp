#include "../include/Scene.h"
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>
#include <meshoptimizer.h>
#include <filesystem>
#include <json.hpp>
#include "../../Math/include/Quaternion.h"
#include "../../TaskFlow/include/TaskFlow.h"
#include "../include/Image.h"
#include "../../Core/include/ComRoot.h"
#include "../../Core/include/ComIntf.h"
#include "../../Core/include/File.h"


namespace FTS
{
	static std::unique_ptr<tinygltf::TinyGLTF> gpGLTFLoader;
	static std::unique_ptr<tinygltf::Model> gpGLTFModel;

	FMatrix4x4 GetMatrixFromGLTFNode(const tinygltf::Node& crGLTFNode);
	void LoadIndicesFromGLTFPrimitive(const tinygltf::Primitive* cpGLTFPrimitive, FMesh::SubMesh& rSubMesh);
	void LoadVerticesBoxFromGLTFPrimitive(const tinygltf::Primitive* cpGLTFPrimitive, FMesh::SubMesh& rSubMesh);


	BOOL FSceneSystem::Initialize(FWorld* pWorld)
	{
		if (!gpGLTFLoader) gpGLTFLoader = std::make_unique<tinygltf::TinyGLTF>();

		m_pWorld = pWorld;

		pWorld->Subscribe<Event::OnGeometryLoad>(this);
		pWorld->Subscribe<Event::OnComponentAssigned<FMesh>>(this);
		pWorld->Subscribe<Event::OnComponentAssigned<FMaterial>>(this);
		pWorld->Subscribe<Event::OnComponentAssigned<std::string>>(this);
		pWorld->Subscribe<Event::OnComponentAssigned<FDistanceField>>(this);
		return true;
	}

	BOOL FSceneSystem::Destroy()
	{
		if (gpGLTFLoader) gpGLTFLoader.reset();

		m_pWorld->UnsubscribeAll(this);
		return true;
	}

	void FSceneSystem::Tick(FWorld* world, FLOAT fDelta)
	{
		world->Each<FCamera>(
			[fDelta](FEntity* pEntity, FCamera* pCamera) -> BOOL
			{
				pCamera->HandleInput(fDelta);
				return true;
			}
		);
	}


	BOOL FSceneSystem::Publish(FWorld* pWorld, const Event::OnGeometryLoad& crEvent)
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

		LOG_INFO("Loaded GLTF: " + strProjDir + crEvent.strModelPath);

		std::string strModelName = RemoveFileExtension(crEvent.strModelPath.c_str());

		m_strModelDirectory = crEvent.strModelPath.substr(0, crEvent.strModelPath.find_last_of('/') + 1);
		m_strSdfDataPath = strProjDir + "Asset/SDF/" + strModelName + ".sdf";

		crEvent.pEntity->Assign<std::string>(strModelName);
		crEvent.pEntity->Assign<FMesh>();
		crEvent.pEntity->Assign<FMaterial>();
		crEvent.pEntity->Assign<FTransform>();
		crEvent.pEntity->Assign<FDistanceField>();

		gpGLTFModel.reset();

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
		auto& rSubMeshes = pMesh->SubMeshes;
		rSubMeshes.resize(cpPrimitives.size());

		TaskFlow::ParallelFor(
			[&](UINT64 ix)
			{
				auto& rSubMesh = rSubMeshes[ix];
				rSubMesh.WorldMatrix = WorldMatrixs[ix];
				rSubMesh.dwMaterialIndex = cpPrimitives[ix]->material;

				LoadIndicesFromGLTFPrimitive(cpPrimitives[ix], rSubMesh);
				LoadVerticesBoxFromGLTFPrimitive(cpPrimitives[ix], rSubMesh);
				pMesh->Box = Union(pMesh->Box, rSubMesh.Box);
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

		TaskFlow::ParallelFor(
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

	BOOL FSceneSystem::Publish(FWorld* pWorld, const Event::OnComponentAssigned<std::string>& crEvent)
	{
		return true;
	}

	BOOL FSceneSystem::Publish(FWorld* pWorld, const Event::OnComponentAssigned<FDistanceField>& crEvent)
	{
		const FMesh* pMesh = crEvent.pEntity->GetComponent<FMesh>();

		UINT64 stIndicesNum = 0;
		for (const auto& crMesh : pMesh->SubMeshes)
		{
			stIndicesNum += crMesh.Indices.size();
		}

		UINT64 ix = 0;
		std::vector<FBvh::Vertex> BvhVertices(stIndicesNum);
		for (const auto& crMesh : pMesh->SubMeshes)
		{
			for (auto VertexId : crMesh.Indices)
			{
				BvhVertices[ix++] = {
					FVector3F(Mul(FVector4F(crMesh.Vertices[VertexId].Position, 1.0f), crMesh.WorldMatrix)),
					FVector3F(Mul(FVector4F(crMesh.Vertices[VertexId].Normal, 1.0f), Transpose(Inverse(crMesh.WorldMatrix))))
				};
			}
		}
		ReturnIfFalse(ix == stIndicesNum);

		FDistanceField* pDistanceField = crEvent.pComponent;
		pDistanceField->pBvh = std::make_shared<FBvh>();
		pDistanceField->pBvh->Build(BvhVertices, stIndicesNum / 3);
		pDistanceField->SdfBox = pDistanceField->pBvh->GlobalBox;
		
		return true;
	}

	BOOL FSceneSystem::Publish(FWorld* pWorld, const Event::OnComponentAssigned<FCamera>& crEvent)
	{
		return true;
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

	void LoadIndicesFromGLTFPrimitive(const tinygltf::Primitive* cpGLTFPrimitive, FMesh::SubMesh& rSubMesh)
	{
		const auto& GLTFIndicesAccessor = gpGLTFModel->accessors[cpGLTFPrimitive->indices];
		const auto& GLTFIndicesBufferView = gpGLTFModel->bufferViews[GLTFIndicesAccessor.bufferView];
		const auto& GLTFIndicesBuffer = gpGLTFModel->buffers[GLTFIndicesBufferView.buffer];

		rSubMesh.Indices.reserve(GLTFIndicesAccessor.count);

		auto AddIndices = [&]<typename T>()
		{
			const T* IndexData = reinterpret_cast<const T*>(GLTFIndicesBuffer.data.data() + GLTFIndicesBufferView.byteOffset + GLTFIndicesAccessor.byteOffset);
			for (UINT64 ix = 0; ix < GLTFIndicesAccessor.count; ix += 3)
			{
				// 默认为顺时针旋转
				rSubMesh.Indices.push_back(IndexData[ix + 0]);
				rSubMesh.Indices.push_back(IndexData[ix + 1]);
				rSubMesh.Indices.push_back(IndexData[ix + 2]);
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

	void LoadVerticesBoxFromGLTFPrimitive(const tinygltf::Primitive* cpGLTFPrimitive, FMesh::SubMesh& rSubMesh)
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

			meshopt_optimizeVertexCache(rSubMesh.Indices.data(), rSubMesh.Indices.data(), rSubMesh.Indices.size(), stVertexCount);
			meshopt_optimizeOverdraw(
				rSubMesh.Indices.data(),
				rSubMesh.Indices.data(),
				rSubMesh.Indices.size(),
				&PositionStream[0].x,
				stVertexCount,
				sizeof(FVector3F),
				1.05f
			);

			meshopt_optimizeVertexFetchRemap(&remap[0], rSubMesh.Indices.data(), rSubMesh.Indices.size(), stVertexCount);
			meshopt_remapIndexBuffer(rSubMesh.Indices.data(), rSubMesh.Indices.data(), rSubMesh.Indices.size(), &remap[0]);
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


		rSubMesh.Vertices.resize(stVertexCount);
		for (UINT32 ix = 0; ix < stVertexCount; ++ix)
		{
			if (bPos) rSubMesh.Vertices[ix].Position = PositionStream[ix];
			if (bNor) rSubMesh.Vertices[ix].Normal = NormalStream[ix];
			if (bTan) rSubMesh.Vertices[ix].Tangent = TangentStream[ix];
			if (bUV) rSubMesh.Vertices[ix].UV = UVStream[ix];
		}

		rSubMesh.Box = CreateAABB(PositionStream);
	}

}