#include "Bvh.h"
#include "bvh/bvh.hpp"
#include "bvh/leaf_collapser.hpp"
#include "bvh/locally_ordered_clustering_builder.hpp"
#include "../parallel/parallel.h"

namespace fantasy 
{
    bvh::Vector3<float> ConvertVector(Vector3F vec)
    {
        return { vec.x, vec.y, vec.z };
    }

    bvh::BoundingBox<float> ConvertBounds(Bounds3F Bounds)
    {
        return bvh::BoundingBox<float>{ ConvertVector(Bounds._lower), ConvertVector(Bounds._upper) };
    }


    void Bvh::build(std::span<Bvh::Vertex> vertices, uint32_t triangle_num)
    {
        global_box = Bounds3F();
        std::vector<bvh::BoundingBox<float>> PrimitiveBoxes(triangle_num);
        std::vector<bvh::Vector3<float>> PrimitiveCentroids(triangle_num);

        for (uint32_t ix = 0; ix < triangle_num; ++ix)
        {
            Bounds3F PrimitiveBox;
            uint64_t stVertexIndex = ix * 3;
            PrimitiveBox = merge(PrimitiveBox, vertices[stVertexIndex].position);
            PrimitiveBox = merge(PrimitiveBox, vertices[stVertexIndex + 1].position);
            PrimitiveBox = merge(PrimitiveBox, vertices[stVertexIndex + 2].position);

            global_box = merge(global_box, PrimitiveBox);
            PrimitiveBoxes[ix] = ConvertBounds(PrimitiveBox);
            PrimitiveCentroids[ix] = ConvertVector(
                (vertices[stVertexIndex].position + vertices[stVertexIndex + 1].position + vertices[stVertexIndex + 2].position) / 3.0f
            );
        }
        
        bvh::Bvh<float> Bvh;
        bvh::LocallyOrderedClusteringBuilder<bvh::Bvh<float>, uint32_t> BvhBuilder(Bvh);
        BvhBuilder.build(
            ConvertBounds(global_box), 
            PrimitiveBoxes.data(), 
            PrimitiveCentroids.data(), 
            triangle_num
        );
        bvh::LeafCollapser<bvh::Bvh<float>> LeafCollapser(Bvh);
        LeafCollapser.collapse();


        
        _vertices.clear();
        _nodes.resize(Bvh.node_count);

        for (size_t ix = 0; ix< Bvh.node_count; ++ix)
        {
            const auto& crNode = Bvh.nodes[ix];
            _nodes[ix].box = Bounds3F(
                crNode.bounds[0], crNode.bounds[2], crNode.bounds[4], 
                crNode.bounds[1], crNode.bounds[3], crNode.bounds[5]
            );

            if (crNode.is_leaf())
            {
                uint32_t dwPrimitiveBegin = static_cast<uint32_t>(_vertices.size()) / 3;

                uint32_t dwCycleEnd = crNode.first_child_or_primitive + crNode.primitive_count;
                for (size_t ix = crNode.first_child_or_primitive; ix < dwCycleEnd; ++ix)
                {
                    uint64_t index = Bvh.primitive_indices[ix] * 3;
                    _vertices.emplace_back(vertices[index]);
                    _vertices.emplace_back(vertices[index + 1]);
                    _vertices.emplace_back(vertices[index + 2]);
                }

                uint32_t dwPrimitiveEnd = static_cast<uint32_t>(_vertices.size()) / 3;

                assert(dwPrimitiveBegin != dwPrimitiveEnd);
                _nodes[ix].child_index = dwPrimitiveBegin;
                _nodes[ix].child_num = dwPrimitiveEnd - dwPrimitiveBegin;
            }
            else 
            {
                _nodes[ix].child_index = crNode.first_child_or_primitive;
                _nodes[ix].child_num = 0;
            }
        }
        
        this->triangle_num = triangle_num;

    }


    
    void Bvh::build(std::span<Bounds3F> boxes, const Bounds3F& global_box)
	{
		std::vector<bvh::BoundingBox<float>> BvhBoxes(boxes.size());
		std::vector<bvh::Vector3<float>> BvhCentroids(boxes.size());
		for (uint32_t ix = 0; ix < boxes.size(); ++ix)
		{
			BvhBoxes[ix] = ConvertBounds(boxes[ix]);
			BvhCentroids[ix] = ConvertVector((boxes[ix]._upper + boxes[ix]._lower) * 0.5f);
		}

		bvh::Bvh<float> Bvh;
		bvh::LocallyOrderedClusteringBuilder<bvh::Bvh<float>, uint32_t> BvhBuilder(Bvh);
		BvhBuilder.build(
			ConvertBounds(global_box),
			BvhBoxes.data(),
			BvhCentroids.data(),
            boxes.size()
		);

		_nodes.resize(Bvh.node_count);
		for (size_t ix = 0; ix < Bvh.node_count; ++ix)
		{
			const auto& crNode = Bvh.nodes[ix];
			_nodes[ix].box = Bounds3F(
				crNode.bounds[0], crNode.bounds[2], crNode.bounds[4],
				crNode.bounds[1], crNode.bounds[3], crNode.bounds[5]
			);

			if (crNode.is_leaf())
			{
                assert(crNode.primitive_count == 1);

				_nodes[ix].child_index = Bvh.primitive_indices[crNode.first_child_or_primitive];
				_nodes[ix].child_num = 1;
			}
			else
			{
				_nodes[ix].child_index = crNode.first_child_or_primitive;
				_nodes[ix].child_num = 0;
			}
		}
	}

#define MORTON_BITS_NUM  10
#define MORTON_HIGH_BITS 12

    struct BvhPrimitiveInfo
    {
        uint64_t stPrimitiveIndex;
        Bounds3F Bounds;
        Vector3F Centroid;

        BvhPrimitiveInfo() = default;
        BvhPrimitiveInfo(uint64_t stPrimIndex, const Bounds3F& bounds) :
            stPrimitiveIndex(stPrimIndex),
            Bounds(bounds),
            Centroid(0.5f * bounds._lower + 0.5f * bounds._upper)
        {
        }
    };

    struct BvhBuildNode
    {
        Bounds3F Bounds;
        uint32_t dwSplitAxis, dwFirstPrimitiveOffset, dwPrimitivesNum;

        /**
         * @brief       Distinguish between leaf and interior nodes 
                        by whether their children pointers have the value nullptr or not
         */
        BvhBuildNode* pChildren[2];
    
        void InitLeaf(uint32_t dwFirstPrimOffset, uint32_t dwPrimsNum, const Bounds3F& bounds)
        {
            dwFirstPrimitiveOffset = dwFirstPrimOffset;
            dwPrimitivesNum = dwPrimsNum;
            Bounds = bounds;
            pChildren[0] = pChildren[1] = nullptr;

        }

        void InitInterior(uint32_t dwAxis, BvhBuildNode* pChild0, BvhBuildNode* pChild1)
        {
            pChildren[0] = pChild0;
            pChildren[1] = pChild1;
            Bounds = merge(pChild0->Bounds, pChild1->Bounds);
            dwSplitAxis = dwAxis;
            dwPrimitivesNum = 0;
        }
    
    };

    /**
     * @brief       Depth-first preorder. 
     * 
     */
    struct LinearBvhNode
    {
        Bounds3F Bounds;
        union
        {
            uint32_t dwPrimitivesOffset;      /**< For leaf. */
            uint32_t dwSecondChildOffset;     /**< For interior. */
        };

        uint16_t wPrimitivesNum;
        uint8_t btAxis;
        uint8_t pad[1];     /**< Ensure 32 bytes totoal size. */

    };

    BvhAccel::BvhAccel(
        std::vector<std::shared_ptr<FTrianglePrimitive>> pPrimitives, 
        uint32_t dwMaxPrimitivesInNode, 
        ESplitMethod SplitMethod
    )   :
        m_dwMaxPrimitivesInNode(dwMaxPrimitivesInNode),
        cSplidMethod(SplitMethod), m_pPrimitives(std::move(pPrimitives))
    {
        if (m_pPrimitives.empty()) return;

        std::vector<BvhPrimitiveInfo> PrimitiveInfos(m_pPrimitives.size());
        for (uint32_t ix = 0; ix < m_pPrimitives.size(); ++ix)
        {
            PrimitiveInfos[ix] = BvhPrimitiveInfo(ix, m_pPrimitives[ix]->WorldBound());
        }

        // build BVH tree for primitives using BvhPrimitiveInfo

        uint32_t dwTotalNodes = 0;
        std::vector<std::shared_ptr<FTrianglePrimitive>> pOrderedPrimitives;
        pOrderedPrimitives.reserve(pPrimitives.size());

        BvhBuildNode* pRoot = nullptr;
        if (cSplidMethod == ESplitMethod::HLBVH)
        {
            pRoot = HLBVHBuild(PrimitiveInfos, &dwTotalNodes, pOrderedPrimitives);
        }
        else 
        {
            pRoot = RecursiveBuild(PrimitiveInfos, 0, pPrimitives.size(), &dwTotalNodes, pOrderedPrimitives);
        }

        pPrimitives.swap(pOrderedPrimitives);
        PrimitiveInfos.clear();

        _nodes.resize(dwTotalNodes);
        uint32_t offset = 0;
        FlattenBvhTree(pRoot, &offset);

        // EQ(dwTotalNodes, offset);
    }

    Bounds3F BvhAccel::WorldBound() const
    {
        return !_nodes.empty() ? _nodes[0].Bounds : Bounds3F();
    }

    bool BvhAccel::intersect(const Ray& ray) const
    {
        if (!_nodes.empty()) return false;

        Vector3F InvDirection(1.0f / ray.dir.x, 1.0f / ray.dir.y, 1.0f / ray.dir.z);
        uint32_t dwDirIsNeg[3] = { InvDirection.x < 0, InvDirection.y < 0, InvDirection.z < 0 };

        // Depth-first preorder to check if intersect. 
        // Follow ray through BVH nodes to find primitive intersections. 
        uint32_t dwNodesToVisitIndex[64];
        uint32_t dwToVisitOffset = 0, dwCurrentNodeIndex = 0;
        while (true)
        {
            const LinearBvhNode* pNode = &_nodes[dwCurrentNodeIndex];
            // Check ray against BVH node. 
            if (pNode->Bounds.intersect(ray, InvDirection, dwDirIsNeg))
            {
                if (pNode->wPrimitivesNum > 0)
                {
                    for (uint32_t ix = 0; ix < pNode->wPrimitivesNum; ++ix)
                    {
                        if (m_pPrimitives[pNode->dwPrimitivesOffset + ix]->intersect(ray))
                        {
                            return true;
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

        return false;
    }

    BvhBuildNode* BvhAccel::RecursiveBuild(
        std::vector<BvhPrimitiveInfo>& rPrimitiveInfos,
        uint32_t dwStart,
        uint32_t dwEnd,
        uint32_t* pdwTotoalNodes,
        std::vector<std::shared_ptr<FTrianglePrimitive>>& rpOrderedPrimitives
    )
    {
        // NE(dwStart, dwEnd);

        BvhBuildNode* pNode = new BvhBuildNode();
        (*pdwTotoalNodes)++;

        Bounds3F GlobalBounds;
        for (uint32_t ix = dwStart; ix < dwEnd; ++ix)
        {
            GlobalBounds = merge(GlobalBounds, rPrimitiveInfos[ix].Bounds);
        }

        uint32_t dwPartPrimsNum = dwEnd - dwStart;
        if (dwPartPrimsNum == 1)
        {
            // Create leaf BvhBuildNode. 
            uint32_t dwFirstPrimitiveOffset = rpOrderedPrimitives.size();
            for (uint32_t ix = dwStart; ix < dwEnd; ++ix)
            {
                uint32_t child_index = rPrimitiveInfos[ix].stPrimitiveIndex;
                rpOrderedPrimitives.push_back(m_pPrimitives[child_index]);
            }
            pNode->InitLeaf(dwFirstPrimitiveOffset, dwPartPrimsNum, GlobalBounds);
        }
        else 
        {
            // Compute bound of primitive centroids, choose split dimension with dimension. 
            Bounds3F CentroidBounds;
            for (uint32_t ix = dwStart; ix < dwEnd; ++ix)
            {
                CentroidBounds = merge(CentroidBounds, rPrimitiveInfos[ix].Centroid);
            }
            uint32_t dwDimension = CentroidBounds.max_axis();

            // Partition primitives into two sets and build children. 
            uint32_t dwMid = (dwStart + dwEnd) / 2;
            if (CentroidBounds._upper[dwDimension] == CentroidBounds._lower[dwDimension])
            {
                // Create leaf BvhBuildNode. 
                uint32_t dwFirstPrimitiveOffset = rpOrderedPrimitives.size();
                for (uint32_t ix = dwStart; ix < dwEnd; ++ix)
                {
                    uint32_t child_index = rPrimitiveInfos[ix].stPrimitiveIndex;
                    rpOrderedPrimitives.push_back(m_pPrimitives[child_index]);
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
                        float fDimensionMid = (CentroidBounds._lower[dwDimension] + CentroidBounds._upper[dwDimension]) / 2.0f;
                        BvhPrimitiveInfo* pMidPrimitiveInfo = std::partition(
                            &rPrimitiveInfos[dwStart], 
                            &rPrimitiveInfos[dwEnd - 1] + 1, 
                            [dwDimension, fDimensionMid](const BvhPrimitiveInfo& crInfo)
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
                            [dwDimension](const BvhPrimitiveInfo& crInfo0, const BvhPrimitiveInfo& crInfo1)
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
                                [dwDimension](const BvhPrimitiveInfo& crInfo0, const BvhPrimitiveInfo& crInfo1)
                                {
                                    return crInfo0.Centroid[dwDimension] < crInfo1.Centroid[dwDimension];
                                }
                            );
                        }
                        else 
                        {
                            struct FBucketInfo
                            {
                                uint32_t dwPrimitivesCount = 0;
                                Bounds3F Bounds;
                            };

                            // allocate FBucketInfo for SAH partition buckets. 
                            constexpr uint32_t dwBucketsNum = 12;
                            FBucketInfo Buckets[dwBucketsNum];

                            for (uint32_t ix = dwStart; ix < dwEnd; ++ix)
                            {
                                uint32_t dwBucketIndex = dwBucketsNum * CentroidBounds.offset(rPrimitiveInfos[ix].Centroid)[dwDimension];
                                if (dwBucketIndex == dwBucketsNum) dwBucketIndex -= 1;
                                // GE(dwBucketIndex, 0);
                                // LT(dwBucketIndex, dwBucketsNum);

                                Buckets[dwBucketIndex].dwPrimitivesCount++;
                                Buckets[dwBucketIndex].Bounds = merge(Buckets[dwBucketIndex].Bounds, rPrimitiveInfos[ix].Bounds);
                            }

                            // Compute costs for splitting after each bucket. 
                            float fBucketCost[dwBucketsNum - 1];
                            for (uint32_t i = 0; i < dwBucketsNum - 1; ++i)
                            {
                                Bounds3F b0, b1;
                                uint32_t dwCount0 = 0, dwCount1 = 0;
                                for (uint32_t j = 0; j <= i; ++j)
                                {
                                    b0 = merge(b0, Buckets[j].Bounds);
                                    dwCount0 += Buckets[j].dwPrimitivesCount;
                                }
                                for (uint32_t j = i + 1; j < dwBucketsNum; ++j)
                                {
                                    b1 = merge(b1, Buckets[j].Bounds);
                                    dwCount1 += Buckets[j].dwPrimitivesCount;
                                }
                                fBucketCost[i] = 1 + (dwCount0 * b0.surface_area() + dwCount1 * b1.surface_area()) / GlobalBounds.surface_area();
                            }

                            // Find minimum bucket cost to split at that minimizes SAH metric. 
                            float fMinBucketCost = fBucketCost[0];
                            uint32_t dwMinCostSplitBucketIndex = 0;
                            for (uint32_t ix = 1; ix < dwBucketsNum - 1; ++ix)
                            {
                                if (fBucketCost[ix] < fMinBucketCost)
                                {
                                    fMinBucketCost = fBucketCost[ix];
                                    dwMinCostSplitBucketIndex = ix;
                                }
                            }

                            // Either create leaf or split primitives at selected SAH bucket. 
                            float fLeafCost = dwPartPrimsNum;
                            if (dwPartPrimsNum > m_dwMaxPrimitivesInNode || fMinBucketCost < fLeafCost)
                            {
                                BvhPrimitiveInfo* pMidInfo = std::partition(
                                    &rPrimitiveInfos[dwStart], 
                                    &rPrimitiveInfos[dwEnd - 1] + 1, 
                                    [=](const BvhPrimitiveInfo& crInfo)
                                    {
                                        uint32_t dwBucketIndex = dwBucketsNum * 
                                                                CentroidBounds.offset(crInfo.Centroid)[dwDimension];
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
                                // Create leaf BvhBuildNode. 
                                uint32_t dwFirstPrimitiveOffset = rpOrderedPrimitives.size();
                                for (uint32_t ix = dwStart; ix < dwEnd; ++ix)
                                {
                                    uint32_t child_index = rPrimitiveInfos[ix].stPrimitiveIndex;
                                    rpOrderedPrimitives.push_back(m_pPrimitives[child_index]);
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
        uint32_t child_index;
        uint32_t dwMortonCode;
    };

    struct FLBVHTreelet
    {
        uint32_t dwStartIdnex;
        uint32_t dwPrimitivesNum;
        BvhBuildNode* pBuildNode;
    };

    inline uint32_t LeftShift3(uint32_t dwVal)
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

    inline uint32_t EncodeMorton3(const Vector3F& crVec)
    {
        // GE(crVec.x, 0);
        // GE(crVec.y, 0);
        // GE(crVec.z, 0);

        return (LeftShift3(crVec.z) << 2) | (LeftShift3(crVec.y) << 1) | (LeftShift3(crVec.x));
    }

    static void RadixSort(std::vector<FMortonPrimitive>* pMortonPrimitives)
    {
        std::vector<FMortonPrimitive> TempMortonPrims(pMortonPrimitives->size());
        constexpr uint32_t dwBitsPerPass = 6;
        constexpr uint32_t dwBitsNum = MORTON_BITS_NUM * 3;
        constexpr uint32_t dwPassesNum = dwBitsNum / dwBitsPerPass;

        constexpr uint32_t dwBucketsNum = 1 << dwBitsPerPass;
        constexpr uint32_t dwBitMask = (1 << dwBitsPerPass) - 1;

        for (uint32_t pass = 0; pass < dwPassesNum; ++pass)
        {
            // Perform one pass of radix sort, sorting dwBitsPerPass bits. 

            // Low bits which has been sorted.  
            uint32_t dwLowBit = pass * dwBitsPerPass;

            // Set in and out vector pointers for radix sort pass to avoid extra copying. 
            std::vector<FMortonPrimitive>& rIn = pass & 1 ? TempMortonPrims : *pMortonPrimitives;
            std::vector<FMortonPrimitive>& rOut = pass & 1 ? *pMortonPrimitives : TempMortonPrims;

            // Count number of zero bits in array for current radix sort bit. 
            uint32_t pdwBucketCount[dwBucketsNum] = {0};

            for (const auto& crMortonPrim : rIn)
            {
                uint32_t dwBucket = (crMortonPrim.dwMortonCode >> dwLowBit) & dwBitMask;
                // GE(dwBucket, 0);
                // LT(dwBucket, dwBucketsNum);
                pdwBucketCount[dwBucket]++;
            }

            // Compute starting index in output array for each bucket. 
            uint32_t pdwOutIndex[dwBucketsNum] = {0};
            for (uint32_t ix = 1; ix < dwBucketsNum; ++ix)
            {
                pdwOutIndex[ix] = pdwOutIndex[ix - 1] + pdwBucketCount[ix - 1];
            }

            // Store sorted values in output array. 
            for (const auto& crMortonPrim : rIn)
            {
                uint32_t dwBucket = (crMortonPrim.dwMortonCode >> dwLowBit) & dwBitMask;
                rOut[pdwOutIndex[dwBucket]++] = crMortonPrim;
            }
        }   
        
        // Copy final result from _tempVector_, if needed
        if (dwPassesNum & 1) 
        {
            std::swap(*pMortonPrimitives, TempMortonPrims);
        }    
    }


    BvhBuildNode* BvhAccel::HLBVHBuild(
        std::vector<BvhPrimitiveInfo>& rPrimitiveInfos,
        uint32_t* pdwTotalNodes,
        std::vector<std::shared_ptr<FTrianglePrimitive>>& rpOrderedPrimitives
    ) const
    {
        // Compute bounding box of all primitive centroids. 
        Bounds3F CentroidBounds;
        for (const auto& crInfo : rPrimitiveInfos)
        {
            CentroidBounds = merge(CentroidBounds, crInfo.Bounds);
        }

        // Compute Morton indices of primitives. 
        parallel::initialize();
        std::vector<FMortonPrimitive> MortonPrimitives(rPrimitiveInfos.size());
        parallel::parallel_for(
            [&](uint64_t ix)
            {
                // initialize MortonPrimitives[ix] for ixth primitive. 
                constexpr uint32_t dwMortonScale = 1 << MORTON_BITS_NUM;

                MortonPrimitives[ix].child_index = rPrimitiveInfos[ix].stPrimitiveIndex;
                Vector3F CentroidOffset = CentroidBounds.offset(rPrimitiveInfos[ix].Centroid);

                MortonPrimitives[ix].dwMortonCode = EncodeMorton3(CentroidOffset * dwMortonScale);
            },
            rPrimitiveInfos.size(),
            512
        );

        // Radix sort primitive Morton indices. 
        RadixSort(&MortonPrimitives);

        // Create LBVH treelets at bottom of BVH
        std::vector<FLBVHTreelet> TreeletToBuild;
        for (uint32_t dwStart = 0, dwEnd = 1; dwEnd <= MortonPrimitives.size(); ++dwEnd)
        {
            constexpr uint32_t dwMask = 0b00111111111111000000000000000000;

            if (
                dwEnd == MortonPrimitives.size() ||
                (MortonPrimitives[dwStart].dwMortonCode & dwMask) != (MortonPrimitives[dwEnd].dwMortonCode & dwMask) 
            )
            {
                // add entry to TreeletToBuild for this treelet. 
                uint32_t dwPrimitivesNum = dwEnd - dwStart;
                BvhBuildNode* pNodes = new BvhBuildNode();
                TreeletToBuild.push_back({ dwStart, dwPrimitivesNum, pNodes });
                dwStart = dwEnd;
            }
        }

        // Create LBVHs for treelets in parallel. 
        std::atomic<uint32_t> dwAtomicTotal(0), dwAtomicOrderedPrimsOffset(0);
        rpOrderedPrimitives.resize(m_pPrimitives.size());
        parallel::parallel_for(
            [&](uint64_t ix)
            {
                // Generate ix_th LBVH treelet. 
                uint32_t dwCreatedNodesNum = 0;
                constexpr uint32_t dwFirstBitIndex = 3 * MORTON_BITS_NUM - 1 - MORTON_HIGH_BITS;

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

        parallel::destroy();

        // Create and return SAH BVH from LBVH treelets. 
        std::vector<BvhBuildNode*> pFinishedTreelets; 
        pFinishedTreelets.reserve(TreeletToBuild.size());
        for (auto& Treelet : TreeletToBuild)
        {
            pFinishedTreelets.push_back(Treelet.pBuildNode);
        }
        return BuildUpperSAH(pFinishedTreelets, 0, pFinishedTreelets.size(), pdwTotalNodes);
    }

    BvhBuildNode* BvhAccel::EmitLBVH(
        BvhBuildNode*& rpBuildNodes,
        const std::vector<BvhPrimitiveInfo>& crPrimitiveInfos,
        FMortonPrimitive* pMortonPrimitives,
        uint32_t dwPrimitivesNum,
        uint32_t* pdwTotalNodes,
        std::vector<std::shared_ptr<FTrianglePrimitive>>& rpOrderedPrimitives,
        std::atomic<uint32_t>* pdwAtomicOrderedPrimsOffset,
        uint32_t bit_index
    ) const
    {
        // For each of the remaining bits in the Morton codes, 
        // this function tries to split the primitives along the plane corresponding to the bit bitIndex, 
        // then calls itself recursively. 

        // GT(dwPrimitivesNum, 0);

        if (bit_index == -1 || dwPrimitivesNum < m_dwMaxPrimitivesInNode)
        {
            // Create and return leaf node of LBVH treelet. 
            (*pdwTotalNodes)++;
            BvhBuildNode* pNode = rpBuildNodes++;
            Bounds3F Bounds;
            uint32_t dwFirstPrimitiveOffset = pdwAtomicOrderedPrimsOffset->fetch_add(dwPrimitivesNum);
            for (uint32_t ix = 0; ix < dwPrimitivesNum; ++ix)
            {
                uint32_t child_index = pMortonPrimitives[ix].child_index;
                rpOrderedPrimitives[dwFirstPrimitiveOffset + ix] = m_pPrimitives[child_index];
                Bounds = merge(Bounds, crPrimitiveInfos[child_index].Bounds);
            }
            pNode->InitLeaf(dwFirstPrimitiveOffset, dwPrimitivesNum, Bounds);
            return pNode;
        }
        else 
        {
            uint32_t dwMask = 1 << bit_index;

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
                    bit_index - 1
                );
            }

            // Use binary search to find LBVH split point for this dimension. 
            uint32_t dwSearchStart = 0, dwSearchEnd = dwPrimitivesNum - 1;
            while (dwSearchStart + 1 != dwSearchEnd)
            {
                // NE(dwSearchStart, dwSearchEnd);
                uint32_t dwMid = (dwSearchStart + dwSearchEnd) / 2;
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

            uint32_t dwSplitOffset = dwSearchEnd;
            // LE(dwSplitOffset, dwPrimitivesNum - 1);
            // NE(pMortonPrimitives[dwSplitOffset - 1].dwMortonCode & dwMask, pMortonPrimitives[dwSplitOffset].dwMortonCode & dwMask);

            // Create and return interior LBVH node. 
            (*pdwTotalNodes)++;
            BvhBuildNode* pNode = rpBuildNodes++;
            BvhBuildNode* pChildren[2] = {
                EmitLBVH(
                    rpBuildNodes, 
                    crPrimitiveInfos, 
                    pMortonPrimitives, 
                    dwSplitOffset, 
                    pdwTotalNodes, 
                    rpOrderedPrimitives, 
                    pdwAtomicOrderedPrimsOffset, 
                    bit_index - 1
                ),
                EmitLBVH(
                    rpBuildNodes, 
                    crPrimitiveInfos, 
                    &pMortonPrimitives[dwSplitOffset], 
                    dwPrimitivesNum - dwSplitOffset, 
                    pdwTotalNodes, 
                    rpOrderedPrimitives, 
                    pdwAtomicOrderedPrimsOffset, 
                    bit_index - 1
                )
            };

            uint32_t dwAxis = bit_index % 3;
            pNode->InitInterior(dwAxis, pChildren[0], pChildren[1]);

            return pNode;
        }
    }

    BvhBuildNode* BvhAccel::BuildUpperSAH(
        std::vector<BvhBuildNode*>& rpTreeletRoots,
        uint32_t dwStart,
        uint32_t dwEnd,
        uint32_t* pdwTotalNodes
    ) const
    {
        // LT(dwStart, dwEnd);
        uint32_t dwNodesNum = dwStart - dwEnd;
        if (dwNodesNum == 1) return rpTreeletRoots[dwStart];

        (*pdwTotalNodes)++;
        BvhBuildNode* pNode = new BvhBuildNode();

        // Compute bounds of all nodes under this HLBVH node. 
        Bounds3F GlobalBounds;
        for (uint32_t ix = dwStart; ix < dwEnd; ++ix)
        {
            GlobalBounds = merge(GlobalBounds, rpTreeletRoots[ix]->Bounds);
        }

        // Compute bound of HLBVH node centroids, choose split dimension _dim_
        Bounds3F CentroidBounds;
        for (uint32_t ix = dwStart; ix < dwEnd; ++ix)
        {
            Vector3F Centroid((rpTreeletRoots[ix]->Bounds._lower + rpTreeletRoots[ix]->Bounds._upper) * 0.5f);
            CentroidBounds = merge(CentroidBounds, Centroid);
        }

        uint32_t dwDimension = CentroidBounds.max_axis();
        // NE(CentroidBounds._upper[dwDimension], CentroidBounds._lower[dwDimension]);

        // allocate _BucketInfo_ for SAH partition buckets. 
        struct FBucketInfo
        {
            uint32_t dwPrimitivesCount = 0;
            Bounds3F Bounds;
        };

        // allocate FBucketInfo for SAH partition buckets. 
        constexpr uint32_t dwBucketsNum = 12;
        FBucketInfo Buckets[dwBucketsNum];

        for (uint32_t ix = dwStart; ix < dwEnd; ++ix)
        {
            float fCentroid = (rpTreeletRoots[ix]->Bounds._lower[dwDimension] + rpTreeletRoots[ix]->Bounds._upper[dwDimension]) * 0.5f;

            uint32_t dwBucketIndex = dwBucketsNum * 
                                   ((fCentroid - CentroidBounds._lower[dwDimension]) / 
                                   (CentroidBounds._upper[dwDimension] - CentroidBounds._lower[dwDimension]));

            if (dwBucketIndex == dwBucketsNum) dwBucketIndex -= 1;
            // GE(dwBucketIndex, 0);
            // LT(dwBucketIndex, dwBucketsNum);

            Buckets[dwBucketIndex].dwPrimitivesCount++;
            Buckets[dwBucketIndex].Bounds = merge(Buckets[dwBucketIndex].Bounds, rpTreeletRoots[ix]->Bounds);
        }

        // Compute costs for splitting after each bucket. 
        float fBucketCost[dwBucketsNum - 1];
        for (uint32_t i = 0; i < dwBucketsNum - 1; ++i)
        {
            Bounds3F b0, b1;
            uint32_t dwCount0 = 0, dwCount1 = 0;
            for (uint32_t j = 0; j <= i; ++j)
            {
                b0 = merge(b0, Buckets[j].Bounds);
                dwCount0 += Buckets[j].dwPrimitivesCount;
            }
            for (uint32_t j = i + 1; j < dwBucketsNum; ++j)
            {
                b1 = merge(b1, Buckets[j].Bounds);
                dwCount1 += Buckets[j].dwPrimitivesCount;
            }
            fBucketCost[i] = 0.125f + (dwCount0 * b0.surface_area() + dwCount1 * b1.surface_area()) / GlobalBounds.surface_area();
        }

        // Find minimum bucket cost to split at that minimizes SAH metric. 
        float fMinBucketCost = fBucketCost[0];
        uint32_t dwMinCostSplitBucketIndex = 0;
        for (uint32_t ix = 1; ix < dwBucketsNum - 1; ++ix)
        {
            if (fBucketCost[ix] < fMinBucketCost)
            {
                fMinBucketCost = fBucketCost[ix];
                dwMinCostSplitBucketIndex = ix;
            }
        }

        // Split nodes and create interior HLBVH SAH node. 
        BvhBuildNode** ppMidNode = std::partition(
            &rpTreeletRoots[dwStart], 
            &rpTreeletRoots[dwEnd - 1] + 1, 
            [=](const BvhBuildNode* cpNode)
            {
                float fCentroid = (cpNode->Bounds._lower[dwDimension] + cpNode->Bounds._upper[dwDimension]) * 0.5f;

                uint32_t dwBucketIndex = dwBucketsNum *
                                   ((fCentroid - CentroidBounds._lower[dwDimension]) / 
                                   (CentroidBounds._upper[dwDimension] - CentroidBounds._lower[dwDimension]));
                if (dwBucketIndex == dwBucketsNum) dwBucketIndex -= 1;
                // GE(dwBucketIndex, 0);
                // LT(dwBucketIndex, dwBucketsNum);
                return dwBucketIndex <= dwMinCostSplitBucketIndex;
            } 
        );
        uint32_t dwMid = ppMidNode - &rpTreeletRoots[0];
        // GT(dwMid, dwStart);
        // LT(dwMid, dwEnd);

        pNode->InitInterior(
            dwDimension,
            BuildUpperSAH(rpTreeletRoots, dwStart, dwMid, pdwTotalNodes), 
            BuildUpperSAH(rpTreeletRoots, dwMid, dwEnd, pdwTotalNodes)
        );

        return pNode;
    }

    uint32_t BvhAccel::FlattenBvhTree(BvhBuildNode* pNode, uint32_t* pdwOffset)
    {
        LinearBvhNode* pLinearNode = &_nodes[*pdwOffset];
        pLinearNode->Bounds = pNode->Bounds;
        
        uint32_t dwPartOffset = *pdwOffset;
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