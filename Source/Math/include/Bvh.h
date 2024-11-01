#ifndef MATH_BVH_H
#define MATH_BVH_H

#include "../../Core/include/SysCall.h"
#include "Bounds.h"
#include "Vector.h"
#include <span>
#include <vector>
#include <bvh/bvh.hpp>
#include <bvh/leaf_collapser.hpp>
#include <bvh/locally_ordered_clustering_builder.hpp>
#include <bvh/triangle.hpp>

namespace FTS 
{
    class FBvh
    {
    public:
        struct Node
        {
            FBounds3F Box;
            UINT32 dwPrimitiveIndex;
            UINT32 dwPrimitiveNum;
        };
        static_assert(sizeof(Node) == sizeof(float) * 8);

        struct Vertex
        {
            FVector3F Position;
            FVector3F Normal;
        };

        BOOL Build(std::span<FBvh::Vertex> Vertices, UINT32 dwTriangleNum);
        std::span<const FBvh::Node> GetNodes() const { return m_Nodes; }
        std::span<const FBvh::Vertex> GetVertices() const { return m_Vertices; }

    private:
        std::vector<Node> m_Nodes;
        std::vector<Vertex> m_Vertices;

        UINT32 m_dwTriangleNum;
    };


        struct FTrianglePrimitive
    {
        FVector3F a, b, c;
        FBounds3F Box;

        FTrianglePrimitive(FVector3F A, FVector3F B, FVector3F C) :
            a(A), b(B), c(C)
        {
            Box = CreateAABB({ a, b, c });
        }

        FBounds3F WorldBound() const { return Box; }

        BOOL Intersect(const FRay& crRay) const
        {
            FVector3F ac = c - a;
            FVector3F ab = b - a;

            FVector3F S = crRay.m_Ori - a;
            FVector3F S1 = Cross(crRay.m_Dir, ac);
            FVector3F S2 = Cross(S, ab);
            float fInvDenom = 1.0f / Dot(S1, ab);
            
            float t = Dot(S2, ac) * fInvDenom;
            float b1 = Dot(S1, S) * fInvDenom;
            float b2 = Dot(S2, crRay.m_Dir) * fInvDenom;

            if (t < 0 || t > crRay.m_fMax || b1 < 0 || b2 < 0 || b1 + b2 > 1.0f) return false;
            
            return true;
        }

    };



    struct FLinearBvhNode;
    struct FBvhBuildNode;
    struct FBvhPrimitiveInfo;
    struct FMortonPrimitive;

    class FBvhAccel
    {
    public:
        enum class ESplitMethod
        {
            SAH,
            HLBVH,
            Middle,
            EqualCounts
        };

        FBvhAccel(
            std::vector<std::shared_ptr<FTrianglePrimitive>> pPrimitives, 
            UINT32 dwMaxPrimitivesInNode = 1, 
            ESplitMethod SplitMethod = ESplitMethod::SAH
        );

        FBounds3F WorldBound() const;

        BOOL Intersect(const FRay& crRay) const;

    private:
        FBvhBuildNode* RecursiveBuild(
            std::vector<FBvhPrimitiveInfo>& rPrimitiveInfos,
            UINT32 dwStart,
            UINT32 dwEnd,
            UINT32* pdwTotoalNodes,
            std::vector<std::shared_ptr<FTrianglePrimitive>>& rpOrderedPrimitives
        );

        FBvhBuildNode* HLBVHBuild(
            std::vector<FBvhPrimitiveInfo>& rPrimitiveInfos,
            UINT32* pdwTotalNodes,
            std::vector<std::shared_ptr<FTrianglePrimitive>>& rpOrderedPrimitives
        ) const;

        FBvhBuildNode* EmitLBVH(
            FBvhBuildNode*& rpBuildNodes,
            const std::vector<FBvhPrimitiveInfo>& crPrimitiveInfos,
            FMortonPrimitive* pMortonPrimitives,
            UINT32 dwPrimitivesNum,
            UINT32* pdwTotalNodes,
            std::vector<std::shared_ptr<FTrianglePrimitive>>& rpOrderedPrimitives,
            std::atomic<UINT32>* pdwAtomicOrderedPrimsOffset,
            UINT32 dwSplitBitIndex
        ) const;

        FBvhBuildNode* BuildUpperSAH(
            std::vector<FBvhBuildNode*>& rpTreeletRoots,
            UINT32 dwStart,
            UINT32 dwEnd,
            UINT32* pdwTotalNodes
        ) const;

        UINT32 FlattenBvhTree(FBvhBuildNode* pNode, UINT32* pdwOffset);

    private:
        const UINT32 m_dwMaxPrimitivesInNode;
        const ESplitMethod cSplidMethod;
        std::vector<std::shared_ptr<FTrianglePrimitive>> m_pPrimitives;
        std::vector<FLinearBvhNode> m_Nodes;
    };

}

#endif