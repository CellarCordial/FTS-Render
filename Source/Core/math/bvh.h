#ifndef MATH_BVH_H
#define MATH_BVH_H

#include "bounds.h"
#include "vector.h"
#include <span>
#include <vector>
#include <bvh/bvh.hpp>
#include <bvh/leaf_collapser.hpp>
#include <bvh/locally_ordered_clustering_builder.hpp>
#include <bvh/triangle.hpp>

namespace fantasy 
{
    class Bvh
    {
    public:
        struct Node
        {
            Bounds3F box;
            uint32_t child_index = 0;
            uint32_t child_num = 0;
        };
        static_assert(sizeof(Node) == sizeof(float) * 8);

        struct Vertex
        {
            Vector3F position;
            Vector3F normal;
        };

		void build(std::span<Bounds3F> boxes, const Bounds3F& global_box);
		void build(std::span<Bvh::Vertex> vertices, uint32_t triangle_num);
        std::span<const Bvh::Node> GetNodes() const { return _nodes; }
        std::span<const Bvh::Vertex> GetVertices() const { return _vertices; }

        bool empty() const
        {
            return _nodes.empty() && _vertices.empty() && triangle_num == 0;
        }

        void clear() 
        { 
            _nodes.clear(); _nodes.shrink_to_fit();
            _vertices.clear(); _vertices.shrink_to_fit();
            triangle_num = 0;
        }

        Bounds3F global_box;
        uint32_t triangle_num = 0;

    private:
        std::vector<Node> _nodes;
        std::vector<Vertex> _vertices;
    };


    struct FTrianglePrimitive
    {
        Vector3F a, b, c;
        Bounds3F box;

        FTrianglePrimitive(Vector3F A, Vector3F B, Vector3F C) :
            a(A), b(B), c(C)
        {
            box = create_aabb({ a, b, c });
        }

        Bounds3F WorldBound() const { return box; }

        bool intersect(const Ray& ray) const
        {
            Vector3F ac = c - a;
            Vector3F ab = b - a;

            Vector3F S = ray.ori - a;
            Vector3F S1 = cross(ray.dir, ac);
            Vector3F S2 = cross(S, ab);
            float fInvDenom = 1.0f / dot(S1, ab);
            
            float t = dot(S2, ac) * fInvDenom;
            float b1 = dot(S1, S) * fInvDenom;
            float b2 = dot(S2, ray.dir) * fInvDenom;

            if (t < 0 || t > ray.max || b1 < 0 || b2 < 0 || b1 + b2 > 1.0f) return false;
            
            return true;
        }

    };



    struct LinearBvhNode;
    struct BvhBuildNode;
    struct BvhPrimitiveInfo;
    struct FMortonPrimitive;

    class BvhAccel
    {
    public:
        enum class ESplitMethod
        {
            SAH,
            HLBVH,
            Middle,
            EqualCounts
        };

        BvhAccel(
            std::vector<std::shared_ptr<FTrianglePrimitive>> pPrimitives, 
            uint32_t dwMaxPrimitivesInNode = 1, 
            ESplitMethod SplitMethod = ESplitMethod::SAH
        );

        Bounds3F WorldBound() const;

        bool intersect(const Ray& ray) const;

    private:
        BvhBuildNode* RecursiveBuild(
            std::vector<BvhPrimitiveInfo>& rPrimitiveInfos,
            uint32_t dwStart,
            uint32_t dwEnd,
            uint32_t* pdwTotoalNodes,
            std::vector<std::shared_ptr<FTrianglePrimitive>>& rpOrderedPrimitives
        );

        BvhBuildNode* HLBVHBuild(
            std::vector<BvhPrimitiveInfo>& rPrimitiveInfos,
            uint32_t* pdwTotalNodes,
            std::vector<std::shared_ptr<FTrianglePrimitive>>& rpOrderedPrimitives
        ) const;

        BvhBuildNode* EmitLBVH(
            BvhBuildNode*& rpBuildNodes,
            const std::vector<BvhPrimitiveInfo>& crPrimitiveInfos,
            FMortonPrimitive* pMortonPrimitives,
            uint32_t dwPrimitivesNum,
            uint32_t* pdwTotalNodes,
            std::vector<std::shared_ptr<FTrianglePrimitive>>& rpOrderedPrimitives,
            std::atomic<uint32_t>* pdwAtomicOrderedPrimsOffset,
            uint32_t dwSplitBitIndex
        ) const;

        BvhBuildNode* BuildUpperSAH(
            std::vector<BvhBuildNode*>& rpTreeletRoots,
            uint32_t dwStart,
            uint32_t dwEnd,
            uint32_t* pdwTotalNodes
        ) const;

        uint32_t FlattenBvhTree(BvhBuildNode* pNode, uint32_t* pdwOffset);

    private:
        const uint32_t m_dwMaxPrimitivesInNode;
        const ESplitMethod cSplidMethod;
        std::vector<std::shared_ptr<FTrianglePrimitive>> m_pPrimitives;
        std::vector<LinearBvhNode> _nodes;
    };

}

#endif