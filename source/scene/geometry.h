#ifndef SCENE_GEOMETRY_H
#define SCENE_GEOMETRY_H


#include "../core/tools/log.h"
#include "../core/math/matrix.h"
#include "../core/math/bounds.h"
#include "../core/math/sphere.h"
#include "../core/Math/surface.h"
#include "../core/tools/hash_table.h"
#include "../core/tools/bit_allocator.h"
#include "image.h"
#include <basetsd.h>


namespace fantasy 
{
    struct Vertex
    {
        Vector3F position;
        Vector3F normal;
        Vector4F tangent;
        Vector2F uv;
    };

    struct Material
    {
        enum
        {
            TextureType_Diffuse,
            TextureType_Normal,
            TextureType_Emissive,
            TextureType_Occlusion,
            TextureType_MetallicRoughness,
            TextureType_Num
        };

        struct SubMaterial
        {
			float diffuse_factor[4] = { 0.0f };
			float roughness_factor = 0.0f;
			float metallic_factor = 0.0f;
			float occlusion_factor = 0.0f;
			float emissive_factor[3] = { 0.0f };

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
            
            Matrix4x4 world_matrix;
            uint32_t material_index;
        };

        std::vector<Submesh> submeshes;
        Matrix4x4 world_matrix;
        bool culling = false;
    };



    class MeshOptimizer
    {
    public:
        MeshOptimizer(const std::span<Vector3F>& vertices, const std::span<uint32_t>& indices);

        bool optimize(uint32_t target_triangle_num);
        void lock_position(Vector3F position);


        float _max_error;
        uint32_t _remain_vertex_num;
        uint32_t _remain_triangle_num;

    private:
        bool compact();
        static uint32_t hash(const Vector3F& v);
        void fix_triangle(uint32_t triangle_index);

        // 评估 p0 和 p1 合并后的误差
        float evaluate(const Vector3F& p0, const Vector3F& p1, bool bMerge);
        void merge_begin(const Vector3F& p);
        void mergen_end();

        class BinaryHeap
        {
        public:
            void resize(uint32_t _num_index);
            float get_key(uint32_t idx);
            bool empty();
            bool is_valid(uint32_t idx);
            uint32_t top();
            void pop();
            void insert(float key, uint32_t index);
            void remove(uint32_t index);

        private:
            void push_down(uint32_t index);
            void push_up(uint32_t index);
        
        private:
            uint32_t _current_size;
            uint32_t _index_count;
            std::vector<uint32_t> _heap;
            std::vector<uint32_t> _keys;
            std::vector<uint32_t> _heap_indices;
        };

    private:
        enum
        {
            AdjacencyFlag   = 1,
            LockFlag        = 2
        };
        std::vector<uint8_t> _flags;

        const std::span<Vector3F> _vertices;
        const std::span<uint32_t> _indices;
        std::vector<uint32_t> _vertex_ref_count;
        std::vector<QuadricSurface> _triangle_surfaces;

        BitSetAllocator _triangle_removed_array;

        HashTable _vertex_table;
        HashTable _index_table;


        std::vector<std::pair<Vector3F, Vector3F>> _edges;
        HashTable _edges_begin_table;
        HashTable _edges_end_table;
        BinaryHeap _heap;


        std::vector<uint32_t> moved_vertex_indices;
        std::vector<uint32_t> moved_index_indices;
        std::vector<uint32_t> moved_edge_indices;
        std::vector<uint32_t> edge_need_reevaluate_indices;
    };


    struct Cluster
    {
        static const uint32_t cluster_size=128;

        std::vector<Vector3F> verts;
        std::vector<uint32_t> indexes;
        std::vector<uint32_t> external_edges;

        Bounds3F box_bounds;
        Sphere3F sphere_bounds;
        Sphere3F lod_bounds;
        float lod_error;
        uint32_t mip_level;
        uint32_t group_id;
    };

    struct ClusterGroup
    {
        static const uint32_t group_size=32;

        Sphere3F bounds;
        Sphere3F lod_bounds;
        float min_lod_error;
        float max_parent_lod_error;
        uint32_t mip_level;
        std::vector<uint32_t> clusters;
        std::vector<std::pair<uint32_t,uint32_t>> external_edges;
    };


    void create_triangle_cluster(
        const std::vector<Vector3F>& verts,
        const std::vector<uint32_t>& indexes,
        std::vector<Cluster>& clusters
    );

    void build_cluster_groups(
        std::vector<Cluster>& clusters,
        uint32_t offset,
        uint32_t num_cluster,
        std::vector<ClusterGroup>& cluster_groups,
        uint32_t mip_level
    );

    void build_cluster_group_parent_clusters(
        ClusterGroup& cluster_group,
        std::vector<Cluster>& clusters
    );

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