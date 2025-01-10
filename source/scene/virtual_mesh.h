#ifndef SCENE_VIRTUAL_MESH_H
#define SCENE_VIRTUAL_MESH_H

#include <cstdint>

#include "../core/math/bounds.h"
#include "../core/Math/surface.h"
#include "../core/tools/hash_table.h"
#include "../core/tools/bit_allocator.h"
#include "geometry.h"
#include <utility>

namespace fantasy 
{
    class MeshOptimizer
    {
    public:
        MeshOptimizer(std::vector<float3>& vertices, std::vector<uint32_t>& indices);

        bool optimize(uint32_t target_triangle_num);
        void lock_position(const float3& position);

        float _max_error;

    private:
        bool compact();
        void fix_triangle(uint32_t triangle_index);

        float evaluate(const float3& p0, const float3& p1, bool bMerge);
        void merge_begin(const float3& p);
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
            std::vector<float> _keys;
            std::vector<uint32_t> _heap_indices;
        };

    private:
        std::vector<float3>& _vertices;
        std::vector<uint32_t>& _indices;
        uint32_t _remain_vertex_num;
        uint32_t _remain_triangle_num;

        enum
        {
            AdjacencyFlag   = 1,
            LockFlag        = 2
        };
        std::vector<uint8_t> _flags;
        std::vector<uint32_t> _vertex_ref_count;
        std::vector<QuadricSurface> _triangle_surfaces;

        BitSetAllocator _triangle_removed_array;

        HashTable _vertex_table;    // key: vertex_position; hash value: vertex_index.
        HashTable _index_table;     // key: vertex_position; hash value: index_index.


        std::vector<std::pair<float3, float3>> _edges;
        HashTable _edges_begin_table;   // key: vertex_position; hash value: edge_index.   
        HashTable _edges_end_table;     // key: vertex_position; hash value: edge_index.
        BinaryHeap _heap;


        std::vector<uint32_t> moved_vertex_indices;
        std::vector<uint32_t> moved_index_indices;
        std::vector<uint32_t> moved_edge_indices;
        std::vector<uint32_t> edge_need_reevaluate_indices;
    };

    struct MeshCluster
    {
        static const uint32_t cluster_size = 128;

        std::vector<float3> vertices;
        std::vector<uint32_t> indices;
        std::vector<uint32_t> external_edges;   // edge_index essentially corresponds to the vertex_index of the edge's starting point.

        Bounds3F bounding_box;
        Sphere bounding_sphere;
        Sphere lod_bounding_sphere;
        float lod_error = 0.0f;
        uint32_t mip_level = 0;
        uint32_t group_id = 0;
    };

    struct MeshClusterGroup
    {
        static const uint32_t group_size = 32;

        std::vector<uint32_t> cluster_indices;
        std::vector<std::pair<uint32_t,uint32_t>> external_edges;
        Sphere bounding_sphere;
        Sphere lod_bounding_sphere;
        float parent_lod_error = 0.0f;
        uint32_t mip_level = 0;
    };

    class VirtualMesh
    {
    public:
        bool build(const Mesh* mesh);

    private:
        struct VirtualSubmesh
        {
            std::vector<MeshCluster> clusters;
            std::vector<MeshClusterGroup> cluster_groups;
            uint32_t mip_level_num;
        };

        bool cluster_triangles(VirtualSubmesh& submesh);
        bool build_cluster_groups(VirtualSubmesh& submesh, uint32_t level_offset, uint32_t level_cluster_count, uint32_t mip_level);
        bool build_parent_clusters(VirtualSubmesh& submesh, uint32_t cluster_group_index);
        void build_adjacency_graph(
            const std::vector<float3>& vertices, 
            const std::vector<uint32_t>& indices, 
            SimpleGraph& edge_link_graph, 
            SimpleGraph& adjacency_graph
        );
        uint32_t edge_hash(const float3& p0, const float3& p1);

    private:
        std::vector<VirtualSubmesh> _submeshes;

		std::vector<uint32_t> _indices;
        std::vector<float3> _vertex_positons;
    };

}

#endif