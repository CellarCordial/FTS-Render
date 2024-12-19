#include "scene.h"
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>
#include <json.hpp>
#include "image.h"
#include "camera.h"
#include "../core/parallel/parallel.h"
#include "../core/math/quaternion.h"
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

	static std::unique_ptr<tinygltf::TinyGLTF> gltf_loader;
	static std::unique_ptr<tinygltf::Model> gltf_model;

	Matrix4x4 get_matrix_from_gltf_node(const tinygltf::Node& gltf_node);
	void load_indices_from_gltf_primitive(const tinygltf::Primitive* gltf_primitive, Mesh::Submesh& submesh);
	void load_vertices_box_from_gltf_primitive(const tinygltf::Primitive* gltf_primitive, Mesh::Submesh& submesh);


	bool SceneSystem::initialize(World* world)
	{
		if (!gltf_loader) gltf_loader = std::make_unique<tinygltf::TinyGLTF>();

		_world = world;
		_world->subscribe<event::OnModelLoad>(this);
		_world->subscribe<event::OnModelTransform>(this);
		_world->subscribe<event::OnComponentAssigned<Mesh>>(this);
		_world->subscribe<event::OnComponentAssigned<Material>>(this);
		_world->subscribe<event::OnComponentAssigned<SurfaceCache>>(this);
		_world->subscribe<event::OnComponentAssigned<DistanceField>>(this);

		_global_entity = world->get_global_entity();

		_global_entity->assign<SceneGrid>();
		_global_entity->assign<event::GenerateSdf>();
		_global_entity->assign<event::UpdateGBuffer>();
		_global_entity->assign<event::UpdateGlobalSdf>();
		_global_entity->assign<event::GenerateSurfaceCache>();

		return true;
	}

	bool SceneSystem::destroy()
	{
		if (gltf_loader) gltf_loader.reset();

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
				ReturnIfFalse(_global_entity->get_component<event::UpdateGBuffer>()->broadcast());
				ReturnIfFalse(_global_entity->get_component<event::GenerateSdf>()->broadcast(model_entity));
				// ReturnIfFalse(_global_entity->get_component<event::GenerateSurfaceCache>()->Broadcast(pModelEntity));

				thread_id = INVALID_SIZE_64;
				model_entity = nullptr;
			}
		}

		return true;
	}


	bool SceneSystem::publish(World* world, const event::OnModelLoad& event)
	{
		ReturnIfFalse(gltf_loader != nullptr && event.entity != nullptr);


		gltf_model.reset();
		gltf_model = std::make_unique<tinygltf::Model>();

		std::string error;
		std::string warn;
		std::string proj_dir = PROJ_DIR;

		if (!gltf_loader->LoadASCIIFromFile(gltf_model.get(), &error, &warn, proj_dir + event.model_path))
		{
			LOG_ERROR("Failed to load model.");
			if (!error.empty() || !warn.empty())
			{
				LOG_ERROR(std::string(error + warn).c_str());
			}
			return false;
		}

		std::string model_name = remove_file_extension(event.model_path.c_str());

		gui::notify_message(gui::ENotifyType::Info, "Loaded " + event.model_path);

		_model_directory = event.model_path.substr(0, event.model_path.find_last_of('/') + 1);
		_sdf_data_path = proj_dir + "asset/sdf/" + model_name + ".sdf";
		_surface_cache_path = proj_dir + "asset/SurfaceCache/" + model_name + ".sc";

		event.entity->assign<std::string>(model_name);
		event.entity->assign<Mesh>();
		event.entity->assign<Material>();
		event.entity->assign<Transform>();
		event.entity->assign<SurfaceCache>();
		event.entity->assign<DistanceField>();

		gltf_model.reset();
		_sdf_data_path.clear();
		_model_directory.clear();

		_loaded_model_names.insert(event.model_path);

		
		Entity* tmp_model_entity = event.entity;
		gui::add(
			[tmp_model_entity, this]()
			{
				std::string model_name = *tmp_model_entity->get_component<std::string>();
				model_name += " Transform";
				if (ImGui::TreeNode(model_name.c_str()))
				{
					bool changed = false;

					Transform tmp_trans = *tmp_model_entity->get_component<Transform>();
					changed |= ImGui::SliderFloat3("position", reinterpret_cast<float*>(&tmp_trans.position), -32.0f, 32.0f);		
					changed |= ImGui::SliderFloat3("rotation", reinterpret_cast<float*>(&tmp_trans.rotation), -180.0f, 180.0f);
					changed |= ImGui::SliderFloat3("scale", reinterpret_cast<float*>(&tmp_trans.scale), 0.1f, 8.0f);
					if (changed) _world->broadcast(event::OnModelTransform{ .entity = tmp_model_entity, .transform = tmp_trans });

					ImGui::TreePop();
				}
			}
		);

		return true;
	}

	bool SceneSystem::publish(World* world, const event::OnComponentAssigned<Mesh>& event)
	{
		const auto& gltf_scene = gltf_model->scenes[gltf_model->defaultScene];

		std::vector<Matrix4x4> world_matrixs;
		std::vector<const tinygltf::Primitive*> primitives;

		std::function<void(const tinygltf::Node&, const Matrix4x4&)> func;
		func = [&](const tinygltf::Node& crGLTFNode, const Matrix4x4& crParentMatrix) -> void
			{
				Matrix4x4 world_matrix = mul(crParentMatrix, get_matrix_from_gltf_node(crGLTFNode));

				if (crGLTFNode.mesh >= 0)
				{
					const auto& mesh_primitives = gltf_model->meshes[crGLTFNode.mesh].primitives;
					for (const auto& mesh_primitive : mesh_primitives)
					{
						primitives.emplace_back(&mesh_primitive);
						world_matrixs.push_back(world_matrix);
					}
				}

				for (int child_node_index : crGLTFNode.children)
				{
					func(gltf_model->nodes[child_node_index], world_matrixs.back());
				}
			};

		for (uint32_t ix = 0; ix < gltf_scene.nodes.size(); ++ix)
		{
			func(gltf_model->nodes[gltf_scene.nodes[ix]], event.component->world_matrix);
		}

		Mesh* mesh = event.component;
		auto& submeshes = mesh->submeshes;
		submeshes.resize(primitives.size());

		parallel::parallel_for(
			[&](uint64_t ix)
			{
				auto& submesh = submeshes[ix];
				submesh.world_matrix = world_matrixs[ix];
				submesh.material_index = primitives[ix]->material;

				load_indices_from_gltf_primitive(primitives[ix], submesh);
				load_vertices_box_from_gltf_primitive(primitives[ix], submesh);
			},
			primitives.size()
		);
		return true;
	}

	bool SceneSystem::publish(World* world, const event::OnComponentAssigned<Material>& event)
	{
		std::string file_path = PROJ_DIR + _model_directory;
		
		auto& material = event.component;

		if (!gltf_model)
		{
			LOG_ERROR("You should load a model first.");
			return false;
		}

		material->submaterials.resize(gltf_model->materials.size());

		parallel::parallel_for(
			[&material, &file_path](uint64_t ix)
			{
				auto& submaterial = material->submaterials[ix];

				const auto& gltf_material = gltf_model->materials[ix];
				submaterial.diffuse_factor[0] = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor[0]);
				submaterial.diffuse_factor[1] = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor[1]);
				submaterial.diffuse_factor[2] = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor[2]);
				submaterial.diffuse_factor[3] = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor[3]);
				submaterial.roughness_factor = static_cast<float>(gltf_material.pbrMetallicRoughness.roughnessFactor);
				submaterial.metallic_factor = static_cast<float>(gltf_material.pbrMetallicRoughness.metallicFactor);
				submaterial.occlusion_factor = static_cast<float>(gltf_material.occlusionTexture.strength);
				submaterial.emissive_factor[0] = static_cast<float>(gltf_material.emissiveFactor[0]);
				submaterial.emissive_factor[1] = static_cast<float>(gltf_material.emissiveFactor[1]);
				submaterial.emissive_factor[2] = static_cast<float>(gltf_material.emissiveFactor[2]);


				if (gltf_material.pbrMetallicRoughness.baseColorTexture.index >= 0)
				{
					const auto& gltf_texture = gltf_model->textures[gltf_material.pbrMetallicRoughness.baseColorTexture.index];
					const auto& gltf_image = gltf_model->images[gltf_texture.source];

					submaterial.images[Material::TextureType_Diffuse] = Image::LoadImageFromFile((file_path + gltf_image.uri).c_str());
				}
				if (gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
				{
					const auto& gltf_texture = gltf_model->textures[gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index];
					const auto& gltf_image = gltf_model->images[gltf_texture.source];

					submaterial.images[Material::TextureType_MetallicRoughness] = Image::LoadImageFromFile((file_path + gltf_image.uri).c_str());
				}
				if (gltf_material.normalTexture.index >= 0)
				{
					const auto& gltf_texture = gltf_model->textures[gltf_material.normalTexture.index];
					const auto& gltf_image = gltf_model->images[gltf_texture.source];

					submaterial.images[Material::TextureType_Normal] = Image::LoadImageFromFile((file_path + gltf_image.uri).c_str());
				}
				if (gltf_material.occlusionTexture.index >= 0)
				{
					const auto& gltf_texture = gltf_model->textures[gltf_material.occlusionTexture.index];
					const auto& gltf_image = gltf_model->images[gltf_texture.source];

					submaterial.images[Material::TextureType_Occlusion] = Image::LoadImageFromFile((file_path + gltf_image.uri).c_str());
				}
				if (gltf_material.emissiveTexture.index >= 0)
				{
					const auto& gltf_texture = gltf_model->textures[gltf_material.emissiveTexture.index];
					const auto& gltf_image = gltf_model->images[gltf_texture.source];

					submaterial.images[Material::TextureType_Emissive] = Image::LoadImageFromFile((file_path + gltf_image.uri).c_str());
				}
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

		return _world->each<event::UpdateGlobalSdf>(
			[](Entity* entity, event::UpdateGlobalSdf* event) -> bool
			{
				return event->broadcast();
			}
		);
	}

	Matrix4x4 get_matrix_from_gltf_node(const tinygltf::Node& gltf_node)
	{
		struct WorldTransform
		{
			Vector3F translation_vector = { 0.0f, 0.0f, 0.0f };
			Vector3F scale_vector = { 1.0f, 1.0f, 1.0f };
			Quaternion rotation_vector;
			Matrix4x4 world_matrix;

			void create_world_matrix()
			{
				world_matrix = mul(mul(translate(translation_vector), scale(scale_vector)), rotation_vector.to_matrix());
			}
		};

		WorldTransform transform;

		if (!gltf_node.matrix.empty())
		{
			transform.world_matrix = {
					static_cast<float>(gltf_node.matrix[0]),
					static_cast<float>(gltf_node.matrix[1]),
					static_cast<float>(gltf_node.matrix[2]),
					static_cast<float>(gltf_node.matrix[3]),
					static_cast<float>(gltf_node.matrix[4]),
					static_cast<float>(gltf_node.matrix[5]),
					static_cast<float>(gltf_node.matrix[6]),
					static_cast<float>(gltf_node.matrix[7]),
					static_cast<float>(gltf_node.matrix[8]),
					static_cast<float>(gltf_node.matrix[9]),
					static_cast<float>(gltf_node.matrix[10]),
					static_cast<float>(gltf_node.matrix[11]),
					static_cast<float>(gltf_node.matrix[12]),
					static_cast<float>(gltf_node.matrix[13]),
					static_cast<float>(gltf_node.matrix[14]),
					static_cast<float>(gltf_node.matrix[15])
			};
		}
		else
		{
			if (!gltf_node.translation.empty()) transform.translation_vector = { (float)gltf_node.translation[0], (float)gltf_node.translation[1], (float)gltf_node.translation[2] };
			if (!gltf_node.scale.empty()) transform.scale_vector = { (float)gltf_node.scale[0], (float)gltf_node.scale[1], (float)gltf_node.scale[2] };
			if (!gltf_node.rotation.empty()) transform.rotation_vector = { (float)gltf_node.rotation[0], (float)gltf_node.rotation[1], (float)gltf_node.rotation[2], (float)gltf_node.rotation[3] };

			transform.create_world_matrix();
		}

		return transform.world_matrix;
	}

	void load_indices_from_gltf_primitive(const tinygltf::Primitive* gltf_primitive, Mesh::Submesh& submesh)
	{
		const auto& gltf_indices_accessor = gltf_model->accessors[gltf_primitive->indices];
		const auto& gltf_indices_buffer_view = gltf_model->bufferViews[gltf_indices_accessor.bufferView];
		const auto& gltf_indices_buffer = gltf_model->buffers[gltf_indices_buffer_view.buffer];

		submesh.indices.reserve(gltf_indices_accessor.count);

		auto AddIndices = [&]<typename T>()
		{
			const T* index_data = reinterpret_cast<const T*>(
				gltf_indices_buffer.data.data() + 
				gltf_indices_buffer_view.byteOffset + 
				gltf_indices_accessor.byteOffset
			);
			for (uint64_t ix = 0; ix < gltf_indices_accessor.count; ix += 3)
			{
				submesh.indices.push_back(index_data[ix + 0]);
				submesh.indices.push_back(index_data[ix + 1]);
				submesh.indices.push_back(index_data[ix + 2]);
			}
		};

		const uint32_t index_stride = gltf_indices_accessor.ByteStride(gltf_indices_buffer_view);
		switch (index_stride)
		{
		case 1: AddIndices.operator() < uint8_t > (); break;
		case 2: AddIndices.operator() < uint16_t > (); break;
		case 4: AddIndices.operator() < uint32_t > (); break;
		default:
			assert(!"Doesn't support such stride.");
		}
	}

	void load_vertices_box_from_gltf_primitive(const tinygltf::Primitive* gltf_primitive, Mesh::Submesh& rSubmesh)
	{
		uint32_t temp_counter = 0;
		uint64_t vertex_count = 0;
		uint32_t attribute_stride[4] = { 0 };
		auto LoadAttribute = [&](const std::string& attribute_name, bool& out_exist)
			{
				if (!out_exist) return size_t(0);
				const auto iterator = gltf_primitive->attributes.find(attribute_name);
				if (iterator == gltf_primitive->attributes.end())
				{
					out_exist = false;
					return size_t(0);
				}
				const auto& gltf_attribute_accessor = gltf_model->accessors[iterator->second];
				const auto& gltf_attribute_buffer_view = gltf_model->bufferViews[gltf_attribute_accessor.bufferView];
				const auto& gltf_attribute_buffer = gltf_model->buffers[gltf_attribute_buffer_view.buffer];

				vertex_count = gltf_attribute_accessor.count;
				attribute_stride[temp_counter++] = gltf_attribute_accessor.ByteStride(gltf_attribute_buffer_view);

				if (temp_counter >= 5)
				{
					LOG_ERROR("LoadGLTFNode() calling has a mistake which is Model's attribute is too more.");
					out_exist = false;
					return size_t(0);
				}
				out_exist = true;
				return reinterpret_cast<size_t>(
					gltf_attribute_buffer.data.data() + 
					gltf_attribute_buffer_view.byteOffset + 
					gltf_attribute_accessor.byteOffset
				);
			};


		bool position_exist = true, normal_exist = true, tangent_exist = true, uv_exist = true;
		size_t position_data = LoadAttribute("POSITION", position_exist);
		size_t normal_data = LoadAttribute("NORMAL", normal_exist);
		size_t tangent_data = LoadAttribute("TANGENT", tangent_exist);
		size_t uv_data = LoadAttribute("TEXCOORD_0", uv_exist);

		rSubmesh.vertices.resize(vertex_count);
		for (uint32_t ix = 0; ix < vertex_count; ++ix)
		{
			if (position_exist) rSubmesh.vertices[ix].position = *reinterpret_cast<Vector3F*>(position_data + ix * attribute_stride[0]);
			if (normal_exist) rSubmesh.vertices[ix].normal = *reinterpret_cast<Vector3F*>(normal_data + ix * attribute_stride[1]);
			if (tangent_exist) rSubmesh.vertices[ix].tangent = *reinterpret_cast<Vector4F*>(tangent_data + ix * attribute_stride[2]);
			if (uv_exist) rSubmesh.vertices[ix].uv = *reinterpret_cast<Vector2F*>(uv_data + ix * attribute_stride[3]);
		}
	}

}