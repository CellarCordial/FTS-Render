#ifndef SCENE_VIRTUAL_MESH_H
#define SCENE_VIRTUAL_MESH_H

#include <cstdint>

#include "../core/math/bounds.h"
#include "../core/Math/surface.h"
#include "../core/tools/hash_table.h"
#include "../core/tools/bit_allocator.h"
#include "geometry.h"
#include <utility>
#include <vector>

namespace fantasy 
{

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

#ifndef SIMPLE_VIRTUAL_MESH
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
        bool build(const Mesh* mesh)
        {		
            for (const auto& submesh : mesh->submeshes)
            {
                _indices.insert(_indices.end(), submesh.indices.begin(), submesh.indices.end());
                _vertices.insert(_vertices.end(), submesh.vertices.begin(), submesh.vertices.end());
                convert_triangles_to_quads();
    
                auto& virtual_submesh = _submeshes.emplace_back();
                cluster_quad(virtual_submesh);

                Sphere group_sphere;
                for (const auto& cluster : virtual_submesh.clusters) 
                {
                    group_sphere = merge(group_sphere, cluster.bounding_sphere);
                }
                virtual_submesh.cluster_groups.push_back(MeshClusterGroup{
                    .cluster_count = static_cast<uint32_t>(virtual_submesh.clusters.size()),
                    .bounding_sphere = group_sphere
                });
                
                _indices.clear();
                _vertices.clear();
            }
            _indices.shrink_to_fit();
            _vertices.shrink_to_fit();

            return true;          
        }
        
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

        void cluster_quad(VirtualSubmesh& submesh)
        {
            std::vector<Quad> quads(_indices.size() / 4);
            for (uint32_t ix = 0; ix < quads.size(); ++ix)
            {
                quads[ix] = Quad{
                    .v1 = _vertices[_indices[ix * 4 + 0]],
                    .v2 = _vertices[_indices[ix * 4 + 1]],
                    .v3 = _vertices[_indices[ix * 4 + 2]],
                    .v4 = _vertices[_indices[ix * 4 + 3]]
                };
            }

            // 64 个 quad 为一个 cluster.
            uint32_t quad_num_per_cluster = MeshCluster::cluster_tirangle_num / 2;
            uint32_t new_cluster_num = static_cast<uint32_t>(quads.size()) / quad_num_per_cluster;
            uint32_t supply_quad_num = quad_num_per_cluster - static_cast<uint32_t>(quads.size()) % quad_num_per_cluster;
            
            for (uint32_t ix = 0; ix < supply_quad_num; ++ix) quads.push_back(Quad());
            new_cluster_num += supply_quad_num == 0 ? 0 : 1;

            submesh.clusters.resize(new_cluster_num);
            for (uint32_t ix = 0; ix < new_cluster_num; ++ix)
            {
                float3 center = quads[0].get_center();
                
                std::sort(
                    quads.begin(), 
                    quads.end(),
                    [&center](const Quad& q0, const Quad& q1)
                    {
                        return distance(q0.get_center(), center) < distance(q1.get_center(), center);
                    }
                );

                auto& cluster = submesh.clusters[ix];

                auto& vertices = cluster.vertices;

                vertices.resize(quad_num_per_cluster * 4);
                for (uint32_t jx = 0; jx < quad_num_per_cluster; ++jx)
                {
                    vertices[jx * 4 + 0] = quads[jx].v1;
                    vertices[jx * 4 + 1] = quads[jx].v2;
                    vertices[jx * 4 + 2] = quads[jx].v3;
                    vertices[jx * 4 + 3] = quads[jx].v4;
                }
                
                quads.erase(quads.begin(), quads.begin() + quad_num_per_cluster);

                std::vector<float3> vertex_positons(vertices.size());
                for (uint32_t jx = 0; jx < vertices.size(); ++jx) vertex_positons[jx] = vertices[jx].position;

                cluster.bounding_sphere = Sphere(vertex_positons);
            }
        }

        void convert_triangles_to_quads()
        {
            std::vector<Triangle> triangles(_indices.size() / 3);
            std::vector<Vertex> quad_vertices;
            std::vector<uint32_t> quad_indices;

            for (uint32_t ix = 0; ix < triangles.size(); ++ix)
            {
                triangles[ix] = Triangle{
                    .v1 = _indices[ix * 3 + 0],
                    .v2 = _indices[ix * 3 + 1],
                    .v3 = _indices[ix * 3 + 2]
                };
            }

            for (uint32_t ix = 0; ix < triangles.size(); ++ix)
            {
                auto& src_tri = triangles[ix];
                if (src_tri.merged) continue;

                bool found = false;
                for (uint32_t jx = ix + 1; jx < triangles.size(); ++jx)
                {
                    auto& dst_tri = triangles[jx];
                    if (dst_tri.merged) continue;

                    std::vector<uint32_t> combine_quad_indices;
                    add_triangle_index(combine_quad_indices, src_tri.v1);
                    add_triangle_index(combine_quad_indices, src_tri.v2);
                    add_triangle_index(combine_quad_indices, src_tri.v3);
                    add_triangle_index(combine_quad_indices, dst_tri.v1);
                    add_triangle_index(combine_quad_indices, dst_tri.v2);
                    add_triangle_index(combine_quad_indices, dst_tri.v3);

                    // 并非相邻的三角形.
                    if (combine_quad_indices.size() != 8) continue;

                    src_tri.merged = true;
                    dst_tri.merged = true;

                    for (uint32_t kx = 0; kx < 4; ++kx)
                    {
                        quad_vertices.push_back(_vertices[combine_quad_indices[kx * 2]]);
                    }

                    uint32_t index_start = static_cast<uint32_t>(quad_indices.size());
                    assert(index_start % 4 == 0);

                    quad_indices.push_back(index_start);
                    if (combine_quad_indices[5] == 0) quad_indices.push_back(index_start + 3); // v1 v2是公用边
                    quad_indices.push_back(index_start + 1);
                    if (combine_quad_indices[1] == 0) quad_indices.push_back(index_start + 3); // v2 v3是公用边
                    quad_indices.push_back(index_start + 2);
                    if (combine_quad_indices[3] == 0) quad_indices.push_back(index_start + 3); // v1 v3是公用边

                    found = true; break;
                }

                if (!found)
                {
                    src_tri.merged = true;

                    uint32_t index_start = static_cast<uint32_t>(quad_indices.size());
                    assert(index_start % 4 == 0);

                    quad_vertices.push_back(_vertices[src_tri.v1]);
                    quad_vertices.push_back(_vertices[src_tri.v2]);
                    quad_vertices.push_back(_vertices[src_tri.v3]);
                    quad_vertices.push_back(Vertex());

                    quad_indices.push_back(index_start);
                    quad_indices.push_back(index_start + 1);
                    quad_indices.push_back(index_start + 2);
                    quad_indices.push_back(index_start);
                }
            }

            _vertices = quad_vertices;
            _indices = quad_indices;
        }

        void add_triangle_index(std::vector<uint32_t>& quad_indices, uint32_t index)
        {
            for (int ix = 0; ix < quad_indices.size(); ix += 2)
            {
                if (quad_indices[ix] == index) 
                {
                    quad_indices[ix + 1] = 1;
                    return;
                }
            }
            quad_indices.push_back(index);
            quad_indices.push_back(0);
        }

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