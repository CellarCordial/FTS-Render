#ifndef SCENE_VIRTUAL_MESH_H
#define SCENE_VIRTUAL_MESH_H

#include <cstdint>

#include "../core/math/bounds.h"

#if NANITE
#include "../core/Math/surface.h"
#include "../core/tools/hash_table.h"
#include "../core/tools/bit_allocator.h"
#endif

#include "geometry.h"
#include <utility>
#include <vector>

namespace fantasy 
{

    struct MeshCluster
    {
        static const uint32_t cluster_tirangle_num = 128;

        uint32_t geometry_id = 0;
        std::vector<Vertex> vertices;
        
        std::vector<uint32_t> indices;
        std::vector<uint32_t> external_edges;
        
        uint32_t group_id = 0;
        uint32_t mip_level = 0;
        float lod_error = 0.0f;

        Sphere bounding_sphere;
        Sphere lod_bounding_sphere;
    };

    struct MeshClusterGroup
    {
        static const uint32_t group_size = 32;

        uint32_t mip_level = 0;
        uint32_t cluster_count = 0;
        std::vector<uint32_t> cluster_indices;
        std::vector<std::pair<uint32_t,uint32_t>> external_edges;

        Sphere bounding_sphere;
        float parent_lod_error = 0.0f;
    };

#if NANITE
    class MeshOptimizer
    {
    public:
        MeshOptimizer(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

        bool optimize(uint32_t target_triangle_num);
        void lock_position(const float3& position);

        float _max_error;

    private:
        bool compact();
        void fix_triangle(uint32_t triangle_index);

        float evaluate(const Vertex& p0, const Vertex& p1, bool bMerge);
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
        std::vector<Vertex>& _vertices;
        std::vector<uint32_t>& _indices;
        uint32_t _remain_vertex_count;
        uint32_t _remain_triangle_count;

        enum
        {
            AdjacencyFlag   = 1,
            LockFlag        = 2
        };
        std::vector<uint8_t> _flags;
        std::vector<uint32_t> _vertex_ref_count;
        std::vector<QuadricSurface> _triangle_surfaces;

        BitSetAllocator _triangle_removed_set;

        HashTable _vertex_table;    // key: vertex_position; hash value: vertex_index.
        HashTable _index_table;     // key: vertex_position; hash value: index_index.


        std::vector<std::pair<Vertex, Vertex>> _edges;
        HashTable _edges_begin_table;   // key: vertex_position; hash value: edge_index.   
        HashTable _edges_end_table;     // key: vertex_position; hash value: edge_index.
        BinaryHeap _heap;


        std::vector<uint32_t> removed_vertex_indices;
        std::vector<uint32_t> removed_index_indices;
        std::vector<uint32_t> removed_edge_indices;
        std::vector<uint32_t> edge_need_reevaluate_indices;
    };

    class VirtualMesh
    {
    public:
        bool build(const Mesh* mesh);
        
        struct VirtualSubmesh
        {
            uint32_t mip_levels;
            std::vector<MeshCluster> clusters;
            std::vector<MeshClusterGroup> cluster_groups;
        };

        std::vector<VirtualSubmesh> _submeshes;

    private:
        bool cluster_triangles(VirtualSubmesh& submesh);
        bool build_cluster_groups(VirtualSubmesh& submesh, uint32_t level_offset, uint32_t level_cluster_count, uint32_t mip_level);
        bool build_parent_clusters(VirtualSubmesh& submesh, uint32_t cluster_group_index);
        void build_adjacency_graph(
            const std::vector<Vertex>& vertices, 
            const std::vector<uint32_t>& indices, 
            SimpleGraph& edge_link_graph, 
            SimpleGraph& adjacency_graph
        );
        uint32_t edge_hash(const float3& p0, const float3& p1);

    private:
		std::vector<uint32_t> _indices;
        std::vector<Vertex> _vertices;
    };


#else
    class VirtualMesh
    {
    public:
        bool build(const Mesh* mesh);
        
        struct VirtualSubmesh
        {
            uint32_t mip_levels = 0;
            std::vector<MeshCluster> clusters;
            std::vector<MeshClusterGroup> cluster_groups;
        };

        std::vector<VirtualSubmesh> _submeshes;

    private:

        struct Triangle
        {
            uint32_t v1;
            uint32_t v2;
            uint32_t v3;
            bool merged = false;
        };

        struct Quad
        {
            Vertex v1;
            Vertex v2;
            Vertex v3;
            Vertex v4;

            float3 get_center() const { return (v1.position + v2.position + v3.position + v4.position) / 4; }
        };

        void cluster_quad(VirtualSubmesh& submesh);
        void convert_triangles_to_quads();

    private:
        std::vector<uint32_t> _indices;
        std::vector<Vertex> _vertices;
    };

#endif
    
    struct MeshClusterGpu
    {
        float4 bounding_sphere;
        float4 lod_bounding_sphere;

        uint32_t mip_level;
        uint32_t group_id;
        float lod_error;

        uint32_t vertex_offset;
        uint32_t triangle_offset;
        uint32_t vertex_index_count;

        uint32_t geometry_id;
    };

    struct MeshClusterGroupGpu
    {
        float4 bounding_sphere;

        uint32_t mip_level;
        uint32_t cluster_count;
        uint32_t cluster_index_offset;
        float max_parent_lod_error;

        uint32_t max_mip_level;
    };

    inline MeshClusterGpu convert_mesh_cluster(
        const MeshCluster& cluster,
        uint32_t vertex_offset,
        uint32_t triangle_offset
    )
    {
        MeshClusterGpu ret;
        ret.bounding_sphere = float4(
            cluster.bounding_sphere.center.x,
            cluster.bounding_sphere.center.y,
            cluster.bounding_sphere.center.z,
            cluster.bounding_sphere.radius
        );
        ret.lod_bounding_sphere = float4(
            cluster.lod_bounding_sphere.center.x,
            cluster.lod_bounding_sphere.center.y,
            cluster.lod_bounding_sphere.center.z,
            cluster.lod_bounding_sphere.radius
        );
        ret.mip_level = cluster.mip_level;
        ret.group_id = cluster.group_id;
        ret.lod_error = cluster.lod_error;
        ret.vertex_index_count = static_cast<uint32_t>(cluster.indices.size()) / 3;
        ret.vertex_offset = vertex_offset;
        ret.triangle_offset = triangle_offset;
        ret.geometry_id = cluster.geometry_id;
        return ret;
    }

    inline MeshClusterGroupGpu convert_mesh_cluster_group(
        const MeshClusterGroup& group,
        uint32_t cluster_index_offset,
        uint32_t max_mip_level
    )
    {
        MeshClusterGroupGpu ret;
        ret.bounding_sphere = float4(
            group.bounding_sphere.center.x,
            group.bounding_sphere.center.y,
            group.bounding_sphere.center.z,
            group.bounding_sphere.radius
        );

        ret.cluster_count = group.cluster_indices.empty() ? group.cluster_count : static_cast<uint32_t>(group.cluster_indices.size());
        ret.max_parent_lod_error = group.parent_lod_error;
        ret.cluster_index_offset = cluster_index_offset;
        ret.mip_level = group.mip_level;
        ret.max_mip_level = max_mip_level;
        return ret;
    }
}

#endif