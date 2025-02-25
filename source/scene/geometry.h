#ifndef SCENE_GEOMETRY_H
#define SCENE_GEOMETRY_H


#include "../core/tools/ecs.h"
#include "../core/tools/log.h"
#include "../core/math/graph.h"
#include "../core/math/matrix.h"
#include "../core/math/bounds.h"
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

		DELCARE_MULTI_DELEGATE_EVENT(AddModel);
	};


    struct Vertex
    {
        float3 position;
        float3 normal;
        float3 tangent;
        float2 uv;


        bool operator==(const Vertex& other) const 
        {
            return position == other.position &&
                   normal == other.normal &&
                   tangent == other.tangent &&
                   uv == other.uv;
        }

        bool operator!=(const Vertex& other) const
        {
            return !((*this) == other);
        }
    };

    struct Material
    {
        enum
        {
            TextureType_BaseColor,
            TextureType_Normal,
            TextureType_PBR,    // Metallic, Roughness, Occlusion.
            TextureType_Emissive,
            TextureType_Num
        };

        struct SubMaterial
        {
			float base_color_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			float roughness_factor = 0.0f;
			float metallic_factor = 0.0f;
			float occlusion_factor = 0.0f;
			float emissive_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

            Image images[TextureType_Num];

            bool operator==(const SubMaterial& other) const
            {
                ReturnIfFalse(
                    base_color_factor[0] == other.base_color_factor[0] &&
                    base_color_factor[1] == other.base_color_factor[1] &&
                    base_color_factor[2] == other.base_color_factor[2] &&
                    base_color_factor[3] == other.base_color_factor[3] &&
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
        uint32_t image_resolution = 0;

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
            Sphere bounding_sphere;
        };

        std::vector<Submesh> submeshes;
        float4x4 world_matrix;
        bool moved = false;
        
        uint32_t submesh_global_base_id = 0;
    };

    struct GeometryConstantGpu
    {
        float4x4 world_matrix;
        float4x4 inv_trans_world;

        float4 base_color;
        float4 emissive;
        float roughness;
        float metallic;
        float occlusion;

        uint2 texture_resolution;
    };
    
	inline std::string get_geometry_texture_name(
        uint32_t submesh_index,
        uint32_t image_type, 
        const std::string& model_name
    )
	{
		std::string texture_name;
		switch (image_type) 
		{
		case Material::TextureType_BaseColor: 
			texture_name = model_name + "_base_color_" + std::to_string(submesh_index); break;
		case Material::TextureType_Normal:  
			texture_name = model_name + "_normal_" + std::to_string(submesh_index); break;
		case Material::TextureType_PBR:  
			texture_name = model_name + "_pbr_" + std::to_string(submesh_index); break;
		case Material::TextureType_Emissive:  
			texture_name = model_name + "_emissive_" + std::to_string(submesh_index); break;
		default: break;
		}
		return texture_name;
	}


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