#include "scene.h"
#include "assimp/material.h"
#include "assimp/mesh.h"
#include "assimp/types.h"
#include "geometry.h"
#include <cstdint>
#include <cstring>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "camera.h"
#include "../core/parallel/parallel.h"
#include "../core/tools/file.h"
#include "../gui/gui_panel.h"


namespace fantasy
{
	DistanceField::TransformData DistanceField::MeshDistanceField::get_transformed(const Transform* transform) const
	{
		TransformData ret;
		ret.sdf_box = sdf_box;

		Matrix4x4 S = scale(transform->scale);
		ret.sdf_box._lower = Vector3F(mul(Vector4F(ret.sdf_box._lower, 1.0f), S));
		ret.sdf_box._upper = Vector3F(mul(Vector4F(ret.sdf_box._upper, 1.0f), S));

		Matrix4x4 R = rotate(transform->rotation);
		std::array<Vector3F, 8> box_vertices;
		box_vertices[0] = ret.sdf_box._lower;
		box_vertices[1] = Vector3F(ret.sdf_box._lower.x, ret.sdf_box._upper.y, ret.sdf_box._lower.z);
		box_vertices[2] = Vector3F(ret.sdf_box._upper.x, ret.sdf_box._upper.y, ret.sdf_box._lower.z);
		box_vertices[3] = Vector3F(ret.sdf_box._upper.x, ret.sdf_box._lower.y, ret.sdf_box._lower.z);
		box_vertices[4] = ret.sdf_box._upper;
		box_vertices[7] = Vector3F(ret.sdf_box._upper.x, ret.sdf_box._lower.y, ret.sdf_box._upper.z);
		box_vertices[5] = Vector3F(ret.sdf_box._lower.x, ret.sdf_box._lower.y, ret.sdf_box._upper.z);
		box_vertices[6] = Vector3F(ret.sdf_box._lower.x, ret.sdf_box._upper.y, ret.sdf_box._upper.z);

		Bounds3F blank_box(0.0f, 0.0f);
		for (const auto& crVertex : box_vertices)
		{
			blank_box = merge(blank_box, Vector3F(mul(Vector4F(crVertex, 1.0f), R)));
		}

		ret.sdf_box = blank_box;

		Matrix4x4 T = translate(transform->position);
		ret.sdf_box._lower = Vector3F(mul(Vector4F(ret.sdf_box._lower, 1.0f), T));
		ret.sdf_box._upper = Vector3F(mul(Vector4F(ret.sdf_box._upper, 1.0f), T));

		Vector3F sdf_extent = sdf_box._upper - sdf_box._lower;
		ret.coord_matrix = mul(
			inverse(mul(mul(S, R), T)),		// Local Matrix.
			Matrix4x4(
				1.0f / sdf_extent.x, 0.0f, 0.0f, 0.0f,
				0.0f, -1.0f / sdf_extent.y, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f / sdf_extent.z, 0.0f,
				-sdf_box._lower.x / sdf_extent.x,
				sdf_box._upper.y / sdf_extent.y,
				-sdf_box._lower.z / sdf_extent.z,
				1.0f
			)
		);
		return ret;
	}

	SceneGrid::SceneGrid()
	{
		uint32_t chunk_num_per_axis = GLOBAL_SDF_RESOLUTION / VOXEL_NUM_PER_CHUNK;
		float voxel_size = SCENE_GRID_SIZE / GLOBAL_SDF_RESOLUTION;
		float chunk_size = VOXEL_NUM_PER_CHUNK * voxel_size;

		chunks.resize(chunk_num_per_axis * chunk_num_per_axis * chunk_num_per_axis);
		std::vector<Bounds3F> boxes(chunk_num_per_axis * chunk_num_per_axis * chunk_num_per_axis);
		for (uint32_t z = 0; z < chunk_num_per_axis; ++z)
			for (uint32_t y = 0; y < chunk_num_per_axis; ++y)
				for (uint32_t x = 0; x < chunk_num_per_axis; ++x)
				{
					Vector3F Lower = {
						-SCENE_GRID_SIZE * 0.5f + x * chunk_size,
						-SCENE_GRID_SIZE * 0.5f + y * chunk_size,
						-SCENE_GRID_SIZE * 0.5f + z * chunk_size
					};
					boxes[x + y * chunk_num_per_axis + z * chunk_num_per_axis * chunk_num_per_axis] = Bounds3F(Lower, Lower + chunk_size);
				}
		Bounds3F global_box(Vector3F(-SCENE_GRID_SIZE * 0.5f), Vector3F(SCENE_GRID_SIZE * 0.5f));
		bvh.build(boxes, global_box);
	}

	static const aiScene* assimp_scene = nullptr;

	bool SceneSystem::initialize(World* world)
	{
		_world = world;
		_world->subscribe<event::OnModelLoad>(this);
		_world->subscribe<event::OnModelTransform>(this);
		_world->subscribe<event::OnComponentAssigned<Mesh>>(this);
		_world->subscribe<event::OnComponentAssigned<Material>>(this);
		_world->subscribe<event::OnComponentAssigned<SurfaceCache>>(this);
		_world->subscribe<event::OnComponentAssigned<DistanceField>>(this);
		_world->subscribe<event::OnComponentAssigned<VirtualGeometry>>(this);

		_global_entity = world->get_global_entity();

		_global_entity->assign<SceneGrid>();
		_global_entity->assign<event::GenerateSdf>();
		_global_entity->assign<event::ModelLoaded>();
		_global_entity->assign<event::UpdateGlobalSdf>();
		_global_entity->assign<event::GenerateSurfaceCache>();

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
					model_entity = _world->create_entity();
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
				// ReturnIfFalse(_global_entity->get_component<event::ModelLoaded>()->broadcast());
				// ReturnIfFalse(_global_entity->get_component<event::GenerateSdf>()->broadcast(model_entity));
				// ReturnIfFalse(_global_entity->get_component<event::GenerateSurfaceCache>()->Broadcast(pModelEntity));

				thread_id = INVALID_SIZE_64;
				model_entity = nullptr;
			}
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
			aiProcess_CalcTangentSpace
		);

        if(!assimp_scene || assimp_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !assimp_scene->mRootNode)
        {
			LOG_ERROR("Failed to load model.");
			LOG_ERROR(assimp_importer.GetErrorString());
			return false;
        }

		std::string model_name = remove_file_extension(event.model_path.c_str());

		_model_directory = event.model_path.substr(0, event.model_path.find_last_of('/') + 1);
		_sdf_data_path = proj_dir + "asset/sdf/" + model_name + ".sdf";
		_surface_cache_path = proj_dir + "asset/SurfaceCache/" + model_name + ".sc";

		event.entity->assign<std::string>(model_name);
		event.entity->assign<Mesh>();
		event.entity->assign<Material>();
		event.entity->assign<Transform>();
		event.entity->assign<SurfaceCache>();
		// event.entity->assign<DistanceField>();
		event.entity->assign<VirtualGeometry>();
		// TODO: reverse the order of virtual geometry and distance field.

		_sdf_data_path.clear();
		_model_directory.clear();

		_loaded_model_names.insert(event.model_path);

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

	bool SceneSystem::publish(World* world, const event::OnComponentAssigned<Mesh>& event)
	{
		if (!assimp_scene)
		{
			LOG_ERROR("You should load a model first.");
			return false;
		}

		std::vector<Matrix4x4> world_matrixs;
		std::vector<const aiMesh*> assimp_meshes;

		std::function<void(aiNode *node, const aiScene *scene, const Matrix4x4&)> func;
		func = [&](aiNode* node, const aiScene* scene, const Matrix4x4& parent_matrix) -> void
		{
			const auto& m = node->mTransformation;
			Matrix4x4 world_matrix = mul(
				parent_matrix, 
				Matrix4x4(
					m.a1, m.a2, m.a3, m.a4, 
					m.b1, m.b2, m.b3, m.b4, 
					m.c1, m.c2, m.c3, m.c4, 
					m.d1, m.d2, m.d3, m.d4
				)
			);

			for (uint32_t ix = 0; ix < node->mNumMeshes; ++ix)
			{
				assimp_meshes.emplace_back(assimp_scene->mMeshes[node->mMeshes[ix]]);
				world_matrixs.push_back(world_matrix);
			}

			for(uint32_t ix = 0; ix < node->mNumChildren; ix++)
			{
				func(node->mChildren[ix], scene, world_matrix);
			}
		};

		func(assimp_scene->mRootNode, assimp_scene, event.component->world_matrix);


		Mesh* mesh = event.component;
		auto& submeshes = mesh->submeshes;
		submeshes.resize(assimp_meshes.size());

		parallel::parallel_for(
			[&](uint64_t ix)
			{
				auto& submesh = submeshes[ix];
				submesh.world_matrix = world_matrixs[ix];
				submesh.material_index = assimp_meshes[ix]->mMaterialIndex;

				for(uint32_t jx = 0; jx < assimp_meshes[ix]->mNumFaces; jx++)
				{
					aiFace face = assimp_meshes[ix]->mFaces[jx];
					for(uint32_t kx = 0; kx < face.mNumIndices; kx++)
					{
						submesh.indices.push_back(face.mIndices[kx]);
					}
				}

				for(uint32_t jx = 0; jx < assimp_meshes[ix]->mNumVertices; jx++)
				{
					Vertex& vertex = submesh.vertices.emplace_back();
					
					vertex.position.x = assimp_meshes[ix]->mVertices[jx].x;
					vertex.position.y = assimp_meshes[ix]->mVertices[jx].y;
					vertex.position.z = assimp_meshes[ix]->mVertices[jx].z;

					if (assimp_meshes[ix]->HasNormals())
					{
						vertex.normal.x = assimp_meshes[ix]->mNormals[jx].x;
						vertex.normal.y = assimp_meshes[ix]->mNormals[jx].y;
						vertex.normal.z = assimp_meshes[ix]->mNormals[jx].z;
					}

					if (assimp_meshes[ix]->HasTangentsAndBitangents())
					{
						vertex.tangent.x = assimp_meshes[ix]->mTangents[jx].x;
						vertex.tangent.y = assimp_meshes[ix]->mTangents[jx].y;
						vertex.tangent.z = assimp_meshes[ix]->mTangents[jx].z;
					}

					if(assimp_meshes[ix]->HasTextureCoords(0))
					{
						vertex.uv.x = assimp_meshes[ix]->mTextureCoords[0][jx].x; 
						vertex.uv.y = assimp_meshes[ix]->mTextureCoords[0][jx].y;
					}
				}
			},
			submeshes.size()
		);

		return true;
	}

	bool SceneSystem::publish(World* world, const event::OnComponentAssigned<Material>& event)
	{
		std::string file_path = PROJ_DIR + _model_directory;
		
		auto& material = event.component;

		if (!assimp_scene)
		{
			LOG_ERROR("You should load a model first.");
			return false;
		}

		material->submaterials.resize(assimp_scene->mNumMaterials);
		
		parallel::parallel_for(
			[&material, &file_path](uint64_t ix)
			{
				auto& submaterial = material->submaterials[ix];
				aiMaterial* assimp_material = assimp_scene->mMaterials[ix];

				aiColor4D ai_color;
				if (assimp_material->Get(AI_MATKEY_BASE_COLOR, ai_color) == AI_SUCCESS) 
					memcpy(submaterial.diffuse_factor, &ai_color, sizeof(float) * 4);
				if (assimp_material->Get(AI_MATKEY_COLOR_EMISSIVE, ai_color) == AI_SUCCESS) 
					memcpy(submaterial.emissive_factor, &ai_color, sizeof(float) * 4);
				
				float ai_float;
				if (assimp_material->Get(AI_MATKEY_METALLIC_FACTOR, ai_float) == AI_SUCCESS) 
					submaterial.metallic_factor = ai_float;
				if (assimp_material->Get(AI_MATKEY_ROUGHNESS_FACTOR, ai_float) == AI_SUCCESS) 
					submaterial.roughness_factor = ai_float;

				aiString material_name;
				if (assimp_material->GetTexture(aiTextureType_BASE_COLOR, 0, &material_name) == aiReturn_SUCCESS)
					submaterial.images[Material::TextureType_BaseColor] = Image::load_image_from_file((file_path + material_name.C_Str()).c_str());
				if (assimp_material->GetTexture(aiTextureType_NORMAL_CAMERA, 0, &material_name) == aiReturn_SUCCESS)
					submaterial.images[Material::TextureType_BaseColor] = Image::load_image_from_file((file_path + material_name.C_Str()).c_str());
				if (assimp_material->GetTexture(aiTextureType_METALNESS, 0, &material_name) == aiReturn_SUCCESS)
					submaterial.images[Material::TextureType_Metallic] = Image::load_image_from_file((file_path + material_name.C_Str()).c_str());
				if (assimp_material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &material_name) == aiReturn_SUCCESS)
					submaterial.images[Material::TextureType_Roughness] = Image::load_image_from_file((file_path + material_name.C_Str()).c_str());
				if (assimp_material->GetTexture(aiTextureType_EMISSION_COLOR, 0, &material_name) == aiReturn_SUCCESS)
					submaterial.images[Material::TextureType_Emissive] = Image::load_image_from_file((file_path + material_name.C_Str()).c_str());
				if (assimp_material->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &material_name) == aiReturn_SUCCESS)
					submaterial.images[Material::TextureType_Occlusion] = Image::load_image_from_file((file_path + material_name.C_Str()).c_str());
			},
			material->submaterials.size()
		);
		return true;
	}

	bool SceneSystem::publish(World* world, const event::OnComponentAssigned<SurfaceCache>& event)
	{
		SurfaceCache* surface_cache = event.component;
		Mesh* mesh = event.entity->get_component<Mesh>();
		surface_cache->mesh_surface_caches.resize(mesh->submeshes.size());

		bool load_from_file = false;
		if (is_file_exist(_surface_cache_path.c_str()))
		{
			serialization::BinaryInput input(_surface_cache_path);
			uint32_t card_resolution = 0;
			uint32_t surface_resolution = 0;
			input(card_resolution);
			input(surface_resolution);

			if (card_resolution == CARD_RESOLUTION && surface_resolution == SURFACE_RESOLUTION)
			{
				FormatInfo FormaInfo = get_format_info(surface_cache->format);
				uint64_t data_size = static_cast<uint64_t>(SURFACE_RESOLUTION) * SURFACE_RESOLUTION * FormaInfo.byte_size_per_pixel;

				for (uint32_t ix = 0; ix < surface_cache->mesh_surface_caches.size(); ++ix)
				{
					auto& mesh_surface_cache = surface_cache->mesh_surface_caches[ix];
					for (uint32_t ix = 0; ix < SurfaceCache::MeshSurfaceCache::SurfaceType::Count; ++ix)
					{
						mesh_surface_cache.surfaces[ix].surface_texture_name = 
							*event.entity->get_component<std::string>() + "SurfaceTexture" + std::to_string(ix);
						mesh_surface_cache.surfaces[ix].data.resize(data_size);
						input.load_binary_data(mesh_surface_cache.surfaces[ix].data.data(), data_size);
					}
				}
				gui::notify_message(gui::ENotifyType::Info, "Loaded " + _sdf_data_path.substr(_sdf_data_path.find("asset")));
				load_from_file = true;
			}
		}

		if (!load_from_file)
		{
			std::string model_name = *event.entity->get_component<std::string>();
			for (uint32_t ix = 0; ix < surface_cache->mesh_surface_caches.size(); ++ix)
			{
				std::string strMeshIndex = std::to_string(ix);
				auto& mesh_surface_cache = surface_cache->mesh_surface_caches[ix];
				mesh_surface_cache.surfaces[0].surface_texture_name = model_name + "SurfaceColorTexture" + strMeshIndex;
				mesh_surface_cache.surfaces[1].surface_texture_name = model_name + "SurfaceNormalTexture" + strMeshIndex;
				mesh_surface_cache.surfaces[2].surface_texture_name = model_name + "SurfacePBRTexture" + strMeshIndex;
				mesh_surface_cache.surfaces[3].surface_texture_name = model_name + "SurfaceEmissveTexture" + strMeshIndex;
				mesh_surface_cache.LightCache = model_name + "SurfaceLightTexture" + strMeshIndex;
			}
		}

		return true;
	}

	bool SceneSystem::publish(World* world, const event::OnComponentAssigned<DistanceField>& event)
	{
		DistanceField* distance_field = event.component;
		Mesh* mesh = event.entity->get_component<Mesh>();
		distance_field->mesh_distance_fields.resize(mesh->submeshes.size());

		bool load_from_file = false;
		if (is_file_exist(_sdf_data_path.c_str()))
		{
			serialization::BinaryInput input(_sdf_data_path);
			uint32_t mesh_sdf_resolution = 0;
			input(mesh_sdf_resolution);
			
			if (mesh_sdf_resolution == SDF_RESOLUTION)
			{
				for (uint32_t ix = 0; ix < distance_field->mesh_distance_fields.size(); ++ix)
				{
					auto& mesh_df = distance_field->mesh_distance_fields[ix];
					mesh_df.sdf_texture_name = *event.entity->get_component<std::string>() + "SdfTexture" + std::to_string(ix);

					input(
						mesh_df.sdf_box._lower.x,
						mesh_df.sdf_box._lower.y,
						mesh_df.sdf_box._lower.z,
						mesh_df.sdf_box._upper.x,
						mesh_df.sdf_box._upper.y,
						mesh_df.sdf_box._upper.z
					);

					uint64_t data_size = static_cast<uint64_t>(SDF_RESOLUTION) * SDF_RESOLUTION * SDF_RESOLUTION * sizeof(float);
					mesh_df.sdf_data.resize(data_size);
					input.load_binary_data(mesh_df.sdf_data.data(), data_size);
				}
				gui::notify_message(gui::ENotifyType::Info, "Loaded " + _sdf_data_path.substr(_sdf_data_path.find("asset")));
				load_from_file = true;
			}
		}

		if (!load_from_file)
		{
			std::string model_name = *event.entity->get_component<std::string>();
			for (uint32_t ix = 0; ix < distance_field->mesh_distance_fields.size(); ++ix)
			{
				auto& mesh_df = distance_field->mesh_distance_fields[ix];
				const auto& submesh = mesh->submeshes[ix];
				mesh_df.sdf_texture_name = model_name + "SdfTexture" + std::to_string(ix);

				uint64_t jx = 0;
				std::vector<Bvh::Vertex> BvhVertices(submesh.indices.size());
				for (auto VertexIndex : submesh.indices)
				{
					BvhVertices[jx++] = {
						Vector3F(mul(Vector4F(submesh.vertices[VertexIndex].position, 1.0f), submesh.world_matrix)),
						Vector3F(mul(Vector4F(submesh.vertices[VertexIndex].normal, 1.0f), transpose(inverse(submesh.world_matrix))))
					};
				}

				mesh_df.bvh.build(BvhVertices, static_cast<uint32_t>(submesh.indices.size() / 3));
				mesh_df.sdf_box = mesh_df.bvh.global_box;
			}
		}

		SceneGrid* grid = _global_entity->get_component<SceneGrid>();

		for (const auto& mesh_df : distance_field->mesh_distance_fields)
		{
			DistanceField::TransformData data = mesh_df.get_transformed(event.entity->get_component<Transform>());
			uint32_t chunk_num_per_axis = GLOBAL_SDF_RESOLUTION / VOXEL_NUM_PER_CHUNK;
			float voxel_size = SCENE_GRID_SIZE / GLOBAL_SDF_RESOLUTION;
			float chunk_size = VOXEL_NUM_PER_CHUNK * voxel_size;
			float grid_size = chunk_size * chunk_num_per_axis;

			Vector3I uniform_lower = Vector3I((data.sdf_box._lower + grid_size / 2.0f) / chunk_size);
			Vector3I uniform_upper = Vector3I((data.sdf_box._upper + grid_size / 2.0f) / chunk_size);

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
						grid->chunks[index].model_entities.insert(event.entity);
					}
				}
			}
		}
		
		return true;
	}

	bool SceneSystem::publish(World* world, const event::OnComponentAssigned<VirtualGeometry>& event)
	{
		VirtualGeometry* virtual_geometry = event.component;
		Mesh* mesh = event.entity->get_component<Mesh>();

		return virtual_geometry->build(mesh);
	}


	bool SceneSystem::publish(World* world, const event::OnModelTransform& event)
	{
		Transform* transform = event.entity->get_component<Transform>();
		DistanceField* distance_field = event.entity->get_component<DistanceField>();

		uint32_t chunk_num_per_axis = GLOBAL_SDF_RESOLUTION / VOXEL_NUM_PER_CHUNK;
		float voxel_size = SCENE_GRID_SIZE / GLOBAL_SDF_RESOLUTION;
		float chunk_size = 1.0f * VOXEL_NUM_PER_CHUNK * voxel_size;

		SceneGrid* grid = _global_entity->get_component<SceneGrid>();

		auto func_mark = [&](const Bounds3F& box, bool insert_or_erase)
		{
			Vector3I uniform_lower = Vector3I((box._lower + SCENE_GRID_SIZE / 2.0f) / chunk_size);
			Vector3I uniform_upper = Vector3I((box._upper + SCENE_GRID_SIZE / 2.0f) / chunk_size);

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
		Matrix4x4 S = scale(transform->scale);
		Matrix4x4 R = rotate(transform->rotation);
		Matrix4x4 T = translate(transform->position);
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