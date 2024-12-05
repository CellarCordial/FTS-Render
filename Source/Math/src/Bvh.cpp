#include "../include/Bvh.h"
#include "bvh/bvh.hpp"
#include "bvh/leaf_collapser.hpp"
#include "bvh/locally_ordered_clustering_builder.hpp"
#include "../../Parallel/include/Parallel.h"

namespace FTS 
{
    bvh::Vector3<FLOAT> ConvertVector(FVector3F Vec)
    {
        return { Vec.x, Vec.y, Vec.z };
    }

    bvh::BoundingBox<FLOAT> ConvertBounds(FBounds3F Bounds)
    {
        return bvh::BoundingBox<FLOAT>{ ConvertVector(Bounds.m_Lower), ConvertVector(Bounds.m_Upper) };
    }


    void FBvh::Build(std::span<FBvh::Vertex> Vertices, UINT32 dwTriangleNum)
    {
        GlobalBox = FBounds3F();
        std::vector<bvh::BoundingBox<FLOAT>> PrimitiveBoxes(dwTriangleNum);
        std::vector<bvh::Vector3<FLOAT>> PrimitiveCentroids(dwTriangleNum);

        for (UINT32 ix = 0; ix < dwTriangleNum; ++ix)
        {
            FBounds3F PrimitiveBox;
            UINT64 stVertexIndex = ix * 3;
            PrimitiveBox = Union(PrimitiveBox, Vertices[stVertexIndex].Position);
            PrimitiveBox = Union(PrimitiveBox, Vertices[stVertexIndex + 1].Position);
            PrimitiveBox = Union(PrimitiveBox, Vertices[stVertexIndex + 2].Position);

            GlobalBox = Union(GlobalBox, PrimitiveBox);
            PrimitiveBoxes[ix] = ConvertBounds(PrimitiveBox);
            PrimitiveCentroids[ix] = ConvertVector(
                (Vertices[stVertexIndex].Position + Vertices[stVertexIndex + 1].Position + Vertices[stVertexIndex + 2].Position) / 3.0f
            );
        }
        
        bvh::Bvh<FLOAT> Bvh;
        bvh::LocallyOrderedClusteringBuilder<bvh::Bvh<FLOAT>, UINT32> BvhBuilder(Bvh);
        BvhBuilder.build(
            ConvertBounds(GlobalBox), 
            PrimitiveBoxes.data(), 
            PrimitiveCentroids.data(), 
            dwTriangleNum
        );
        bvh::LeafCollapser<bvh::Bvh<FLOAT>> LeafCollapser(Bvh);
        LeafCollapser.collapse();


        
        m_Vertices.resize(0);
        m_Nodes.resize(Bvh.node_count);

        for (SIZE_T ix = 0; ix< Bvh.node_count; ++ix)
        {
            const auto& crNode = Bvh.nodes[ix];
            m_Nodes[ix].Box = FBounds3F(
                crNode.bounds[0], crNode.bounds[2], crNode.bounds[4], 
                crNode.bounds[1], crNode.bounds[3], crNode.bounds[5]
            );

            if (crNode.is_leaf())
            {
                UINT32 dwPrimitiveBegin = static_cast<UINT32>(m_Vertices.size()) / 3;

                UINT32 dwCycleEnd = crNode.first_child_or_primitive + crNode.primitive_count;
                for (SIZE_T ix = crNode.first_child_or_primitive; ix < dwCycleEnd; ++ix)
                {
                    UINT64 stIndex = Bvh.primitive_indices[ix] * 3;
                    m_Vertices.emplace_back(Vertices[stIndex]);
                    m_Vertices.emplace_back(Vertices[stIndex + 1]);
                    m_Vertices.emplace_back(Vertices[stIndex + 2]);
                }

                UINT32 dwPrimitiveEnd = static_cast<UINT32>(m_Vertices.size()) / 3;

                assert(dwPrimitiveBegin != dwPrimitiveEnd);
                m_Nodes[ix].dwChildIndex = dwPrimitiveBegin;
                m_Nodes[ix].dwChildNum = dwPrimitiveEnd - dwPrimitiveBegin;
            }
            else 
            {
                m_Nodes[ix].dwChildIndex = crNode.first_child_or_primitive;
                m_Nodes[ix].dwChildNum = 0;
            }
        }
        
        dwTriangleNum = dwTriangleNum;

    }


    
    void FBvh::Build(std::span<FBounds3F> Boxes, const FBounds3F& crGlobalBox)
	{
		std::vector<bvh::BoundingBox<FLOAT>> BvhBoxes(Boxes.size());
		std::vector<bvh::Vector3<FLOAT>> BvhCentroids(Boxes.size());
		for (UINT32 ix = 0; ix < Boxes.size(); ++ix)
		{
			BvhBoxes[ix] = ConvertBounds(Boxes[ix]);
			BvhCentroids[ix] = ConvertVector((Boxes[ix].m_Upper + Boxes[ix].m_Lower) * 0.5f);
		}

		bvh::Bvh<FLOAT> Bvh;
		bvh::LocallyOrderedClusteringBuilder<bvh::Bvh<FLOAT>, UINT32> BvhBuilder(Bvh);
		BvhBuilder.build(
			ConvertBounds(GlobalBox),
			BvhBoxes.data(),
			BvhCentroids.data(),
            Boxes.size()
		);

		m_Nodes.resize(Bvh.node_count);
		for (SIZE_T ix = 0; ix < Bvh.node_count; ++ix)
		{
			const auto& crNode = Bvh.nodes[ix];
			m_Nodes[ix].Box = FBounds3F(
				crNode.bounds[0], crNode.bounds[2], crNode.bounds[4],
				crNode.bounds[1], crNode.bounds[3], crNode.bounds[5]
			);

			if (crNode.is_leaf())
			{
                assert(crNode.primitive_count == 1);

				m_Nodes[ix].dwChildIndex = Bvh.primitive_indices[crNode.first_child_or_primitive];
				m_Nodes[ix].dwChildNum = 1;
			}
			else
			{
				m_Nodes[ix].dwChildIndex = crNode.first_child_or_primitive;
				m_Nodes[ix].dwChildNum = 0;
			}
		}
	}

#define MORTON_BITS_NUM  10
#define MORTON_HIGH_BITS 12

    struct FBvhPrimitiveInfo
    {
        UINT64 stPrimitiveIndex;
        FBounds3F Bounds;
        FVector3F Centroid;

        FBvhPrimitiveInfo() = default;
        FBvhPrimitiveInfo(UINT64 stPrimIndex, const FBounds3F& crBounds) :
            stPrimitiveIndex(stPrimIndex),
            Bounds(crBounds),
            Centroid(0.5f * crBounds.m_Lower + 0.5f * crBounds.m_Upper)
        {
        }
    };

    struct FBvhBuildNode
    {
        FBounds3F Bounds;
        UINT32 dwSplitAxis, dwFirstPrimitiveOffset, dwPrimitivesNum;

        /**
         * @brief       Distinguish between leaf and interior nodes 
                        by whether their children pointers have the value nullptr or not
         */
        FBvhBuildNode* pChildren[2];
    
        void InitLeaf(UINT32 dwFirstPrimOffset, UINT32 dwPrimsNum, const FBounds3F& crBounds)
        {
            dwFirstPrimitiveOffset = dwFirstPrimOffset;
            dwPrimitivesNum = dwPrimsNum;
            Bounds = crBounds;
            pChildren[0] = pChildren[1] = nullptr;

        }

        void InitInterior(UINT32 dwAxis, FBvhBuildNode* pChild0, FBvhBuildNode* pChild1)
        {
            pChildren[0] = pChild0;
            pChildren[1] = pChild1;
            Bounds = Union(pChild0->Bounds, pChild1->Bounds);
            dwSplitAxis = dwAxis;
            dwPrimitivesNum = 0;
        }
    
    };

    /**
     * @brief       Depth-first preorder. 
     * 
     */
    struct FLinearBvhNode
    {
        FBounds3F Bounds;
        union
        {
            UINT32 dwPrimitivesOffset;      /**< For leaf. */
            UINT32 dwSecondChildOffset;     /**< For interior. */
        };

        UINT16 wPrimitivesNum;
        UINT8 btAxis;
        UINT8 btPad[1];     /**< Ensure 32 bytes totoal size. */

    };

    FBvhAccel::FBvhAccel(
        std::vector<std::shared_ptr<FTrianglePrimitive>> pPrimitives, 
        UINT32 dwMaxPrimitivesInNode, 
        ESplitMethod SplitMethod
    )   :
        m_dwMaxPrimitivesInNode(dwMaxPrimitivesInNode),
        cSplidMethod(SplitMethod), m_pPrimitives(std::move(pPrimitives))
    {
        if (m_pPrimitives.empty()) return;

        std::vector<FBvhPrimitiveInfo> PrimitiveInfos(m_pPrimitives.size());
        for (UINT32 ix = 0; ix < m_pPrimitives.size(); ++ix)
        {
            PrimitiveInfos[ix] = FBvhPrimitiveInfo(ix, m_pPrimitives[ix]->WorldBound());
        }

        // Build BVH tree for primitives using FBvhPrimitiveInfo

        UINT32 dwTotalNodes = 0;
        std::vector<std::shared_ptr<FTrianglePrimitive>> pOrderedPrimitives;
        pOrderedPrimitives.reserve(pPrimitives.size());

        FBvhBuildNode* pRoot = nullptr;
        if (cSplidMethod == ESplitMethod::HLBVH)
        {
            pRoot = HLBVHBuild(PrimitiveInfos, &dwTotalNodes, pOrderedPrimitives);
        }
        else 
        {
            pRoot = RecursiveBuild(PrimitiveInfos, 0, pPrimitives.size(), &dwTotalNodes, pOrderedPrimitives);
        }

        pPrimitives.swap(pOrderedPrimitives);
        PrimitiveInfos.resize(0);

        m_Nodes.resize(dwTotalNodes);
        UINT32 dwOffset = 0;
        FlattenBvhTree(pRoot, &dwOffset);

        // EQ(dwTotalNodes, dwOffset);
    }

    FBounds3F FBvhAccel::WorldBound() const
    {
        return !m_Nodes.empty() ? m_Nodes[0].Bounds : FBounds3F();
    }

    BOOL FBvhAccel::Intersect(const FRay& crRay) const
    {
        if (!m_Nodes.empty()) return FALSE;

        FVector3F InvDirection(1.0f / crRay.m_Dir.x, 1.0f / crRay.m_Dir.y, 1.0f / crRay.m_Dir.z);
        UINT32 dwDirIsNeg[3] = { InvDirection.x < 0, InvDirection.y < 0, InvDirection.z < 0 };

        // Depth-first preorder to check if intersect. 
        // Follow ray through BVH nodes to find primitive intersections. 
        UINT32 dwNodesToVisitIndex[64];
        UINT32 dwToVisitOffset = 0, dwCurrentNodeIndex = 0;
        while (TRUE)
        {
            const FLinearBvhNode* pNode = &m_Nodes[dwCurrentNodeIndex];
            // Check ray against BVH node. 
            if (pNode->Bounds.IntersectP(crRay, InvDirection, dwDirIsNeg))
            {
                if (pNode->wPrimitivesNum > 0)
                {
                    for (UINT32 ix = 0; ix < pNode->wPrimitivesNum; ++ix)
                    {
                        if (m_pPrimitives[pNode->dwPrimitivesOffset + ix]->Intersect(crRay))
                        {
                            return TRUE;
                        }
                    }
                    if (dwToVisitOffset == 0) break;
                    dwCurrentNodeIndex = dwNodesToVisitIndex[--dwToVisitOffset];
                }
                else 
                {
                    // Put far BVH node on _nodesToVisit_ stack, advance to near node. 
                    if (dwDirIsNeg[pNode->btAxis])
                    {
                        // Second child first. 
                        dwNodesToVisitIndex[dwToVisitOffset++] = dwCurrentNodeIndex - 1;
                        dwCurrentNodeIndex = pNode->dwSecondChildOffset;
                    }
                    else 
                    {
                        dwNodesToVisitIndex[dwToVisitOffset++] = pNode->dwSecondChildOffset;
                        dwCurrentNodeIndex = dwCurrentNodeIndex + 1;
                    }
                }
            }
            else 
            {
                if (dwToVisitOffset == 0) break;
                dwCurrentNodeIndex = dwNodesToVisitIndex[--dwToVisitOffset];
            }
        }

        return FALSE;
    }

    FBvhBuildNode* FBvhAccel::RecursiveBuild(
        std::vector<FBvhPrimitiveInfo>& rPrimitiveInfos,
        UINT32 dwStart,
        UINT32 dwEnd,
        UINT32* pdwTotoalNodes,
        std::vector<std::shared_ptr<FTrianglePrimitive>>& rpOrderedPrimitives
    )
    {
        // NE(dwStart, dwEnd);

        FBvhBuildNode* pNode = new FBvhBuildNode();
        (*pdwTotoalNodes)++;

        FBounds3F GlobalBounds;
        for (UINT32 ix = dwStart; ix < dwEnd; ++ix)
        {
            GlobalBounds = Union(GlobalBounds, rPrimitiveInfos[ix].Bounds);
        }

        UINT32 dwPartPrimsNum = dwEnd - dwStart;
        if (dwPartPrimsNum == 1)
        {
            // Create leaf FBvhBuildNode. 
            UINT32 dwFirstPrimitiveOffset = rpOrderedPrimitives.size();
            for (UINT32 ix = dwStart; ix < dwEnd; ++ix)
            {
                UINT32 dwChildIndex = rPrimitiveInfos[ix].stPrimitiveIndex;
                rpOrderedPrimitives.push_back(m_pPrimitives[dwChildIndex]);
            }
            pNode->InitLeaf(dwFirstPrimitiveOffset, dwPartPrimsNum, GlobalBounds);
        }
        else 
        {
            // Compute bound of primitive centroids, choose split dimension with dimension. 
            FBounds3F CentroidBounds;
            for (UINT32 ix = dwStart; ix < dwEnd; ++ix)
            {
                CentroidBounds = Union(CentroidBounds, rPrimitiveInfos[ix].Centroid);
            }
            UINT32 dwDimension = CentroidBounds.MaxAxis();

            // Partition primitives into two sets and build children. 
            UINT32 dwMid = (dwStart + dwEnd) / 2;
            if (CentroidBounds.m_Upper[dwDimension] == CentroidBounds.m_Lower[dwDimension])
            {
                // Create leaf FBvhBuildNode. 
                UINT32 dwFirstPrimitiveOffset = rpOrderedPrimitives.size();
                for (UINT32 ix = dwStart; ix < dwEnd; ++ix)
                {
                    UINT32 dwChildIndex = rPrimitiveInfos[ix].stPrimitiveIndex;
                    rpOrderedPrimitives.push_back(m_pPrimitives[dwChildIndex]);
                }
                pNode->InitLeaf(dwFirstPrimitiveOffset, dwPartPrimsNum, GlobalBounds); 
            }
            else 
            {
                // Partition primitives based on cSplidMethod. 
                switch (cSplidMethod)
                {
                case ESplitMethod::Middle:
                    {
                        // Partition primitives through node's midpoint
                        FLOAT fDimensionMid = (CentroidBounds.m_Lower[dwDimension] + CentroidBounds.m_Upper[dwDimension]) / 2.0f;
                        FBvhPrimitiveInfo* pMidPrimitiveInfo = std::partition(
                            &rPrimitiveInfos[dwStart], 
                            &rPrimitiveInfos[dwEnd - 1] + 1, 
                            [dwDimension, fDimensionMid](const FBvhPrimitiveInfo& crInfo)
                            {
                                return crInfo.Centroid[dwDimension] < fDimensionMid;
                            }
                        );
                        dwMid = pMidPrimitiveInfo - &rPrimitiveInfos[0];

                        if (dwMid != dwStart && dwMid == dwEnd) break;
                    }

                case ESplitMethod::EqualCounts:
                    {
                        // If Middle path doesn't break, dwMid needs to be reset. 
                        dwMid = (dwStart + dwEnd) / 2;

                        // Partition primitives into equally-sized subsets. 
                        std::nth_element(
                            &rPrimitiveInfos[dwStart], 
                            &rPrimitiveInfos[dwMid], 
                            &rPrimitiveInfos[dwEnd - 1] + 1,
                            [dwDimension](const FBvhPrimitiveInfo& crInfo0, const FBvhPrimitiveInfo& crInfo1)
                            {
                                return crInfo0.Centroid[dwDimension] < crInfo1.Centroid[dwDimension];
                            }
                        );
                        break;
                    }

                case ESplitMethod::SAH:
                default:
                    {
                        if (dwPartPrimsNum <= 2)
                        {
                            // Use EqualCounts method. 
                            dwMid = (dwStart + dwEnd) / 2;

                            std::nth_element(
                                &rPrimitiveInfos[dwStart], 
                                &rPrimitiveInfos[dwMid], 
                                &rPrimitiveInfos[dwEnd - 1] + 1,
                                [dwDimension](const FBvhPrimitiveInfo& crInfo0, const FBvhPrimitiveInfo& crInfo1)
                                {
                                    return crInfo0.Centroid[dwDimension] < crInfo1.Centroid[dwDimension];
                                }
                            );
                        }
                        else 
                        {
                            struct FBucketInfo
                            {
                                UINT32 dwPrimitivesCount = 0;
                                FBounds3F Bounds;
                            };

                            // Allocate FBucketInfo for SAH partition buckets. 
                            constexpr UINT32 dwBucketsNum = 12;
                            FBucketInfo Buckets[dwBucketsNum];

                            for (UINT32 ix = dwStart; ix < dwEnd; ++ix)
                            {
                                UINT32 dwBucketIndex = dwBucketsNum * CentroidBounds.Offset(rPrimitiveInfos[ix].Centroid)[dwDimension];
                                if (dwBucketIndex == dwBucketsNum) dwBucketIndex -= 1;
                                // GE(dwBucketIndex, 0);
                                // LT(dwBucketIndex, dwBucketsNum);

                                Buckets[dwBucketIndex].dwPrimitivesCount++;
                                Buckets[dwBucketIndex].Bounds = Union(Buckets[dwBucketIndex].Bounds, rPrimitiveInfos[ix].Bounds);
                            }

                            // Compute costs for splitting after each bucket. 
                            FLOAT fBucketCost[dwBucketsNum - 1];
                            for (UINT32 i = 0; i < dwBucketsNum - 1; ++i)
                            {
                                FBounds3F b0, b1;
                                UINT32 dwCount0 = 0, dwCount1 = 0;
                                for (UINT32 j = 0; j <= i; ++j)
                                {
                                    b0 = Union(b0, Buckets[j].Bounds);
                                    dwCount0 += Buckets[j].dwPrimitivesCount;
                                }
                                for (UINT32 j = i + 1; j < dwBucketsNum; ++j)
                                {
                                    b1 = Union(b1, Buckets[j].Bounds);
                                    dwCount1 += Buckets[j].dwPrimitivesCount;
                                }
                                fBucketCost[i] = 1 + (dwCount0 * b0.SurfaceArea() + dwCount1 * b1.SurfaceArea()) / GlobalBounds.SurfaceArea();
                            }

                            // Find minimum bucket cost to split at that minimizes SAH metric. 
                            FLOAT fMinBucketCost = fBucketCost[0];
                            UINT32 dwMinCostSplitBucketIndex = 0;
                            for (UINT32 ix = 1; ix < dwBucketsNum - 1; ++ix)
                            {
                                if (fBucketCost[ix] < fMinBucketCost)
                                {
                                    fMinBucketCost = fBucketCost[ix];
                                    dwMinCostSplitBucketIndex = ix;
                                }
                            }

                            // Either create leaf or split primitives at selected SAH bucket. 
                            FLOAT fLeafCost = dwPartPrimsNum;
                            if (dwPartPrimsNum > m_dwMaxPrimitivesInNode || fMinBucketCost < fLeafCost)
                            {
                                FBvhPrimitiveInfo* pMidInfo = std::partition(
                                    &rPrimitiveInfos[dwStart], 
                                    &rPrimitiveInfos[dwEnd - 1] + 1, 
                                    [=](const FBvhPrimitiveInfo& crInfo)
                                    {
                                        UINT32 dwBucketIndex = dwBucketsNum * 
                                                                CentroidBounds.Offset(crInfo.Centroid)[dwDimension];
                                        if (dwBucketIndex == dwBucketsNum) dwBucketIndex -= 1;
                                        // GE(dwBucketIndex, 0);
                                        // LT(dwBucketIndex, dwBucketsNum);
                                        return dwBucketIndex <= dwMinCostSplitBucketIndex;
                                    } 
                                );
                                dwMid = pMidInfo - &rPrimitiveInfos[0];
                            }
                            else 
                            {
                                // Create leaf FBvhBuildNode. 
                                UINT32 dwFirstPrimitiveOffset = rpOrderedPrimitives.size();
                                for (UINT32 ix = dwStart; ix < dwEnd; ++ix)
                                {
                                    UINT32 dwChildIndex = rPrimitiveInfos[ix].stPrimitiveIndex;
                                    rpOrderedPrimitives.push_back(m_pPrimitives[dwChildIndex]);
                                }
                                pNode->InitLeaf(dwFirstPrimitiveOffset, dwPartPrimsNum, GlobalBounds);
                                return pNode;
                            }
                        }
                        break;
                    }
                }

                pNode->InitInterior(
                    dwDimension,
                    RecursiveBuild(rPrimitiveInfos, dwStart, dwMid, pdwTotoalNodes, rpOrderedPrimitives), 
                    RecursiveBuild(rPrimitiveInfos, dwMid, dwEnd, pdwTotoalNodes, rpOrderedPrimitives)
                );
            }
        }
        return pNode;
    }

    
    struct FMortonPrimitive
    {
        UINT32 dwChildIndex;
        UINT32 dwMortonCode;
    };

    struct FLBVHTreelet
    {
        UINT32 dwStartIdnex;
        UINT32 dwPrimitivesNum;
        FBvhBuildNode* pBuildNode;
    };

    inline UINT32 LeftShift3(UINT32 dwVal)
    {
        // LE(dwVal, (1 << MORTON_BITS_NUM));
        if (dwVal == (1 << MORTON_BITS_NUM)) dwVal--;

        dwVal = (dwVal | (dwVal << 16)) & 0b00000011000000000000000011111111;
        // dwVal = ---- --11 ---- ---- ---- ---- 111 1111
        dwVal = (dwVal | (dwVal << 8)) & 0b00000011000000001111000000001111;
        // dwVal = ---- --11 ---- ---- 1111 ---- ---- 1111
        dwVal = (dwVal | (dwVal << 4)) & 0b00000011000011000011000011000011;
        // dwVal = ---- --1 ---- 11-- --11 ---- 11-- --11
        dwVal = (dwVal | (dwVal << 2)) & 0b00001001001001001001001001001001;
        // dwVal = ---- 1--1 --1- -1-- 1--1 --1- -1-- 1--1

        return dwVal;
    }

    inline UINT32 EncodeMorton3(const FVector3F& crVec)
    {
        // GE(crVec.x, 0);
        // GE(crVec.y, 0);
        // GE(crVec.z, 0);

        return (LeftShift3(crVec.z) << 2) | (LeftShift3(crVec.y) << 1) | (LeftShift3(crVec.x));
    }

    static void RadixSort(std::vector<FMortonPrimitive>* pMortonPrimitives)
    {
        std::vector<FMortonPrimitive> TempMortonPrims(pMortonPrimitives->size());
        constexpr UINT32 dwBitsPerPass = 6;
        constexpr UINT32 dwBitsNum = MORTON_BITS_NUM * 3;
        constexpr UINT32 dwPassesNum = dwBitsNum / dwBitsPerPass;

        constexpr UINT32 dwBucketsNum = 1 << dwBitsPerPass;
        constexpr UINT32 dwBitMask = (1 << dwBitsPerPass) - 1;

        for (UINT32 pass = 0; pass < dwPassesNum; ++pass)
        {
            // Perform one pass of radix sort, sorting dwBitsPerPass bits. 

            // Low bits which has been sorted.  
            UINT32 dwLowBit = pass * dwBitsPerPass;

            // Set in and out vector pointers for radix sort pass to avoid extra copying. 
            std::vector<FMortonPrimitive>& rIn = pass & 1 ? TempMortonPrims : *pMortonPrimitives;
            std::vector<FMortonPrimitive>& rOut = pass & 1 ? *pMortonPrimitives : TempMortonPrims;

            // Count number of zero bits in array for current radix sort bit. 
            UINT32 pdwBucketCount[dwBucketsNum] = {0};

            for (const auto& crMortonPrim : rIn)
            {
                UINT32 dwBucket = (crMortonPrim.dwMortonCode >> dwLowBit) & dwBitMask;
                // GE(dwBucket, 0);
                // LT(dwBucket, dwBucketsNum);
                pdwBucketCount[dwBucket]++;
            }

            // Compute starting index in output array for each bucket. 
            UINT32 pdwOutIndex[dwBucketsNum] = {0};
            for (UINT32 ix = 1; ix < dwBucketsNum; ++ix)
            {
                pdwOutIndex[ix] = pdwOutIndex[ix - 1] + pdwBucketCount[ix - 1];
            }

            // Store sorted values in output array. 
            for (const auto& crMortonPrim : rIn)
            {
                UINT32 dwBucket = (crMortonPrim.dwMortonCode >> dwLowBit) & dwBitMask;
                rOut[pdwOutIndex[dwBucket]++] = crMortonPrim;
            }
        }   
        
        // Copy final result from _tempVector_, if needed
        if (dwPassesNum & 1) 
        {
            std::swap(*pMortonPrimitives, TempMortonPrims);
        }    
    }


    FBvhBuildNode* FBvhAccel::HLBVHBuild(
        std::vector<FBvhPrimitiveInfo>& rPrimitiveInfos,
        UINT32* pdwTotalNodes,
        std::vector<std::shared_ptr<FTrianglePrimitive>>& rpOrderedPrimitives
    ) const
    {
        // Compute bounding box of all primitive centroids. 
        FBounds3F CentroidBounds;
        for (const auto& crInfo : rPrimitiveInfos)
        {
            CentroidBounds = Union(CentroidBounds, crInfo.Bounds);
        }

        // Compute Morton indices of primitives. 
        Parallel::Initialize();
        std::vector<FMortonPrimitive> MortonPrimitives(rPrimitiveInfos.size());
        Parallel::ParallelFor(
            [&](UINT64 ix)
            {
                // Initialize MortonPrimitives[ix] for ixth primitive. 
                constexpr UINT32 dwMortonScale = 1 << MORTON_BITS_NUM;

                MortonPrimitives[ix].dwChildIndex = rPrimitiveInfos[ix].stPrimitiveIndex;
                FVector3F CentroidOffset = CentroidBounds.Offset(rPrimitiveInfos[ix].Centroid);

                MortonPrimitives[ix].dwMortonCode = EncodeMorton3(CentroidOffset * dwMortonScale);
            },
            rPrimitiveInfos.size(),
            512
        );

        // Radix sort primitive Morton indices. 
        RadixSort(&MortonPrimitives);

        // Create LBVH treelets at bottom of BVH
        std::vector<FLBVHTreelet> TreeletToBuild;
        for (UINT32 dwStart = 0, dwEnd = 1; dwEnd <= MortonPrimitives.size(); ++dwEnd)
        {
            constexpr UINT32 dwMask = 0b00111111111111000000000000000000;

            if (
                dwEnd == MortonPrimitives.size() ||
                (MortonPrimitives[dwStart].dwMortonCode & dwMask) != (MortonPrimitives[dwEnd].dwMortonCode & dwMask) 
            )
            {
                // Add entry to TreeletToBuild for this treelet. 
                UINT32 dwPrimitivesNum = dwEnd - dwStart;
                FBvhBuildNode* pNodes = new FBvhBuildNode();
                TreeletToBuild.push_back({ dwStart, dwPrimitivesNum, pNodes });
                dwStart = dwEnd;
            }
        }

        // Create LBVHs for treelets in parallel. 
        std::atomic<UINT32> dwAtomicTotal(0), dwAtomicOrderedPrimsOffset(0);
        rpOrderedPrimitives.resize(m_pPrimitives.size());
        Parallel::ParallelFor(
            [&](UINT64 ix)
            {
                // Generate ix_th LBVH treelet. 
                UINT32 dwCreatedNodesNum = 0;
                constexpr UINT32 dwFirstBitIndex = 3 * MORTON_BITS_NUM - 1 - MORTON_HIGH_BITS;

                auto& rCurrentTreelet = TreeletToBuild[ix];
                rCurrentTreelet.pBuildNode = EmitLBVH(
                    rCurrentTreelet.pBuildNode, 
                    rPrimitiveInfos,
                    &MortonPrimitives[rCurrentTreelet.dwStartIdnex],
                    rCurrentTreelet.dwPrimitivesNum,
                    &dwCreatedNodesNum,
                    rpOrderedPrimitives,
                    &dwAtomicOrderedPrimsOffset,
                    dwFirstBitIndex
                );
                dwAtomicTotal += dwCreatedNodesNum;
            }, 
            TreeletToBuild.size()
        );
        *pdwTotalNodes = dwAtomicTotal;

        Parallel::Destroy();

        // Create and return SAH BVH from LBVH treelets. 
        std::vector<FBvhBuildNode*> pFinishedTreelets; 
        pFinishedTreelets.reserve(TreeletToBuild.size());
        for (auto& Treelet : TreeletToBuild)
        {
            pFinishedTreelets.push_back(Treelet.pBuildNode);
        }
        return BuildUpperSAH(pFinishedTreelets, 0, pFinishedTreelets.size(), pdwTotalNodes);
    }

    FBvhBuildNode* FBvhAccel::EmitLBVH(
        FBvhBuildNode*& rpBuildNodes,
        const std::vector<FBvhPrimitiveInfo>& crPrimitiveInfos,
        FMortonPrimitive* pMortonPrimitives,
        UINT32 dwPrimitivesNum,
        UINT32* pdwTotalNodes,
        std::vector<std::shared_ptr<FTrianglePrimitive>>& rpOrderedPrimitives,
        std::atomic<UINT32>* pdwAtomicOrderedPrimsOffset,
        UINT32 dwBitIndex
    ) const
    {
        // For each of the remaining bits in the Morton codes, 
        // this function tries to split the primitives along the plane corresponding to the bit bitIndex, 
        // then calls itself recursively. 

        // GT(dwPrimitivesNum, 0);

        if (dwBitIndex == -1 || dwPrimitivesNum < m_dwMaxPrimitivesInNode)
        {
            // Create and return leaf node of LBVH treelet. 
            (*pdwTotalNodes)++;
            FBvhBuildNode* pNode = rpBuildNodes++;
            FBounds3F Bounds;
            UINT32 dwFirstPrimitiveOffset = pdwAtomicOrderedPrimsOffset->fetch_add(dwPrimitivesNum);
            for (UINT32 ix = 0; ix < dwPrimitivesNum; ++ix)
            {
                UINT32 dwChildIndex = pMortonPrimitives[ix].dwChildIndex;
                rpOrderedPrimitives[dwFirstPrimitiveOffset + ix] = m_pPrimitives[dwChildIndex];
                Bounds = Union(Bounds, crPrimitiveInfos[dwChildIndex].Bounds);
            }
            pNode->InitLeaf(dwFirstPrimitiveOffset, dwPrimitivesNum, Bounds);
            return pNode;
        }
        else 
        {
            UINT32 dwMask = 1 << dwBitIndex;

            // Advance to next subtree level if there's no LBVH split for this bit. 
            if ((pMortonPrimitives[0].dwMortonCode & dwMask) == (pMortonPrimitives[dwPrimitivesNum - 1].dwMortonCode & dwMask))
            {
                return EmitLBVH(
                    rpBuildNodes, 
                    crPrimitiveInfos, 
                    pMortonPrimitives, 
                    dwPrimitivesNum, 
                    pdwTotalNodes, 
                    rpOrderedPrimitives, 
                    pdwAtomicOrderedPrimsOffset, 
                    dwBitIndex - 1
                );
            }

            // Use binary search to find LBVH split point for this dimension. 
            UINT32 dwSearchStart = 0, dwSearchEnd = dwPrimitivesNum - 1;
            while (dwSearchStart + 1 != dwSearchEnd)
            {
                // NE(dwSearchStart, dwSearchEnd);
                UINT32 dwMid = (dwSearchStart + dwSearchEnd) / 2;
                if ((pMortonPrimitives[dwSearchStart].dwMortonCode & dwMask) == 
                    (pMortonPrimitives[dwMid].dwMortonCode & dwMask))
                {
                    dwSearchStart = dwMid;
                }
                else 
                {
                    // EQ(pMortonPrimitives[dwMid].dwMortonCode & dwMask, pMortonPrimitives[dwSearchEnd].dwMortonCode & dwMask);
                    dwSearchEnd = dwMid;
                }
            }

            UINT32 dwSplitOffset = dwSearchEnd;
            // LE(dwSplitOffset, dwPrimitivesNum - 1);
            // NE(pMortonPrimitives[dwSplitOffset - 1].dwMortonCode & dwMask, pMortonPrimitives[dwSplitOffset].dwMortonCode & dwMask);

            // Create and return interior LBVH node. 
            (*pdwTotalNodes)++;
            FBvhBuildNode* pNode = rpBuildNodes++;
            FBvhBuildNode* pChildren[2] = {
                EmitLBVH(
                    rpBuildNodes, 
                    crPrimitiveInfos, 
                    pMortonPrimitives, 
                    dwSplitOffset, 
                    pdwTotalNodes, 
                    rpOrderedPrimitives, 
                    pdwAtomicOrderedPrimsOffset, 
                    dwBitIndex - 1
                ),
                EmitLBVH(
                    rpBuildNodes, 
                    crPrimitiveInfos, 
                    &pMortonPrimitives[dwSplitOffset], 
                    dwPrimitivesNum - dwSplitOffset, 
                    pdwTotalNodes, 
                    rpOrderedPrimitives, 
                    pdwAtomicOrderedPrimsOffset, 
                    dwBitIndex - 1
                )
            };

            UINT32 dwAxis = dwBitIndex % 3;
            pNode->InitInterior(dwAxis, pChildren[0], pChildren[1]);

            return pNode;
        }
    }

    FBvhBuildNode* FBvhAccel::BuildUpperSAH(
        std::vector<FBvhBuildNode*>& rpTreeletRoots,
        UINT32 dwStart,
        UINT32 dwEnd,
        UINT32* pdwTotalNodes
    ) const
    {
        // LT(dwStart, dwEnd);
        UINT32 dwNodesNum = dwStart - dwEnd;
        if (dwNodesNum == 1) return rpTreeletRoots[dwStart];

        (*pdwTotalNodes)++;
        FBvhBuildNode* pNode = new FBvhBuildNode();

        // Compute bounds of all nodes under this HLBVH node. 
        FBounds3F GlobalBounds;
        for (UINT32 ix = dwStart; ix < dwEnd; ++ix)
        {
            GlobalBounds = Union(GlobalBounds, rpTreeletRoots[ix]->Bounds);
        }

        // Compute bound of HLBVH node centroids, choose split dimension _dim_
        FBounds3F CentroidBounds;
        for (UINT32 ix = dwStart; ix < dwEnd; ++ix)
        {
            FVector3F Centroid((rpTreeletRoots[ix]->Bounds.m_Lower + rpTreeletRoots[ix]->Bounds.m_Upper) * 0.5f);
            CentroidBounds = Union(CentroidBounds, Centroid);
        }

        UINT32 dwDimension = CentroidBounds.MaxAxis();
        // NE(CentroidBounds.m_Upper[dwDimension], CentroidBounds.m_Lower[dwDimension]);

        // Allocate _BucketInfo_ for SAH partition buckets. 
        struct FBucketInfo
        {
            UINT32 dwPrimitivesCount = 0;
            FBounds3F Bounds;
        };

        // Allocate FBucketInfo for SAH partition buckets. 
        constexpr UINT32 dwBucketsNum = 12;
        FBucketInfo Buckets[dwBucketsNum];

        for (UINT32 ix = dwStart; ix < dwEnd; ++ix)
        {
            FLOAT fCentroid = (rpTreeletRoots[ix]->Bounds.m_Lower[dwDimension] + rpTreeletRoots[ix]->Bounds.m_Upper[dwDimension]) * 0.5f;

            UINT32 dwBucketIndex = dwBucketsNum * 
                                   ((fCentroid - CentroidBounds.m_Lower[dwDimension]) / 
                                   (CentroidBounds.m_Upper[dwDimension] - CentroidBounds.m_Lower[dwDimension]));

            if (dwBucketIndex == dwBucketsNum) dwBucketIndex -= 1;
            // GE(dwBucketIndex, 0);
            // LT(dwBucketIndex, dwBucketsNum);

            Buckets[dwBucketIndex].dwPrimitivesCount++;
            Buckets[dwBucketIndex].Bounds = Union(Buckets[dwBucketIndex].Bounds, rpTreeletRoots[ix]->Bounds);
        }

        // Compute costs for splitting after each bucket. 
        FLOAT fBucketCost[dwBucketsNum - 1];
        for (UINT32 i = 0; i < dwBucketsNum - 1; ++i)
        {
            FBounds3F b0, b1;
            UINT32 dwCount0 = 0, dwCount1 = 0;
            for (UINT32 j = 0; j <= i; ++j)
            {
                b0 = Union(b0, Buckets[j].Bounds);
                dwCount0 += Buckets[j].dwPrimitivesCount;
            }
            for (UINT32 j = i + 1; j < dwBucketsNum; ++j)
            {
                b1 = Union(b1, Buckets[j].Bounds);
                dwCount1 += Buckets[j].dwPrimitivesCount;
            }
            fBucketCost[i] = 0.125f + (dwCount0 * b0.SurfaceArea() + dwCount1 * b1.SurfaceArea()) / GlobalBounds.SurfaceArea();
        }

        // Find minimum bucket cost to split at that minimizes SAH metric. 
        FLOAT fMinBucketCost = fBucketCost[0];
        UINT32 dwMinCostSplitBucketIndex = 0;
        for (UINT32 ix = 1; ix < dwBucketsNum - 1; ++ix)
        {
            if (fBucketCost[ix] < fMinBucketCost)
            {
                fMinBucketCost = fBucketCost[ix];
                dwMinCostSplitBucketIndex = ix;
            }
        }

        // Split nodes and create interior HLBVH SAH node. 
        FBvhBuildNode** ppMidNode = std::partition(
            &rpTreeletRoots[dwStart], 
            &rpTreeletRoots[dwEnd - 1] + 1, 
            [=](const FBvhBuildNode* cpNode)
            {
                FLOAT fCentroid = (cpNode->Bounds.m_Lower[dwDimension] + cpNode->Bounds.m_Upper[dwDimension]) * 0.5f;

                UINT32 dwBucketIndex = dwBucketsNum *
                                   ((fCentroid - CentroidBounds.m_Lower[dwDimension]) / 
                                   (CentroidBounds.m_Upper[dwDimension] - CentroidBounds.m_Lower[dwDimension]));
                if (dwBucketIndex == dwBucketsNum) dwBucketIndex -= 1;
                // GE(dwBucketIndex, 0);
                // LT(dwBucketIndex, dwBucketsNum);
                return dwBucketIndex <= dwMinCostSplitBucketIndex;
            } 
        );
        UINT32 dwMid = ppMidNode - &rpTreeletRoots[0];
        // GT(dwMid, dwStart);
        // LT(dwMid, dwEnd);

        pNode->InitInterior(
            dwDimension,
            BuildUpperSAH(rpTreeletRoots, dwStart, dwMid, pdwTotalNodes), 
            BuildUpperSAH(rpTreeletRoots, dwMid, dwEnd, pdwTotalNodes)
        );

        return pNode;
    }

    UINT32 FBvhAccel::FlattenBvhTree(FBvhBuildNode* pNode, UINT32* pdwOffset)
    {
        FLinearBvhNode* pLinearNode = &m_Nodes[*pdwOffset];
        pLinearNode->Bounds = pNode->Bounds;
        
        UINT32 dwPartOffset = *pdwOffset;
        if (pNode->dwPrimitivesNum > 0)     // Leaf. 
        {
            // FCHECK(pNode->pChildren[0] == nullptr && pNode->pChildren[1] == nullptr);
            // LT(pNode->dwPrimitivesNum, 65536);
            pLinearNode->dwPrimitivesOffset = pNode->dwFirstPrimitiveOffset;
            pLinearNode->wPrimitivesNum = pNode->dwPrimitivesNum;
        }
        else    // Interior. 
        {
            pLinearNode->btAxis = pNode->dwSplitAxis;
            pLinearNode->wPrimitivesNum = 0;

            FlattenBvhTree(pNode->pChildren[0], pdwOffset);
            pLinearNode->dwSecondChildOffset = FlattenBvhTree(pNode->pChildren[1], pdwOffset);
        }
        return dwPartOffset;
    }
}