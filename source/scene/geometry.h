#ifndef SCENE_GEOMETRY_H
#define SCENE_GEOMETRY_H


#include "../core/tools/ecs.h"
#include "../core/tools/log.h"
#include "../core/math/graph.h"
#include "../core/math/matrix.h"
#include "../core/tools/delegate.h"
#include "image.h"
#include <basetsd.h>
#include <cstdint>
#include <vector>


namespace fantasy 
{
	namespace event
	{
		struct OnModelLoad
		{
			Entity* entity = nullptr;
			std::string model_path;
		};

		DELCARE_MULTI_DELEGATE_EVENT(ModelLoaded);
	};



    struct Vertex
    {
        float3 position;
        float3 normal;
        float3 tangent;
        float2 uv;
    };

    struct Material
    {
        enum
        {
            TextureType_BaseColor,
            TextureType_Normal,
            TextureType_Metallic,
            TextureType_Roughness,
            TextureType_Emissive,
            TextureType_Occlusion,
            TextureType_Num
        };

        struct SubMaterial
        {
			float diffuse_factor[4] = { 0.0f };
			float roughness_factor = 0.0f;
			float metallic_factor = 0.0f;
			float occlusion_factor = 0.0f;
			float emissive_factor[4] = { 0.0f };

            Image images[TextureType_Num];

            bool operator==(const SubMaterial& other) const
            {
                ReturnIfFalse(
                    diffuse_factor[0] == other.diffuse_factor[0] &&
                    diffuse_factor[1] == other.diffuse_factor[1] &&
                    diffuse_factor[2] == other.diffuse_factor[2] &&
                    diffuse_factor[3] == other.diffuse_factor[3] &&
                    roughness_factor  == other.roughness_factor &&
                    metallic_factor   == other.metallic_factor &&
                    occlusion_factor  == other.occlusion_factor &&
                    emissive_factor[0]   == other.emissive_factor[0] &&
                    emissive_factor[1]   == other.emissive_factor[1] &&
                    emissive_factor[2]   == other.emissive_factor[2]
                );

                for (uint32_t ix = 0; ix < TextureType_Num; ++ix)
                {
                    ReturnIfFalse(images[ix] == other.images[ix]);
                }
                return true;            
            }

            bool operator!=(const SubMaterial& other) const
            {
                return !((*this) == other);
            }
        };

        std::vector<SubMaterial> submaterials;

        bool operator==(const Material& other) const
        {
            ReturnIfFalse(submaterials.size() == other.submaterials.size());
            for (uint64_t ix = 0; ix < submaterials.size(); ++ix)
            {
                ReturnIfFalse(submaterials[ix] == other.submaterials[ix]);
            }
            return true;
        }
    };

    struct Mesh
    {
        struct Submesh
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            
            float4x4 world_matrix;
            uint32_t material_index;
        };

        std::vector<Submesh> submeshes;
        float4x4 world_matrix;
        bool moved = false;
        bool culling = false;
    };


    namespace Geometry
    {
        Mesh create_box(float fWidth, float fHeight, float depth, uint32_t subdivision_count);
        Mesh create_sphere(float radius, uint32_t slice_count, uint32_t stack_count);
        Mesh create_geosphere(float radius, uint32_t numSubdivisions);
        Mesh create_cylinder(float bottom_radius, float top_radius, float height, uint32_t slice_count, uint32_t stack_count);
        Mesh create_grid(float width, float depth, uint32_t m, uint32_t n);
        Mesh create_quad(float x, float y, float w, float h, float depth);
        void subdivide(Mesh::Submesh& rMeshData);
        Vertex get_mid_point(const Vertex& v0, const Vertex& v1);
        void build_cylinder_top_cap(float bottom_radius, float top_radius, float height, uint32_t slice_count, uint32_t stack_count, Mesh::Submesh& mesh_data);
        void build_cylinder_bottom_cap(float bottom_radius, float top_radius, float height, uint32_t slice_count, uint32_t stack_count, Mesh::Submesh& mesh_data);
    };
    
}

















#endif