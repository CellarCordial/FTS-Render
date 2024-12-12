#include "../include/Geometry.h"
#include <cstring>
#include <utility>

namespace FTS
{
	void FMeshOptimizer::BinaryHeap::Resize(UINT32 dwIndexNum)
	{
		m_dwCurrSize = 0;
		m_dwIndexNum = dwIndexNum;
		m_Heap.clear(); m_Heap.resize(dwIndexNum);
		m_Keys.clear(); m_Keys.resize(dwIndexNum);
		m_HeapIndices.clear(); m_HeapIndices.resize(dwIndexNum);

		memset(m_HeapIndices.data(), 0xff, dwIndexNum * sizeof(UINT32));
	}
	
	FLOAT FMeshOptimizer::BinaryHeap::GetKey(UINT32 dwIndex)
	{
		return m_Keys[dwIndex];
	}
	
	BOOL FMeshOptimizer::BinaryHeap::Empty()
	{
		return m_dwCurrSize == 0;
	}
	
	BOOL FMeshOptimizer::BinaryHeap::IsValid(UINT32 dwIndex)
	{
		return m_HeapIndices[dwIndex] != INVALID_SIZE_32;
	}
	
	UINT32 FMeshOptimizer::BinaryHeap::Top()
	{
		assert(m_dwCurrSize > 0);
		return m_Heap[0];
	}
	
	void FMeshOptimizer::BinaryHeap::Pop()
	{
		assert(m_dwCurrSize > 0);
		UINT32 dwIndex = m_Heap[0];
		m_Heap[0] = m_Heap[--m_dwCurrSize];
		m_HeapIndices[m_Heap[0]] = 0;
		m_HeapIndices[dwIndex] = INVALID_SIZE_32;
		PushDown(0);
	}
	
	void FMeshOptimizer::BinaryHeap::Remove(UINT32 dwIndex)
	{
		assert(IsInvalid(dwIndex));

		FLOAT fKey = m_Keys[dwIndex];
		UINT32& rdwIndex = m_HeapIndices[dwIndex];
		if (rdwIndex == m_dwCurrSize - 1)
		{
			m_dwCurrSize--;
			rdwIndex = INVALID_SIZE_32;
			return;
		}

		m_Heap[rdwIndex] = m_Heap[--m_dwCurrSize];
		m_HeapIndices[m_Heap[rdwIndex]] = rdwIndex;
		m_HeapIndices[dwIndex] = INVALID_SIZE_32;

		if (fKey < m_Keys[m_Heap[rdwIndex]]) PushDown(rdwIndex);
		else PushUp(rdwIndex);
	}
	
	void FMeshOptimizer::BinaryHeap::Insert(FLOAT fKey,UINT32 dwIndex)
	{
		assert(IsInvalid(dwIndex));

		m_dwCurrSize++;
		m_Heap[m_dwCurrSize] = dwIndex;
		m_Keys[dwIndex] = fKey;
		m_HeapIndices[dwIndex] = m_dwCurrSize;
		PushUp(m_dwCurrSize);
	}

	void FMeshOptimizer::BinaryHeap::PushDown(UINT32 dwIndex)
	{
		UINT32 ix=m_Heap[dwIndex];
		UINT32 dwFatherIndex=(dwIndex-1)>>1;
		while (dwIndex > 0 && m_Keys[ix] < m_Keys[m_Heap[dwFatherIndex]])
		{
			m_Heap[dwIndex] = m_Heap[dwFatherIndex];
			m_HeapIndices[m_Heap[dwIndex]] = dwIndex;
			dwIndex = dwFatherIndex;
			dwFatherIndex = (dwIndex-1) >> 1;
		}
		m_Heap[dwIndex] = ix;
		m_HeapIndices[m_Heap[dwIndex]] = dwIndex;
	}

	void FMeshOptimizer::BinaryHeap::PushUp(UINT32 dwIndex)
	{
		UINT32 ix = m_Heap[dwIndex];
		UINT32 dwLeftIndex = (dwIndex << 1) + 1;
		UINT32 dwRightIndex = dwLeftIndex + 1;
		while(dwLeftIndex < m_dwCurrSize)
		{
			UINT32 dwTmp = dwLeftIndex;
			if(dwRightIndex < m_dwCurrSize && m_Keys[m_Heap[dwRightIndex]] < m_Keys[m_Heap[dwLeftIndex]]) dwTmp=dwRightIndex;
			if(m_Keys[m_Heap[dwTmp]] < m_Keys[ix])
			{
				m_Heap[dwIndex] = m_Heap[dwTmp];
				m_HeapIndices[m_Heap[dwIndex]] = dwIndex;
				dwIndex = dwTmp;
				dwLeftIndex = (dwIndex << 1) + 1;
				dwRightIndex = dwLeftIndex + 1;
			}
			else 
			{
				break;
			}
		}
		m_Heap[dwIndex] = ix;
		m_HeapIndices[m_Heap[dwIndex]] = dwIndex;
	}

	FMeshOptimizer::FMeshOptimizer(const std::span<FVector3F>& crVertices, const std::span<UINT32>& crIndices) :
		m_Vertices(crVertices), 
		m_Indices(crIndices),
		m_VertexReferenceCount(crVertices.size()),
		m_VertexTable(crVertices.size()),
		m_IndexTable(crIndices.size()),
		m_Flags(crIndices.size()),
		m_TriangleSurfaces(crIndices.size() / 3.0f),
		m_bTrangleRemovedArray(crIndices.size() / 3.0f, false)
	{
		dwRemainVertexNum = static_cast<UINT32>(m_Vertices.size());
		dwRemainTriangleNum = static_cast<UINT32>(m_TriangleSurfaces.size());

		for (UINT32 ix = 0; ix < m_Vertices.size(); ++ix)
		{
			m_VertexTable.Insert(Hash(m_Vertices[ix]), ix);
		}

		UINT64 stEdgeNum = std::min(std::min(m_Indices.size(), 3 * m_Vertices.size() - 6), m_TriangleSurfaces.size() + m_Vertices.size());
		m_Edges.resize(stEdgeNum);
		m_EdgeBeginTable.Resize(stEdgeNum);
		m_EdgeEndTable.Resize(stEdgeNum);

		for (UINT32 dwCorner = 0; dwCorner < m_Indices.size(); ++dwCorner)
		{
			m_VertexReferenceCount[m_Indices[dwCorner]]++;
			const auto& crVertex = m_Vertices[m_Indices[dwCorner]];

			m_IndexTable.Insert(Hash(crVertex), dwCorner);
			
			FVector3F p0 = crVertex;
			FVector3F p1 = m_Vertices[m_Indices[TriangleIndexCycle3(dwCorner)]];

			UINT32 h0 = Hash(p0);
			UINT32 h1 = Hash(p1);
			if (h0 > h1)
			{
				std::swap(h0, h1);
				std::swap(p0, p1);
			}

			BOOL bAlreadyExist = false;
			for (UINT32 ix : m_EdgeBeginTable[h0])
			{
				const auto& crEdge = m_Edges[ix];
				if (crEdge.first == p0 && crEdge.second == p1) 
				{
					bAlreadyExist = true;
					break;
				}
			}
			if (!bAlreadyExist)
			{
				m_EdgeBeginTable.Insert(h0, static_cast<UINT32>(m_Edges.size()));
				m_EdgeEndTable.Insert(h1, static_cast<UINT32>(m_Edges.size()));
				m_Edges.emplace_back(std::make_pair(p0, p1));
			}
		}
	}


	BOOL FMeshOptimizer::Optimize(UINT32 dwTargetTriangleNum)
	{
		for (UINT32 ix = 0; ix < m_TriangleSurfaces.size(); ++ix) FixTriangle(ix);

		if (dwRemainTriangleNum <= dwTargetTriangleNum) return Compact();

		m_Heap.Resize(m_Edges.size());

		UINT32 ix = 0;
		for (const auto& crEdge : m_Edges)
		{
			FLOAT fError = Evaluate(crEdge.first, crEdge.second, false);
			m_Heap.Insert(fError, ix++);
		}

		fMaxError = 0.0f;
		while (!m_Heap.Empty())
		{
			UINT32 dwEdgeIndex = m_Heap.Top();
			if (m_Heap.GetKey(dwEdgeIndex) >= 1e6) break;

			m_Heap.Pop();
			
			const auto& crEdge = m_Edges[dwEdgeIndex];
			m_EdgeBeginTable.Remove(Hash(crEdge.first), dwEdgeIndex);
			m_EdgeEndTable.Remove(Hash(crEdge.second), dwEdgeIndex);

			fMaxError = std::max(Evaluate(crEdge.first, crEdge.second, true), fMaxError);

			if (dwRemainTriangleNum <= dwTargetTriangleNum) break;

			for (UINT32 ix : m_EdgeNeedReevaluateIndices)
			{
				const auto& crReevaluateEdge = m_Edges[ix];
				FLOAT fError = Evaluate(crReevaluateEdge.first, crReevaluateEdge.second, false);
				m_Heap.Insert(fError, ix++);
			}

			m_EdgeNeedReevaluateIndices.clear();
		}
		return Compact();
	}

	void FMeshOptimizer::LockPosition(FVector3F Pos)
	{
		for (UINT32 ix : m_IndexTable[Hash(Pos)])
		{
			if (m_Vertices[m_Indices[ix]] == Pos)
			{
				m_Flags[ix] |= LockFlag;
			}
		}
	}


	UINT32 FMeshOptimizer::Hash(const FVector3F& Vec)
	{
		union 
		{
			FLOAT f;
			UINT32 u;
		} x, y, z;

		x.f = (Vec.x == 0.0f ? 0 : Vec.x);
		y.f = (Vec.y == 0.0f ? 0 : Vec.y);
		z.f = (Vec.z == 0.0f ? 0 : Vec.z);
		return MurmurMix(MurmurAdd(MurmurAdd(x.u, y.u), z.u));
	}

	void FMeshOptimizer::FixTriangle(UINT32 dwTriangleIndex)
	{
		if (m_bTrangleRemovedArray[dwTriangleIndex]) return;

		UINT32 dwVertexIndex0 = m_Indices[dwTriangleIndex * 3 + 0];
		UINT32 dwVertexIndex1 = m_Indices[dwTriangleIndex * 3 + 1];
		UINT32 dwVertexIndex2 = m_Indices[dwTriangleIndex * 3 + 2];

		const FVector3F& p0 = m_Vertices[dwVertexIndex0];	
		const FVector3F& p1 = m_Vertices[dwVertexIndex1];	
		const FVector3F& p2 = m_Vertices[dwVertexIndex2];

		BOOL bNeedRemoved = p0 == p1 || p1 == p2 || p2 == p0;
		if (!bNeedRemoved)
		{
			for (UINT32 ix = 0; ix < 3; ++ix)
			{
				// 去除重复的 Vertex
				UINT32& rdwVertexIndex = m_Indices[dwTriangleIndex * 3 + ix];
				const auto& crVertex = m_Vertices[rdwVertexIndex];
				UINT32 dwHash = Hash(crVertex);
				for (UINT32 jx : m_VertexTable[dwHash])
				{
					if (jx == rdwVertexIndex)
					{
						break;
					}
					else if (crVertex == m_Vertices[jx]) 
					{
						assert(m_VertexReferenceCount[rdwVertexIndex] > 0);
						if (--m_VertexReferenceCount[rdwVertexIndex] == 0)
						{
							m_VertexTable.Remove(dwHash, rdwVertexIndex);
							dwRemainVertexNum--;
						}
						rdwVertexIndex = jx;
						if (rdwVertexIndex != INVALID_SIZE_32) m_VertexReferenceCount[rdwVertexIndex]++;
						break;
					}
				}
			}
			
			// 判断 Triangle 是否重复
			for (UINT32 ix : m_IndexTable[Hash(p0)])
			{
				if (
					ix != dwTriangleIndex * 3 &&
					dwVertexIndex0 == m_Indices[ix] && 
					dwVertexIndex1 == m_Indices[TriangleIndexCycle3(ix)] && 
					dwVertexIndex2 == m_Indices[TriangleIndexCycle3(ix, 2)] 
				)
				{
					bNeedRemoved = true;
				}
			}
		}

		if (bNeedRemoved)
		{
			m_bTrangleRemovedArray.SetTrue(dwTriangleIndex);
			dwRemainTriangleNum--;

			for (UINT32 ix = 0; ix < 3; ++ix)
			{
				UINT32& rdwVertexIndex = m_Indices[dwTriangleIndex * 3 + ix];
				const auto& crVertex = m_Vertices[rdwVertexIndex];

				UINT32 dwHash = Hash(crVertex);
				m_IndexTable.Remove(dwHash, rdwVertexIndex);

				assert(m_VertexReferenceCount[rdwVertexIndex] > 0);
				if (--m_VertexReferenceCount[rdwVertexIndex] == 0)
				{
					m_VertexTable.Remove(dwHash, rdwVertexIndex);
					dwRemainVertexNum--;
				}
				rdwVertexIndex = INVALID_SIZE_32;
			}
		}
		else 
		{
			m_TriangleSurfaces[dwTriangleIndex] = FQuadricSurface(p0, p1, p2);
		}
	}

	BOOL FMeshOptimizer::Compact()
	{
		UINT32 dwCount = 0;
		std::vector<UINT32> IndicesMap(m_Vertices.size());
		for (UINT32 ix = 0; ix < m_Vertices.size(); ++ix)
		{
			if (m_VertexReferenceCount[ix] > 0)
			{
				if (ix != dwCount) m_Vertices[dwCount] = m_Vertices[ix];
				IndicesMap[ix] = dwCount++;
			}
		}
		ReturnIfFalse(dwCount == dwRemainVertexNum);

		dwCount = 0;
		for (UINT32 ix = 0; ix < m_TriangleSurfaces.size(); ++ix)
		{
			if (!m_bTrangleRemovedArray[ix])
			{
				for (UINT32 jx = 0; jx < 3; ++jx)
				{
					m_Indices[dwCount * 3 + jx] = IndicesMap[m_Indices[ix * 3 + jx]];
				}
				dwCount++;
			}
		}
		return dwCount == dwRemainTriangleNum;
	}

	FLOAT FMeshOptimizer::Evaluate(const FVector3F& p0, const FVector3F& p1, BOOL bMerge)
	{
		if (p0 == p1) return 0.0f;

		FLOAT fError;

		std::vector<UINT32> AdjacencyTriangleIndices;
		auto FuncBuildAdjacencyTrianlges = [this, &AdjacencyTriangleIndices](const FVector3F& crVertex, BOOL& rbLock)
		{
			for (UINT32 ix : m_IndexTable[Hash(crVertex)])
			{
				if (m_Vertices[m_Indices[ix]] == crVertex)
				{
					UINT32 dwTriangleIndex = ix / 3;
					if ((m_Flags[dwTriangleIndex * 3] & AdjacencyFlag) == 0)
					{
						m_Flags[dwTriangleIndex * 3] |= AdjacencyFlag;
						AdjacencyTriangleIndices.push_back(dwTriangleIndex);
					}

					if (m_Flags[ix] & LockFlag) rbLock = true;
				}
			}
		};

		BOOL bLock0 = false, bLock1 = false;
		FuncBuildAdjacencyTrianlges(p0, bLock0);
		FuncBuildAdjacencyTrianlges(p1, bLock1);

		if (AdjacencyTriangleIndices.empty()) return 0.0f;
		if (AdjacencyTriangleIndices.size() > 24) fError += 0.5f * (AdjacencyTriangleIndices.size() - 24);

		FQuadricSurface Surface;
		for (UINT32 ix : AdjacencyTriangleIndices) Surface = Union(Surface, m_TriangleSurfaces[ix]);

		FVector3F p = (p0 + p1) * 0.5f;
		if (bLock0 && bLock1) fError += 1e8;
		else if (bLock0 && !bLock1) p = p0;
		else if (!bLock0 && bLock1) p = p1;
		else if (
			!Surface.GetVertexPos(p) || 
			Distance(p, p0) + Distance(p, p1) > 2.0f * Distance(p0, p1)	// Invalid
		)
		{
			p = (p0 + p1) * 0.5f;
		}

		fError += Surface.DistanceToSurface(p);

		if (bMerge)
		{
			MergeBegin(p0);
			MergeBegin(p1);

			for (UINT32 ix : AdjacencyTriangleIndices)
			{
				for (UINT32 jx = 0; jx < 3; ++jx)
				{
					UINT32 dwVertexIndex = ix * 3 + jx;
					FVector3F& rPos = m_Vertices[m_Indices[dwVertexIndex]];
					if (rPos == p0 || rPos == p1)
					{
						rPos = p;
						if (bLock0 || bLock1)
						{
							m_Flags[dwVertexIndex] |= LockFlag;
						}
					}
				}
			}
			for (UINT32 ix : m_MovedEdgeIndices)
			{
				auto& rEdge = m_Edges[ix];
				if (rEdge.first == p0 || rEdge.first == p1) rEdge.first = p;
				if (rEdge.second == p0 || rEdge.second == p1) rEdge.second = p;
			}

			MergeEnd();

			std::vector<UINT32> AdjacencyVertexIndices;
			AdjacencyVertexIndices.reserve((AdjacencyTriangleIndices.size() * 3));
			for (UINT32 ix : AdjacencyTriangleIndices)
			{
				for (UINT32 jx = 0; jx < 3; ++jx)
				{
					AdjacencyVertexIndices.push_back(m_Indices[ix * 3 + jx]);
				}
			}
			std::sort(AdjacencyVertexIndices.begin(), AdjacencyVertexIndices.end());
			AdjacencyVertexIndices.erase(
				std::unique(AdjacencyVertexIndices.begin(), AdjacencyVertexIndices.end()), 
				AdjacencyVertexIndices.end()
			);

			for (UINT32 dwVertexIndex : AdjacencyVertexIndices)
			{
				const auto& crVertex = m_Vertices[dwVertexIndex];
				UINT32 dwHash = Hash(crVertex);
				for (UINT32 ix : m_EdgeBeginTable[dwHash])
				{
					if (m_Edges[ix].first == crVertex && m_Heap.IsValid(ix))
					{
						m_Heap.Remove(ix);
						m_EdgeNeedReevaluateIndices.push_back(ix);
					}
				}
				for (UINT32 ix : m_EdgeEndTable[dwHash])
				{
					if (m_Edges[ix].second == crVertex && m_Heap.IsValid(ix))
					{
						m_Heap.Remove(ix);
						m_EdgeNeedReevaluateIndices.push_back(ix);
					}
				}
			}
			
			for (UINT32 dwTriangleIndex : AdjacencyTriangleIndices) FixTriangle(dwTriangleIndex);
		}	

		for (UINT32 dwTriangleIndex : AdjacencyTriangleIndices) m_Flags[dwTriangleIndex * 3] &= ~AdjacencyFlag;

		return fError;
	}

	void FMeshOptimizer::MergeBegin(const FVector3F& p)
	{
		UINT32 dwHash = Hash(p);
		for (UINT32 ix : m_VertexTable[dwHash])
		{
			if (m_Vertices[ix] == p)
			{
				m_VertexTable.Remove(dwHash, ix);
				m_MovedVertexIndices.push_back(ix);
			}
		}
		for (UINT32 ix : m_IndexTable[dwHash])
		{
			if (m_Vertices[m_Indices[ix]] == p)
			{
				m_IndexTable.Remove(dwHash, ix);
				m_MovedIndexIndices.push_back(ix);
			}
		}
		for (UINT32 ix : m_EdgeBeginTable[dwHash])
		{
			if (m_Edges[ix].first == p)
			{
				m_EdgeBeginTable.Remove(dwHash, ix);
				m_MovedEdgeIndices.push_back(ix);
			}
		}
		for (UINT32 ix : m_EdgeEndTable[dwHash])
		{
			if (m_Edges[ix].second == p)
			{
				m_EdgeEndTable.Remove(dwHash, ix);
				m_MovedEdgeIndices.push_back(ix);
			}
		}
	}

	void FMeshOptimizer::MergeEnd()
	{
		for (UINT32 ix : m_MovedVertexIndices)
		{
			m_VertexTable.Insert(Hash(m_Vertices[ix]), ix);
		}
		for (UINT32 ix : m_MovedIndexIndices)
		{
			m_IndexTable.Insert(Hash(m_Vertices[m_Indices[ix]]), ix);
		}
		for (UINT32 ix : m_MovedEdgeIndices)
		{
			auto& rEdge = m_Edges[ix];
			
			UINT32 h0 = Hash(rEdge.first);
			UINT32 h1 = Hash(rEdge.second);
			if (h0 > h1)
			{
				std::swap(h0, h1);
				std::swap(rEdge.first, rEdge.second);
			}

			BOOL bAlreadyExist = false;
			for (UINT32 ix : m_EdgeBeginTable[h0])
			{
				const auto& crEdge = m_Edges[ix];
				if (crEdge.first == crEdge.first && crEdge.second == crEdge.second) 
				{
					bAlreadyExist = true;
					break;
				}
			}
			if (!bAlreadyExist)
			{
				m_EdgeBeginTable.Insert(h0, static_cast<UINT32>(m_Edges.size()));
				m_EdgeEndTable.Insert(h1, static_cast<UINT32>(m_Edges.size()));
			}

			if (rEdge.first == rEdge.second && bAlreadyExist)
			{
				m_Heap.Remove(ix);
			}
		}
		
		m_MovedEdgeIndices.clear();
		m_MovedVertexIndices.clear();
		m_MovedIndexIndices.clear();
	}


	namespace Cluster
    {
		struct Graph
		{
			std::vector<std::map<UINT32,INT32>> g;

			void init(UINT32 n)
			{
				g.resize(n);
			}
			void add_node()
			{
				g.push_back({});
			}
			void add_edge(UINT32 from,UINT32 to,INT32 cost)
			{
				g[from][to]=cost;
			}
			void increase_edge_cost(UINT32 from,UINT32 to,INT32 i_cost)
			{
				g[from][to]+=i_cost;
			}
		};

		struct MetisGraph;

		class Partitioner
		{
		private:
			UINT32 bisect_graph(MetisGraph* graph_data,MetisGraph* child_graphs[2],UINT32 start,UINT32 end);
			void recursive_bisect_graph(MetisGraph* graph_data,UINT32 start,UINT32 end);
		public:
			void init(UINT32 num_node);
			void partition(const Graph& graph,UINT32 min_part_size,UINT32 max_part_size);

			std::vector<UINT32> node_id;
			std::vector<std::pair<UINT32,UINT32>> ranges;
			std::vector<UINT32> sort_to;
			UINT32 min_part_size;
			UINT32 max_part_size;
		};


        void CreateTriangleCluster(
            const std::vector<FVector3F>& verts,
            const std::vector<UINT32>& indexes,
            std::vector<FCluster>& clusters
        )
		{

		}

        void BuildClusterGroups(
            std::vector<FCluster>& clusters,
            UINT32 offset,
            UINT32 num_cluster,
            std::vector<FClusterGroup>& cluster_groups,
            UINT32 mip_level
        )
		{

		}

        void BuildClusterGroupParentClusters(
            FClusterGroup& cluster_group,
            std::vector<FCluster>& clusters
        )
		{

		}

    };


	namespace Geometry
	{
		FMesh CreateBox(FLOAT width, FLOAT height, FLOAT depth, UINT32 numSubdivisions)
		{
			FMesh Mesh;
			auto& rSubmesh = Mesh.Submeshes.emplace_back();

			FVertex v[24];

			FLOAT w2 = 0.5f * width;
			FLOAT h2 = 0.5f * height;
			FLOAT d2 = 0.5f * depth;

			v[0] = FVertex({ -w2, -h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[1] = FVertex({ -w2, +h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[2] = FVertex({ +w2, +h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[3] = FVertex({ +w2, -h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });
			v[4] = FVertex({ -w2, -h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });
			v[5] = FVertex({ +w2, -h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[6] = FVertex({ +w2, +h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[7] = FVertex({ -w2, +h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[8] = FVertex({ -w2, +h2, -d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[9] = FVertex({ -w2, +h2, +d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[10] = FVertex({ +w2, +h2, +d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[11] = FVertex({ +w2, +h2, -d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });
			v[12] = FVertex({ -w2, -h2, -d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });
			v[13] = FVertex({ +w2, -h2, -d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[14] = FVertex({ +w2, -h2, +d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[15] = FVertex({ -w2, -h2, +d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[16] = FVertex({ -w2, -h2, +d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 0.0f, 1.0f });
			v[17] = FVertex({ -w2, +h2, +d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 0.0f, 0.0f });
			v[18] = FVertex({ -w2, +h2, -d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 1.0f, 0.0f });
			v[19] = FVertex({ -w2, -h2, -d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 1.0f, 1.0f });
			v[20] = FVertex({ +w2, -h2, -d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 1.0f });
			v[21] = FVertex({ +w2, +h2, -d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f });
			v[22] = FVertex({ +w2, +h2, +d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f });
			v[23] = FVertex({ +w2, -h2, +d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f });

			rSubmesh.Vertices.assign(&v[0], &v[24]);

			UINT16 i[36];

			// Fill in the front face index data
			i[0] = 0; i[1] = 1; i[2] = 2;
			i[3] = 0; i[4] = 2; i[5] = 3;

			// Fill in the back face index data
			i[6] = 4; i[7] = 5; i[8] = 6;
			i[9] = 4; i[10] = 6; i[11] = 7;

			// Fill in the top face index data
			i[12] = 8; i[13] = 9; i[14] = 10;
			i[15] = 8; i[16] = 10; i[17] = 11;

			// Fill in the bottom face index data
			i[18] = 12; i[19] = 13; i[20] = 14;
			i[21] = 12; i[22] = 14; i[23] = 15;

			// Fill in the left face index data
			i[24] = 16; i[25] = 17; i[26] = 18;
			i[27] = 16; i[28] = 18; i[29] = 19;

			// Fill in the right face index data
			i[30] = 20; i[31] = 21; i[32] = 22;
			i[33] = 20; i[34] = 22; i[35] = 23;

			rSubmesh.Indices.assign(&i[0], &i[36]);

			// Put a cap on the number of subdivisions.
			numSubdivisions = std::min<UINT32>(numSubdivisions, 6u);

			for (UINT32 i = 0; i < numSubdivisions; ++i) Subdivide(rSubmesh);

			return Mesh;
		}

		void Subdivide(FMesh::Submesh& meshData)
		{
			FMesh::Submesh inputCopy = meshData;


			meshData.Vertices.clear();
			meshData.Indices.clear();

			//       v1
			//       *
			//      / \
            //     /   \
            //  m0*-----*m1
			//   / \   / \
            //  /   \ /   \
            // *-----*-----*
			// v0    m2     v2

			UINT32 numTris = (UINT32)inputCopy.Indices.size() / 3;
			for (UINT32 i = 0; i < numTris; ++i)
			{
				FVertex v0 = inputCopy.Vertices[inputCopy.Indices[i * 3 + 0]];
				FVertex v1 = inputCopy.Vertices[inputCopy.Indices[i * 3 + 1]];
				FVertex v2 = inputCopy.Vertices[inputCopy.Indices[i * 3 + 2]];

				//
				// Generate the midpoints.
				//

				FVertex m0 = MidPoint(v0, v1);
				FVertex m1 = MidPoint(v1, v2);
				FVertex m2 = MidPoint(v0, v2);

				//
				// Add new geometry.
				//

				meshData.Vertices.push_back(v0); // 0
				meshData.Vertices.push_back(v1); // 1
				meshData.Vertices.push_back(v2); // 2
				meshData.Vertices.push_back(m0); // 3
				meshData.Vertices.push_back(m1); // 4
				meshData.Vertices.push_back(m2); // 5

				meshData.Indices.push_back(i * 6 + 0);
				meshData.Indices.push_back(i * 6 + 3);
				meshData.Indices.push_back(i * 6 + 5);

				meshData.Indices.push_back(i * 6 + 3);
				meshData.Indices.push_back(i * 6 + 4);
				meshData.Indices.push_back(i * 6 + 5);

				meshData.Indices.push_back(i * 6 + 5);
				meshData.Indices.push_back(i * 6 + 4);
				meshData.Indices.push_back(i * 6 + 2);

				meshData.Indices.push_back(i * 6 + 3);
				meshData.Indices.push_back(i * 6 + 1);
				meshData.Indices.push_back(i * 6 + 4);
			}
		}

		FVertex MidPoint(const FVertex& v0, const FVertex& v1)
		{
			FVector3F pos = 0.5f * (v0.Position + v1.Position);
			FVector3F normal = Normalize(0.5f * (v0.Normal + v1.Normal));
			FVector4F tangent = Normalize(0.5f * (v0.Tangent + v1.Tangent));
			FVector2F tex = 0.5f * (v0.UV + v1.UV);

			FVertex v;
			v.Position = pos;
			v.Normal = normal;
			v.Tangent = tangent;
			v.UV = tex;

			return v;
		}


		FMesh CreateSphere(FLOAT radius, UINT32 sliceCount, UINT32 stackCount)
		{
			FMesh Mesh;
			auto& rSubmesh = Mesh.Submeshes.emplace_back();
			//
			// Compute the vertices stating at the top pole and moving down the stacks.
			//

			// Poles: note that there will be texture coordinate distortion as there is
			// not a unique point on the texture map to assign to the pole when mapping
			// a rectangular texture onto a sphere.
			FVertex topVertex({ 0.0f, +radius, 0.0f }, { 0.0f, +1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			FVertex bottomVertex({ 0.0f, -radius, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });

			rSubmesh.Vertices.push_back(topVertex);

			FLOAT phiStep = PI / stackCount;
			FLOAT thetaStep = 2.0f * PI / sliceCount;

			// Compute vertices for each stack ring (do not count the poles as rings).
			for (UINT32 i = 1; i <= stackCount - 1; ++i)
			{
				FLOAT phi = i * phiStep;

				// Vertices of ring.
				for (UINT32 j = 0; j <= sliceCount; ++j)
				{
					FLOAT theta = j * thetaStep;

					FVertex v;

					// spherical to cartesian
					v.Position.x = radius * sinf(phi) * cosf(theta);
					v.Position.y = radius * cosf(phi);
					v.Position.z = radius * sinf(phi) * sinf(theta);

					// Partial derivative of P with respect to theta
					v.Tangent.x = -radius * sinf(phi) * sinf(theta);
					v.Tangent.y = 0.0f;
					v.Tangent.z = +radius * sinf(phi) * cosf(theta);

					v.Tangent = Normalize(v.Tangent);
					v.Normal = Normalize(v.Normal);

					v.UV.x = theta / 2.0f * PI;
					v.UV.y = phi / PI;

					rSubmesh.Vertices.push_back(v);
				}
			}

			rSubmesh.Vertices.push_back(bottomVertex);

			//
			// Compute indices for top stack.  The top stack was written first to the FVertex buffer
			// and connects the top pole to the first ring.
			//

			for (UINT32 i = 1; i <= sliceCount; ++i)
			{
				rSubmesh.Indices.push_back(0);
				rSubmesh.Indices.push_back(i + 1);
				rSubmesh.Indices.push_back(i);
			}

			//
			// Compute indices for inner stacks (not connected to poles).
			//

			// Offset the indices to the index of the first FVertex in the first ring.
			// This is just skipping the top pole FVertex.
			UINT32 baseIndex = 1;
			UINT32 ringFVerUVount = sliceCount + 1;
			for (UINT32 i = 0; i < stackCount - 2; ++i)
			{
				for (UINT32 j = 0; j < sliceCount; ++j)
				{
					rSubmesh.Indices.push_back(baseIndex + i * ringFVerUVount + j);
					rSubmesh.Indices.push_back(baseIndex + i * ringFVerUVount + j + 1);
					rSubmesh.Indices.push_back(baseIndex + (i + 1) * ringFVerUVount + j);

					rSubmesh.Indices.push_back(baseIndex + (i + 1) * ringFVerUVount + j);
					rSubmesh.Indices.push_back(baseIndex + i * ringFVerUVount + j + 1);
					rSubmesh.Indices.push_back(baseIndex + (i + 1) * ringFVerUVount + j + 1);
				}
			}

			//
			// Compute indices for bottom stack.  The bottom stack was written last to the FVertex buffer
			// and connects the bottom pole to the bottom ring.
			//

			// South pole FVertex was added last.
			UINT32 southPoleIndex = (UINT32)rSubmesh.Vertices.size() - 1;

			// Offset the indices to the index of the first FVertex in the last ring.
			baseIndex = southPoleIndex - ringFVerUVount;

			for (UINT32 i = 0; i < sliceCount; ++i)
			{
				rSubmesh.Indices.push_back(southPoleIndex);
				rSubmesh.Indices.push_back(baseIndex + i);
				rSubmesh.Indices.push_back(baseIndex + i + 1);
			}

			return Mesh;
		}


		FMesh CreateGeosphere(FLOAT radius, UINT32 numSubdivisions)
		{
			FMesh Mesh;
			auto& rSubmesh = Mesh.Submeshes.emplace_back();

			// Put a cap on the number of subdivisions.
			numSubdivisions = std::min<UINT32>(numSubdivisions, 6u);

			// Approximate a sphere by tessellating an icosahedron.

			const FLOAT X = 0.525731f;
			const FLOAT Z = 0.850651f;

			FVector3F pos[12] =
			{
				FVector3F(-X, 0.0f, Z),  FVector3F(X, 0.0f, Z),
				FVector3F(-X, 0.0f, -Z), FVector3F(X, 0.0f, -Z),
				FVector3F(0.0f, Z, X),   FVector3F(0.0f, Z, -X),
				FVector3F(0.0f, -Z, X),  FVector3F(0.0f, -Z, -X),
				FVector3F(Z, X, 0.0f),   FVector3F(-Z, X, 0.0f),
				FVector3F(Z, -X, 0.0f),  FVector3F(-Z, -X, 0.0f)
			};

			UINT32 k[60] =
			{
				1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,
				1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,
				3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0,
				10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7
			};

			rSubmesh.Vertices.resize(12);
			rSubmesh.Indices.assign(&k[0], &k[60]);

			for (UINT32 i = 0; i < 12; ++i)
				rSubmesh.Vertices[i].Position = pos[i];

			for (UINT32 i = 0; i < numSubdivisions; ++i)
				Subdivide(rSubmesh);

			// Project vertices onto sphere and scale.
			for (UINT32 i = 0; i < rSubmesh.Vertices.size(); ++i)
			{
				// Project onto unit sphere.
				rSubmesh.Vertices[i].Position = Normalize(rSubmesh.Vertices[i].Position);

				// Project onto sphere.
				rSubmesh.Vertices[i].Normal = radius * rSubmesh.Vertices[i].Position;


				// Derive texture coordinates from spherical coordinates.
				FLOAT theta = atan2f(rSubmesh.Vertices[i].Position.z, rSubmesh.Vertices[i].Position.x);

				// Put in [0, 2pi].
				if (theta < 0.0f)
					theta += 2.0f * PI;

				FLOAT phi = acosf(rSubmesh.Vertices[i].Position.y / radius);

				rSubmesh.Vertices[i].UV.x = theta / 2.0f * PI;
				rSubmesh.Vertices[i].UV.y = phi / PI;

				// Partial derivative of P with respect to theta
				rSubmesh.Vertices[i].Tangent.x = -radius * sinf(phi) * sinf(theta);
				rSubmesh.Vertices[i].Tangent.y = 0.0f;
				rSubmesh.Vertices[i].Tangent.z = +radius * sinf(phi) * cosf(theta);

				rSubmesh.Vertices[i].Tangent = Normalize(rSubmesh.Vertices[i].Tangent);
			}

			return Mesh;
		}

		FMesh CreateCylinder(FLOAT bottomRadius, FLOAT topRadius, FLOAT height, UINT32 sliceCount, UINT32 stackCount)
		{
			FMesh Mesh;
			auto& rSubmesh = Mesh.Submeshes.emplace_back();

			//
			// Build Stacks.
			// 

			FLOAT stackHeight = height / stackCount;

			// Amount to increment radius as we move up each stack level from bottom to top.
			FLOAT radiusStep = (topRadius - bottomRadius) / stackCount;

			UINT32 ringCount = stackCount + 1;

			// Compute vertices for each stack ring starting at the bottom and moving up.
			for (UINT32 i = 0; i < ringCount; ++i)
			{
				FLOAT y = -0.5f * height + i * stackHeight;
				FLOAT r = bottomRadius + i * radiusStep;

				// vertices of ring
				FLOAT dTheta = 2.0f * PI / sliceCount;
				for (UINT32 j = 0; j <= sliceCount; ++j)
				{
					FVertex vertex;

					FLOAT c = cosf(j * dTheta);
					FLOAT s = sinf(j * dTheta);

					vertex.Position = FVector3F(r * c, y, r * s);

					vertex.UV.x = (FLOAT)j / sliceCount;
					vertex.UV.y = 1.0f - (FLOAT)i / stackCount;

					// Cylinder can be parameterized as follows, where we introduce v
					// parameter that goes in the same direction as the v tex-coord
					// so that the bitangent goes in the same direction as the v tex-coord.
					//   Let r0 be the bottom radius and let r1 be the top radius.
					//   y(v) = h - hv for v in [0,1].
					//   r(v) = r1 + (r0-r1)v
					//
					//   x(t, v) = r(v)*cos(t)
					//   y(t, v) = h - hv
					//   z(t, v) = r(v)*sin(t)
					// 
					//  dx/dt = -r(v)*sin(t)
					//  dy/dt = 0
					//  dz/dt = +r(v)*cos(t)
					//
					//  dx/dv = (r0-r1)*cos(t)
					//  dy/dv = -h
					//  dz/dv = (r0-r1)*sin(t)

					// This is unit length.
					vertex.Tangent = FVector4F(-s, 0.0f, c, 1.0f);

					FLOAT dr = bottomRadius - topRadius;
					FVector4F bitangent(dr * c, -height, dr * s, 1.0f);

					vertex.Normal = Normalize(Cross(FVector3F(vertex.Tangent), FVector3F(bitangent)));
					rSubmesh.Vertices.push_back(vertex);
				}
			}

			// Add one because we duplicate the first and last vertex per ring
			// since the texture coordinates are different.
			UINT32 ringVerUVount = sliceCount + 1;

			// Compute indices for each stack.
			for (UINT32 i = 0; i < stackCount; ++i)
			{
				for (UINT32 j = 0; j < sliceCount; ++j)
				{
					rSubmesh.Indices.push_back(i * ringVerUVount + j);
					rSubmesh.Indices.push_back((i + 1) * ringVerUVount + j);
					rSubmesh.Indices.push_back((i + 1) * ringVerUVount + j + 1);

					rSubmesh.Indices.push_back(i * ringVerUVount + j);
					rSubmesh.Indices.push_back((i + 1) * ringVerUVount + j + 1);
					rSubmesh.Indices.push_back(i * ringVerUVount + j + 1);
				}
			}

			BuildCylinderTopCap(bottomRadius, topRadius, height, sliceCount, stackCount, rSubmesh);
			BuildCylinderBottomCap(bottomRadius, topRadius, height, sliceCount, stackCount, rSubmesh);

			return Mesh;
		}

		void BuildCylinderTopCap(FLOAT bottomRadius, FLOAT topRadius, FLOAT height, UINT32 sliceCount, UINT32 stackCount, FMesh::Submesh& meshData)
		{
			UINT32 baseIndex = (UINT32)meshData.Vertices.size();

			FLOAT y = 0.5f * height;
			FLOAT dTheta = 2.0f * PI / sliceCount;

			// Duplicate cap ring vertices because the texture coordinates and normals differ.
			for (UINT32 i = 0; i <= sliceCount; ++i)
			{
				FLOAT x = topRadius * cosf(i * dTheta);
				FLOAT z = topRadius * sinf(i * dTheta);

				// Scale down by the height to try and make top cap texture coord area
				// proportional to base.
				FLOAT u = x / height + 0.5f;
				FLOAT v = z / height + 0.5f;

				meshData.Vertices.push_back(FVertex({ x, y, z }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { u, v }));
			}

			// Cap center vertex.
			meshData.Vertices.push_back(FVertex({ 0.0f, y, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }));

			// Index of center vertex.
			UINT32 centerIndex = (UINT32)meshData.Vertices.size() - 1;

			for (UINT32 i = 0; i < sliceCount; ++i)
			{
				meshData.Indices.push_back(centerIndex);
				meshData.Indices.push_back(baseIndex + i + 1);
				meshData.Indices.push_back(baseIndex + i);
			}
		}

		void BuildCylinderBottomCap(FLOAT bottomRadius, FLOAT topRadius, FLOAT height, UINT32 sliceCount, UINT32 stackCount, FMesh::Submesh& meshData)
		{
			// 
			// Build bottom cap.
			//

			UINT32 baseIndex = (UINT32)meshData.Vertices.size();
			FLOAT y = -0.5f * height;

			// vertices of ring
			FLOAT dTheta = 2.0f * PI / sliceCount;
			for (UINT32 i = 0; i <= sliceCount; ++i)
			{
				FLOAT x = bottomRadius * cosf(i * dTheta);
				FLOAT z = bottomRadius * sinf(i * dTheta);

				// Scale down by the height to try and make top cap texture coord area
				// proportional to base.
				FLOAT u = x / height + 0.5f;
				FLOAT v = z / height + 0.5f;

				meshData.Vertices.push_back(FVertex({ x, y, z }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { u, v }));
			}

			// Cap center vertex.
			meshData.Vertices.push_back(FVertex({ 0.0f, y, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }));

			// Cache the index of center vertex.
			UINT32 centerIndex = (UINT32)meshData.Vertices.size() - 1;

			for (UINT32 i = 0; i < sliceCount; ++i)
			{
				meshData.Indices.push_back(centerIndex);
				meshData.Indices.push_back(baseIndex + i);
				meshData.Indices.push_back(baseIndex + i + 1);
			}
		}

		FMesh CreateGrid(FLOAT width, FLOAT depth, UINT32 m, UINT32 n)
		{
			FMesh Mesh;
			auto& rSubmesh = Mesh.Submeshes.emplace_back();

			UINT32 verUVount = m * n;
			UINT32 faceCount = (m - 1) * (n - 1) * 2;

			//
			// Create the vertices.
			//

			FLOAT halfWidth = 0.5f * width;
			FLOAT halfDepth = 0.5f * depth;

			FLOAT dx = width / (n - 1);
			FLOAT dz = depth / (m - 1);

			FLOAT du = 1.0f / (n - 1);
			FLOAT dv = 1.0f / (m - 1);

			rSubmesh.Vertices.resize(verUVount);
			for (UINT32 i = 0; i < m; ++i)
			{
				FLOAT z = halfDepth - i * dz;
				for (UINT32 j = 0; j < n; ++j)
				{
					FLOAT x = -halfWidth + j * dx;

					rSubmesh.Vertices[i * n + j].Position = FVector3F(x, 0.0f, z);
					rSubmesh.Vertices[i * n + j].Normal = FVector3F(0.0f, 1.0f, 0.0f);
					rSubmesh.Vertices[i * n + j].Tangent = FVector4F(1.0f, 0.0f, 0.0f, 1.0f);

					// Stretch texture over grid.
					rSubmesh.Vertices[i * n + j].UV.x = j * du;
					rSubmesh.Vertices[i * n + j].UV.y = i * dv;
				}
			}

			//
			// Create the indices.
			//

			rSubmesh.Indices.resize(faceCount * 3); // 3 indices per face

			// Iterate over each quad and compute indices.
			UINT32 k = 0;
			for (UINT32 i = 0; i < m - 1; ++i)
			{
				for (UINT32 j = 0; j < n - 1; ++j)
				{
					rSubmesh.Indices[k] = i * n + j;
					rSubmesh.Indices[k + 1] = i * n + j + 1;
					rSubmesh.Indices[k + 2] = (i + 1) * n + j;

					rSubmesh.Indices[k + 3] = (i + 1) * n + j;
					rSubmesh.Indices[k + 4] = i * n + j + 1;
					rSubmesh.Indices[k + 5] = (i + 1) * n + j + 1;

					k += 6; // next quad
				}
			}

			return Mesh;
		}

		FMesh CreateQuad(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT depth)
		{
			FMesh Mesh;
			auto& rSubmesh = Mesh.Submeshes.emplace_back();


			rSubmesh.Vertices.resize(4);
			rSubmesh.Indices.resize(6);

			// Position coordinates specified in NDC space.
			rSubmesh.Vertices[0] = FVertex(
				{ x, y - h, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 0.0f, 1.0f }
			);

			rSubmesh.Vertices[1] = FVertex(
				{ x, y, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 0.0f, 0.0f }
			);

			rSubmesh.Vertices[2] = FVertex(
				{ x + w, y, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 1.0f, 0.0f }
			);

			rSubmesh.Vertices[3] = FVertex(
				{ x + w, y - h, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 1.0f, 1.0f }
			);

			rSubmesh.Indices[0] = 0;
			rSubmesh.Indices[1] = 1;
			rSubmesh.Indices[2] = 2;

			rSubmesh.Indices[3] = 0;
			rSubmesh.Indices[4] = 2;
			rSubmesh.Indices[5] = 3;

			return Mesh;
		}

	}


}
