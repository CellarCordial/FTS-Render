#include "geometry.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "scene.h"
#include "assimp/mesh.h"
#include "assimp/types.h"
#include <assimp/scene.h>
#include "assimp/material.h"
#include "../core/parallel/parallel.h"


namespace fantasy
{
	bool SceneSystem::publish(World* world, const event::OnComponentAssigned<Mesh>& event)
	{
		if (!assimp_scene)
		{
			LOG_ERROR("You should load a model first.");
			return false;
		}

		std::vector<float4x4> world_matrixs;
		std::vector<const aiMesh*> assimp_meshes;

		std::function<void(aiNode *node, const aiScene *scene, const float4x4&)> func;
		func = [&](aiNode* node, const aiScene* scene, const float4x4& parent_matrix) -> void
		{
			const auto& m = node->mTransformation;
			float4x4 world_matrix = mul(
				parent_matrix, 
				float4x4(
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

		mesh->submesh_base_id = _current_submesh_count;
		_current_submesh_count += static_cast<uint32_t>(submeshes.size());

		parallel::parallel_for(
			[&](uint64_t ix)
			{
				auto& submesh = submeshes[ix];
				submesh.world_matrix = world_matrixs[ix];
				submesh.material_index = assimp_meshes[ix]->mMaterialIndex;

				for(uint32_t jx = 0; jx < assimp_meshes[ix]->mNumFaces; jx++)
				{
					aiFace face = assimp_meshes[ix]->mFaces[jx];
					for(uint32_t kx = 0; kx < face.mNumIndices; kx += 3)
					{
						submesh.indices.push_back(face.mIndices[kx + 2]);
						submesh.indices.push_back(face.mIndices[kx + 1]);
						submesh.indices.push_back(face.mIndices[kx + 0]);
					}
				}

				std::vector<float3> positons(assimp_meshes[ix]->mNumVertices);
				submesh.vertices.resize(assimp_meshes[ix]->mNumVertices);

				for(uint32_t jx = 0; jx < assimp_meshes[ix]->mNumVertices; jx++)
				{
					Vertex& vertex = submesh.vertices[jx];
					
					vertex.position.x = assimp_meshes[ix]->mVertices[jx].x;
					vertex.position.y = assimp_meshes[ix]->mVertices[jx].y;
					vertex.position.z = assimp_meshes[ix]->mVertices[jx].z;

					positons[jx] = vertex.position;

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

				submesh.bounding_sphere = Sphere(positons);
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
			[&material, &file_path, this](uint64_t ix)
			{
				auto& submaterial = material->submaterials[ix];
				aiMaterial* assimp_material = assimp_scene->mMaterials[ix];

				aiColor4D ai_color;
				if (assimp_material->Get(AI_MATKEY_BASE_COLOR, ai_color) == AI_SUCCESS) 
					memcpy(submaterial.base_color_factor, &ai_color, sizeof(float) * 4);
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
					submaterial.images[Material::TextureType_PBR] = Image::load_image_from_file((file_path + material_name.C_Str()).c_str());
				if (assimp_material->GetTexture(aiTextureType_EMISSION_COLOR, 0, &material_name) == aiReturn_SUCCESS)
					submaterial.images[Material::TextureType_Emissive] = Image::load_image_from_file((file_path + material_name.C_Str()).c_str());
			},
			material->submaterials.size()
		);

		uint2 resolution = uint2(
			material->submaterials[0].images[0].width,
			material->submaterials[0].images[0].height
		);
		ReturnIfFalse(resolution.x == resolution.y);

		for (const auto& submaterial : material->submaterials)
		{
			for (const auto& image : submaterial.images)
			{
				if (resolution != uint2(image.width, image.height))
				{
					LOG_ERROR("All Geometry texture resolution must be the same.");
					return false;
				}
			}
		}

		material->image_resolution = resolution.x;
		if (!is_power_of_2(material->image_resolution))
		{
			LOG_ERROR("Geometry texture resolution must be power of 2.");
			return false;
		}
		
		return true;
	}


	namespace Geometry
	{
		Mesh create_box(float width, float height, float depth, uint32_t numSubdivisions)
		{
			Mesh Mesh;
			auto& rSubmesh = Mesh.submeshes.emplace_back();

			Vertex v[24];

			float w2 = 0.5f * width;
			float h2 = 0.5f * height;
			float d2 = 0.5f * depth;

			v[0] = Vertex({ -w2, -h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });
			v[1] = Vertex({ -w2, +h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
			v[2] = Vertex({ +w2, +h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f });
			v[3] = Vertex({ +w2, -h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f });
			v[4] = Vertex({ -w2, -h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f });
			v[5] = Vertex({ +w2, -h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });
			v[6] = Vertex({ +w2, +h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
			v[7] = Vertex({ -w2, +h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f });
			v[8] = Vertex({ -w2, +h2, -d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });
			v[9] = Vertex({ -w2, +h2, +d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
			v[10] = Vertex({ +w2, +h2, +d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f });
			v[11] = Vertex({ +w2, +h2, -d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f });
			v[12] = Vertex({ -w2, -h2, -d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f });
			v[13] = Vertex({ +w2, -h2, -d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });
			v[14] = Vertex({ +w2, -h2, +d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
			v[15] = Vertex({ -w2, -h2, +d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f });
			v[16] = Vertex({ -w2, -h2, +d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f });
			v[17] = Vertex({ -w2, +h2, +d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f });
			v[18] = Vertex({ -w2, +h2, -d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f });
			v[19] = Vertex({ -w2, -h2, -d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f });
			v[20] = Vertex({ +w2, -h2, -d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[21] = Vertex({ +w2, +h2, -d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[22] = Vertex({ +w2, +h2, +d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[23] = Vertex({ +w2, -h2, +d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });

			rSubmesh.vertices.assign(&v[0], &v[24]);

			uint16_t i[36];

			// Fill in the front face index data
			i[0] = 0; i[1] = 1; i[2] = 2;
			i[3] = 0; i[4] = 2; i[5] = 3;

			// Fill in the back face index data
			i[6] = 4; i[7] = 5; i[8] = 6;
			i[9] = 4; i[10] = 6; i[11] = 7;

			// Fill in the top face index data
			i[12] = 8; i[13] = 9; i[14] = 10;
			i[15] = 8; i[16] = 10; i[17] = 11;

			// Fill in the bottom face index data
			i[18] = 12; i[19] = 13; i[20] = 14;
			i[21] = 12; i[22] = 14; i[23] = 15;

			// Fill in the left face index data
			i[24] = 16; i[25] = 17; i[26] = 18;
			i[27] = 16; i[28] = 18; i[29] = 19;

			// Fill in the right face index data
			i[30] = 20; i[31] = 21; i[32] = 22;
			i[33] = 20; i[34] = 22; i[35] = 23;

			rSubmesh.indices.assign(&i[0], &i[36]);

			// Put a cap on the number of subdivisions.
			numSubdivisions = std::min<uint32_t>(numSubdivisions, 6u);

			for (uint32_t i = 0; i < numSubdivisions; ++i) subdivide(rSubmesh);

			return Mesh;
		}

		void subdivide(Mesh::Submesh& mesh_data)
		{
			Mesh::Submesh inputCopy = mesh_data;


			mesh_data.vertices.clear();
			mesh_data.indices.clear();

			//       v1
			//       *
			//      / \
            //     /   \
            //  m0*-----*m1
			//   / \   / \
            //  /   \ /   \
            // *-----*-----*
			// v0    m2     v2

			uint32_t numTris = (uint32_t)inputCopy.indices.size() / 3;
			for (uint32_t i = 0; i < numTris; ++i)
			{
				Vertex v0 = inputCopy.vertices[inputCopy.indices[i * 3 + 0]];
				Vertex v1 = inputCopy.vertices[inputCopy.indices[i * 3 + 1]];
				Vertex v2 = inputCopy.vertices[inputCopy.indices[i * 3 + 2]];

				//
				// Generate the midpoints.
				//

				Vertex m0 = get_mid_point(v0, v1);
				Vertex m1 = get_mid_point(v1, v2);
				Vertex m2 = get_mid_point(v0, v2);

				//
				// add new geometry.
				//

				mesh_data.vertices.push_back(v0); // 0
				mesh_data.vertices.push_back(v1); // 1
				mesh_data.vertices.push_back(v2); // 2
				mesh_data.vertices.push_back(m0); // 3
				mesh_data.vertices.push_back(m1); // 4
				mesh_data.vertices.push_back(m2); // 5

				mesh_data.indices.push_back(i * 6 + 0);
				mesh_data.indices.push_back(i * 6 + 3);
				mesh_data.indices.push_back(i * 6 + 5);

				mesh_data.indices.push_back(i * 6 + 3);
				mesh_data.indices.push_back(i * 6 + 4);
				mesh_data.indices.push_back(i * 6 + 5);

				mesh_data.indices.push_back(i * 6 + 5);
				mesh_data.indices.push_back(i * 6 + 4);
				mesh_data.indices.push_back(i * 6 + 2);

				mesh_data.indices.push_back(i * 6 + 3);
				mesh_data.indices.push_back(i * 6 + 1);
				mesh_data.indices.push_back(i * 6 + 4);
			}
		}

		Vertex get_mid_point(const Vertex& v0, const Vertex& v1)
		{
			float3 pos = 0.5f * (v0.position + v1.position);
			float3 normal = normalize(0.5f * (v0.normal + v1.normal));
			float3 tangent = normalize(0.5f * (v0.tangent + v1.tangent));
			float2 tex = 0.5f * (v0.uv + v1.uv);

			Vertex v;
			v.position = pos;
			v.normal = normal;
			v.tangent = tangent;
			v.uv = tex;

			return v;
		}


		Mesh create_sphere(float radius, uint32_t slice_count, uint32_t stack_count)
		{
			Mesh mesh;
			auto& submesh = mesh.submeshes.emplace_back();
			//
			// Compute the vertices stating at the top pole and moving down the stacks.
			//

			// Poles: note that there will be texture coordinate distortion as there is
			// not a unique point on the texture map to assign to the pole when mapping
			// a rectangular texture onto a sphere.
			Vertex topVertex({ 0.0f, +radius, 0.0f }, { 0.0f, +1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
			Vertex bottomVertex({ 0.0f, -radius, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });

			submesh.vertices.push_back(topVertex);

			float phiStep = PI / stack_count;
			float thetaStep = 2.0f * PI / slice_count;

			// Compute vertices for each stack ring (do not count the poles as rings).
			for (uint32_t i = 1; i <= stack_count - 1; ++i)
			{
				float phi = i * phiStep;

				// vertices of ring.
				for (uint32_t j = 0; j <= slice_count; ++j)
				{
					float theta = j * thetaStep;

					Vertex v;

					// spherical to cartesian
					v.position.x = radius * sinf(phi) * cosf(theta);
					v.position.y = radius * cosf(phi);
					v.position.z = radius * sinf(phi) * sinf(theta);

					// Partial derivative of P with respect to theta
					v.tangent.x = -radius * sinf(phi) * sinf(theta);
					v.tangent.y = 0.0f;
					v.tangent.z = +radius * sinf(phi) * cosf(theta);

					v.tangent = normalize(v.tangent);
					v.normal = normalize(v.normal);

					v.uv.x = theta / 2.0f * PI;
					v.uv.y = phi / PI;

					submesh.vertices.push_back(v);
				}
			}

			submesh.vertices.push_back(bottomVertex);

			//
			// Compute indices for top stack.  The top stack was written first to the Vertex buffer
			// and connects the top pole to the first ring.
			//

			for (uint32_t i = 1; i <= slice_count; ++i)
			{
				submesh.indices.push_back(0);
				submesh.indices.push_back(i + 1);
				submesh.indices.push_back(i);
			}

			//
			// Compute indices for inner stacks (not connected to poles).
			//

			// offset the indices to the index of the first Vertex in the first ring.
			// This is just skipping the top pole Vertex.
			uint32_t baseIndex = 1;
			uint32_t ringFVerUVount = slice_count + 1;
			for (uint32_t i = 0; i < stack_count - 2; ++i)
			{
				for (uint32_t j = 0; j < slice_count; ++j)
				{
					submesh.indices.push_back(baseIndex + i * ringFVerUVount + j);
					submesh.indices.push_back(baseIndex + i * ringFVerUVount + j + 1);
					submesh.indices.push_back(baseIndex + (i + 1) * ringFVerUVount + j);

					submesh.indices.push_back(baseIndex + (i + 1) * ringFVerUVount + j);
					submesh.indices.push_back(baseIndex + i * ringFVerUVount + j + 1);
					submesh.indices.push_back(baseIndex + (i + 1) * ringFVerUVount + j + 1);
				}
			}

			//
			// Compute indices for bottom stack.  The bottom stack was written last to the Vertex buffer
			// and connects the bottom pole to the bottom ring.
			//

			// South pole Vertex was added last.
			uint32_t southPoleIndex = (uint32_t)submesh.vertices.size() - 1;

			// offset the indices to the index of the first Vertex in the last ring.
			baseIndex = southPoleIndex - ringFVerUVount;

			for (uint32_t i = 0; i < slice_count; ++i)
			{
				submesh.indices.push_back(southPoleIndex);
				submesh.indices.push_back(baseIndex + i);
				submesh.indices.push_back(baseIndex + i + 1);
			}

			return mesh;
		}


		Mesh create_geosphere(float radius, uint32_t subdivision_count)
		{
			Mesh mesh;
			auto& submesh = mesh.submeshes.emplace_back();

			// Put a cap on the number of subdivisions.
			subdivision_count = std::min<uint32_t>(subdivision_count, 6u);

			// Approximate a sphere by tessellating an icosahedron.

			const float X = 0.525731f;
			const float Z = 0.850651f;

			float3 pos[12] =
			{
				float3(-X, 0.0f, Z),  float3(X, 0.0f, Z),
				float3(-X, 0.0f, -Z), float3(X, 0.0f, -Z),
				float3(0.0f, Z, X),   float3(0.0f, Z, -X),
				float3(0.0f, -Z, X),  float3(0.0f, -Z, -X),
				float3(Z, X, 0.0f),   float3(-Z, X, 0.0f),
				float3(Z, -X, 0.0f),  float3(-Z, -X, 0.0f)
			};

			uint32_t k[60] =
			{
				1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,
				1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,
				3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0,
				10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7
			};

			submesh.vertices.resize(12);
			submesh.indices.assign(&k[0], &k[60]);

			for (uint32_t i = 0; i < 12; ++i)
				submesh.vertices[i].position = pos[i];

			for (uint32_t i = 0; i < subdivision_count; ++i)
				subdivide(submesh);

			// Project vertices onto sphere and scale.
			for (uint32_t i = 0; i < submesh.vertices.size(); ++i)
			{
				// Project onto unit sphere.
				submesh.vertices[i].position = normalize(submesh.vertices[i].position);

				// Project onto sphere.
				submesh.vertices[i].normal = radius * submesh.vertices[i].position;


				// Derive texture coordinates from spherical coordinates.
				float theta = atan2f(submesh.vertices[i].position.z, submesh.vertices[i].position.x);

				// Put in [0, 2pi].
				if (theta < 0.0f)
					theta += 2.0f * PI;

				float phi = acosf(submesh.vertices[i].position.y / radius);

				submesh.vertices[i].uv.x = theta / 2.0f * PI;
				submesh.vertices[i].uv.y = phi / PI;

				// Partial derivative of P with respect to theta
				submesh.vertices[i].tangent.x = -radius * sinf(phi) * sinf(theta);
				submesh.vertices[i].tangent.y = 0.0f;
				submesh.vertices[i].tangent.z = +radius * sinf(phi) * cosf(theta);

				submesh.vertices[i].tangent = normalize(submesh.vertices[i].tangent);
			}

			return mesh;
		}

		Mesh create_cylinder(float bottom_radius, float top_radius, float height, uint32_t slice_count, uint32_t stack_count)
		{
			Mesh mesh;
			auto& submesh = mesh.submeshes.emplace_back();

			//
			// build Stacks.
			// 

			float stack_height = height / stack_count;

			// Amount to increment radius as we move up each stack level from bottom to top.
			float radius_step = (top_radius - bottom_radius) / stack_count;

			uint32_t ring_count = stack_count + 1;

			// Compute vertices for each stack ring starting at the bottom and moving up.
			for (uint32_t i = 0; i < ring_count; ++i)
			{
				float y = -0.5f * height + i * stack_height;
				float r = bottom_radius + i * radius_step;

				// vertices of ring
				float dTheta = 2.0f * PI / slice_count;
				for (uint32_t j = 0; j <= slice_count; ++j)
				{
					Vertex vertex;

					float c = cosf(j * dTheta);
					float s = sinf(j * dTheta);

					vertex.position = float3(r * c, y, r * s);

					vertex.uv.x = (float)j / slice_count;
					vertex.uv.y = 1.0f - (float)i / stack_count;

					// Cylinder can be parameterized as follows, where we introduce v
					// parameter that goes in the same direction as the v tex-coord
					// so that the bitangent goes in the same direction as the v tex-coord.
					//   Let r0 be the bottom radius and let r1 be the top radius.
					//   y(v) = h - hv for v in [0,1].
					//   r(v) = r1 + (r0-r1)v
					//
					//   x(t, v) = r(v)*cos(t)
					//   y(t, v) = h - hv
					//   z(t, v) = r(v)*sin(t)
					// 
					//  dx/dt = -r(v)*sin(t)
					//  dy/dt = 0
					//  dz/dt = +r(v)*cos(t)
					//
					//  dx/dv = (r0-r1)*cos(t)
					//  dy/dv = -h
					//  dz/dv = (r0-r1)*sin(t)

					// This is unit length.
					vertex.tangent = float3(-s, 0.0f, c);

					float dr = bottom_radius - top_radius;
					float4 bitangent(dr * c, -height, dr * s, 1.0f);

					vertex.normal = normalize(cross(float3(vertex.tangent), float3(bitangent)));
					submesh.vertices.push_back(vertex);
				}
			}

			// add one because we duplicate the first and last vertex per ring
			// since the texture coordinates are different.
			uint32_t ringVerUVount = slice_count + 1;

			// Compute indices for each stack.
			for (uint32_t i = 0; i < stack_count; ++i)
			{
				for (uint32_t j = 0; j < slice_count; ++j)
				{
					submesh.indices.push_back(i * ringVerUVount + j);
					submesh.indices.push_back((i + 1) * ringVerUVount + j);
					submesh.indices.push_back((i + 1) * ringVerUVount + j + 1);

					submesh.indices.push_back(i * ringVerUVount + j);
					submesh.indices.push_back((i + 1) * ringVerUVount + j + 1);
					submesh.indices.push_back(i * ringVerUVount + j + 1);
				}
			}

			build_cylinder_top_cap(bottom_radius, top_radius, height, slice_count, stack_count, submesh);
			build_cylinder_bottom_cap(bottom_radius, top_radius, height, slice_count, stack_count, submesh);

			return mesh;
		}

		void build_cylinder_top_cap(float bottom_radius, float top_radius, float height, uint32_t slice_count, uint32_t stack_count, Mesh::Submesh& mesh_data)
		{
			uint32_t baseIndex = (uint32_t)mesh_data.vertices.size();

			float y = 0.5f * height;
			float theta = 2.0f * PI / slice_count;

			// Duplicate cap ring vertices because the texture coordinates and normals differ.
			for (uint32_t i = 0; i <= slice_count; ++i)
			{
				float x = top_radius * cosf(i * theta);
				float z = top_radius * sinf(i * theta);

				// scale down by the height to try and make top cap texture coord area
				// proportional to base.
				float u = x / height + 0.5f;
				float v = z / height + 0.5f;

				mesh_data.vertices.push_back(Vertex({ x, y, z }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { u, v }));
			}

			// Cap center vertex.
			mesh_data.vertices.push_back(Vertex({ 0.0f, y, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.5f, 0.5f }));

			// Index of center vertex.
			uint32_t centerIndex = (uint32_t)mesh_data.vertices.size() - 1;

			for (uint32_t i = 0; i < slice_count; ++i)
			{
				mesh_data.indices.push_back(centerIndex);
				mesh_data.indices.push_back(baseIndex + i + 1);
				mesh_data.indices.push_back(baseIndex + i);
			}
		}

		void build_cylinder_bottom_cap(float bottom_radius, float top_radius, float height, uint32_t slice_count, uint32_t stack_count, Mesh::Submesh& mesh_data)
		{
			// 
			// build bottom cap.
			//

			uint32_t base_index = (uint32_t)mesh_data.vertices.size();
			float y = -0.5f * height;

			// vertices of ring
			float dTheta = 2.0f * PI / slice_count;
			for (uint32_t i = 0; i <= slice_count; ++i)
			{
				float x = bottom_radius * cosf(i * dTheta);
				float z = bottom_radius * sinf(i * dTheta);

				// scale down by the height to try and make top cap texture coord area
				// proportional to base.
				float u = x / height + 0.5f;
				float v = z / height + 0.5f;

				mesh_data.vertices.push_back(Vertex({ x, y, z }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { u, v }));
			}

			// Cap center vertex.
			mesh_data.vertices.push_back(Vertex({ 0.0f, y, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.5f, 0.5f }));

			// Cache the index of center vertex.
			uint32_t centerIndex = (uint32_t)mesh_data.vertices.size() - 1;

			for (uint32_t i = 0; i < slice_count; ++i)
			{
				mesh_data.indices.push_back(centerIndex);
				mesh_data.indices.push_back(base_index + i);
				mesh_data.indices.push_back(base_index + i + 1);
			}
		}

		Mesh create_grid(float width, float depth, uint32_t m, uint32_t n)
		{
			Mesh mesh;
			auto& submesh = mesh.submeshes.emplace_back();

			uint32_t verUVount = m * n;
			uint32_t faceCount = (m - 1) * (n - 1) * 2;

			//
			// Create the vertices.
			//

			float halfWidth = 0.5f * width;
			float halfDepth = 0.5f * depth;

			float dx = width / (n - 1);
			float dz = depth / (m - 1);

			float du = 1.0f / (n - 1);
			float dv = 1.0f / (m - 1);

			submesh.vertices.resize(verUVount);
			for (uint32_t i = 0; i < m; ++i)
			{
				float z = halfDepth - i * dz;
				for (uint32_t j = 0; j < n; ++j)
				{
					float x = -halfWidth + j * dx;

					submesh.vertices[i * n + j].position = float3(x, 0.0f, z);
					submesh.vertices[i * n + j].normal = float3(0.0f, 1.0f, 0.0f);
					submesh.vertices[i * n + j].tangent = float3(1.0f, 0.0f, 0.0f);

					// Stretch texture over grid.
					submesh.vertices[i * n + j].uv.x = j * du;
					submesh.vertices[i * n + j].uv.y = i * dv;
				}
			}

			//
			// Create the indices.
			//

			submesh.indices.resize(faceCount * 3); // 3 indices per face

			// Iterate over each quad and compute indices.
			uint32_t k = 0;
			for (uint32_t i = 0; i < m - 1; ++i)
			{
				for (uint32_t j = 0; j < n - 1; ++j)
				{
					submesh.indices[k] = i * n + j;
					submesh.indices[k + 1] = i * n + j + 1;
					submesh.indices[k + 2] = (i + 1) * n + j;

					submesh.indices[k + 3] = (i + 1) * n + j;
					submesh.indices[k + 4] = i * n + j + 1;
					submesh.indices[k + 5] = (i + 1) * n + j + 1;

					k += 6; // next quad
				}
			}

			return mesh;
		}

		Mesh create_quad(float x, float y, float w, float h, float depth)
		{
			Mesh mesh;
			auto& submesh = mesh.submeshes.emplace_back();


			submesh.vertices.resize(4);
			submesh.indices.resize(6);

			// position coordinates specified in NDC space.
			submesh.vertices[0] = Vertex(
				{ x, y - h, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f },
				{ 0.0f, 1.0f }
			);

			submesh.vertices[1] = Vertex(
				{ x, y, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f }
			);

			submesh.vertices[2] = Vertex(
				{ x + w, y, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f },
				{ 1.0f, 0.0f }
			);

			submesh.vertices[3] = Vertex(
				{ x + w, y - h, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f },
				{ 1.0f, 1.0f }
			);

			submesh.indices[0] = 0;
			submesh.indices[1] = 1;
			submesh.indices[2] = 2;

			submesh.indices[3] = 0;
			submesh.indices[4] = 2;
			submesh.indices[5] = 3;

			return mesh;
		}

	}


}
