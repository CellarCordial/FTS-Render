#include "geometry.h"
#include <cstring>
#include <utility>

namespace fantasy
{
	void MeshOptimizer::BinaryHeap::resize(uint32_t dwIndexNum)
	{
		_current_size = 0;
		_index_count = dwIndexNum;
		_heap.clear(); _heap.resize(dwIndexNum);
		_keys.clear(); _keys.resize(dwIndexNum);
		_heap_indices.clear(); _heap_indices.resize(dwIndexNum);

		memset(_heap_indices.data(), 0xff, dwIndexNum * sizeof(uint32_t));
	}
	
	float MeshOptimizer::BinaryHeap::get_key(uint32_t index)
	{
		return _keys[index];
	}
	
	bool MeshOptimizer::BinaryHeap::empty()
	{
		return _current_size == 0;
	}
	
	bool MeshOptimizer::BinaryHeap::is_valid(uint32_t index)
	{
		return _heap_indices[index] != INVALID_SIZE_32;
	}
	
	uint32_t MeshOptimizer::BinaryHeap::top()
	{
		assert(_current_size > 0);
		return _heap[0];
	}
	
	void MeshOptimizer::BinaryHeap::pop()
	{
		assert(_current_size > 0);
		uint32_t index = _heap[0];
		_heap[0] = _heap[--_current_size];
		_heap_indices[_heap[0]] = 0;
		_heap_indices[index] = INVALID_SIZE_32;
		push_down(0);
	}
	
	void MeshOptimizer::BinaryHeap::remove(uint32_t index)
	{
		assert(IsInvalid(index));

		float key = _keys[index];
		uint32_t& rdwIndex = _heap_indices[index];
		if (rdwIndex == _current_size - 1)
		{
			_current_size--;
			rdwIndex = INVALID_SIZE_32;
			return;
		}

		_heap[rdwIndex] = _heap[--_current_size];
		_heap_indices[_heap[rdwIndex]] = rdwIndex;
		_heap_indices[index] = INVALID_SIZE_32;

		if (key < _keys[_heap[rdwIndex]]) push_down(rdwIndex);
		else push_up(rdwIndex);
	}
	
	void MeshOptimizer::BinaryHeap::insert(float key,uint32_t index)
	{
		assert(IsInvalid(index));

		_current_size++;
		_heap[_current_size] = index;
		_keys[index] = key;
		_heap_indices[index] = _current_size;
		push_up(_current_size);
	}

	void MeshOptimizer::BinaryHeap::push_down(uint32_t index)
	{
		uint32_t ix=_heap[index];
		uint32_t dwFatherIndex=(index-1)>>1;
		while (index > 0 && _keys[ix] < _keys[_heap[dwFatherIndex]])
		{
			_heap[index] = _heap[dwFatherIndex];
			_heap_indices[_heap[index]] = index;
			index = dwFatherIndex;
			dwFatherIndex = (index-1) >> 1;
		}
		_heap[index] = ix;
		_heap_indices[_heap[index]] = index;
	}

	void MeshOptimizer::BinaryHeap::push_up(uint32_t index)
	{
		uint32_t ix = _heap[index];
		uint32_t left_index = (index << 1) + 1;
		uint32_t right_index = left_index + 1;
		while(left_index < _current_size)
		{
			uint32_t tmp = left_index;
			if(right_index < _current_size && _keys[_heap[right_index]] < _keys[_heap[left_index]]) tmp=right_index;
			if(_keys[_heap[tmp]] < _keys[ix])
			{
				_heap[index] = _heap[tmp];
				_heap_indices[_heap[index]] = index;
				index = tmp;
				left_index = (index << 1) + 1;
				right_index = left_index + 1;
			}
			else 
			{
				break;
			}
		}
		_heap[index] = ix;
		_heap_indices[_heap[index]] = index;
	}

	MeshOptimizer::MeshOptimizer(const std::span<Vector3F>& crVertices, const std::span<uint32_t>& crIndices) :
		_vertices(crVertices), 
		_indices(crIndices),
		_vertex_ref_count(crVertices.size()),
		_vertex_table(crVertices.size()),
		_index_table(crIndices.size()),
		_flags(crIndices.size()),
		_triangle_surfaces(crIndices.size() / 3.0f),
		_triangle_removed_array(crIndices.size() / 3.0f, false)
	{
		_remain_vertex_num = static_cast<uint32_t>(_vertices.size());
		_remain_triangle_num = static_cast<uint32_t>(_triangle_surfaces.size());

		for (uint32_t ix = 0; ix < _vertices.size(); ++ix)
		{
			_vertex_table.insert(hash(_vertices[ix]), ix);
		}

		uint64_t stEdgeNum = std::min(std::min(_indices.size(), 3 * _vertices.size() - 6), _triangle_surfaces.size() + _vertices.size());
		_edges.resize(stEdgeNum);
		_edges_begin_table.resize(stEdgeNum);
		_edges_end_table.resize(stEdgeNum);

		for (uint32_t dwCorner = 0; dwCorner < _indices.size(); ++dwCorner)
		{
			_vertex_ref_count[_indices[dwCorner]]++;
			const auto& crVertex = _vertices[_indices[dwCorner]];

			_index_table.insert(hash(crVertex), dwCorner);
			
			Vector3F p0 = crVertex;
			Vector3F p1 = _vertices[_indices[TriangleIndexCycle3(dwCorner)]];

			uint32_t h0 = hash(p0);
			uint32_t h1 = hash(p1);
			if (h0 > h1)
			{
				std::swap(h0, h1);
				std::swap(p0, p1);
			}

			bool bAlreadyExist = false;
			for (uint32_t ix : _edges_begin_table[h0])
			{
				const auto& crEdge = _edges[ix];
				if (crEdge.first == p0 && crEdge.second == p1) 
				{
					bAlreadyExist = true;
					break;
				}
			}
			if (!bAlreadyExist)
			{
				_edges_begin_table.insert(h0, static_cast<uint32_t>(_edges.size()));
				_edges_end_table.insert(h1, static_cast<uint32_t>(_edges.size()));
				_edges.emplace_back(std::make_pair(p0, p1));
			}
		}
	}


	bool MeshOptimizer::optimize(uint32_t dwTargetTriangleNum)
	{
		for (uint32_t ix = 0; ix < _triangle_surfaces.size(); ++ix) fix_triangle(ix);

		if (_remain_triangle_num <= dwTargetTriangleNum) return compact();

		_heap.resize(_edges.size());

		uint32_t ix = 0;
		for (const auto& crEdge : _edges)
		{
			float fError = evaluate(crEdge.first, crEdge.second, false);
			_heap.insert(fError, ix++);
		}

		_max_error = 0.0f;
		while (!_heap.empty())
		{
			uint32_t dwEdgeIndex = _heap.top();
			if (_heap.get_key(dwEdgeIndex) >= 1e6) break;

			_heap.pop();
			
			const auto& crEdge = _edges[dwEdgeIndex];
			_edges_begin_table.remove(hash(crEdge.first), dwEdgeIndex);
			_edges_end_table.remove(hash(crEdge.second), dwEdgeIndex);

			_max_error = std::max(evaluate(crEdge.first, crEdge.second, true), _max_error);

			if (_remain_triangle_num <= dwTargetTriangleNum) break;

			for (uint32_t ix : edge_need_reevaluate_indices)
			{
				const auto& crReevaluateEdge = _edges[ix];
				float fError = evaluate(crReevaluateEdge.first, crReevaluateEdge.second, false);
				_heap.insert(fError, ix++);
			}

			edge_need_reevaluate_indices.clear();
		}
		return compact();
	}

	void MeshOptimizer::lock_position(Vector3F Pos)
	{
		for (uint32_t ix : _index_table[hash(Pos)])
		{
			if (_vertices[_indices[ix]] == Pos)
			{
				_flags[ix] |= LockFlag;
			}
		}
	}


	uint32_t MeshOptimizer::hash(const Vector3F& vec)
	{
		union 
		{
			float f;
			uint32_t u;
		} x, y, z;

		x.f = (vec.x == 0.0f ? 0 : vec.x);
		y.f = (vec.y == 0.0f ? 0 : vec.y);
		z.f = (vec.z == 0.0f ? 0 : vec.z);
		return murmur_mix(murmur_add(murmur_add(x.u, y.u), z.u));
	}

	void MeshOptimizer::fix_triangle(uint32_t dwTriangleIndex)
	{
		if (_triangle_removed_array[dwTriangleIndex]) return;

		uint32_t dwVertexIndex0 = _indices[dwTriangleIndex * 3 + 0];
		uint32_t dwVertexIndex1 = _indices[dwTriangleIndex * 3 + 1];
		uint32_t dwVertexIndex2 = _indices[dwTriangleIndex * 3 + 2];

		const Vector3F& p0 = _vertices[dwVertexIndex0];	
		const Vector3F& p1 = _vertices[dwVertexIndex1];	
		const Vector3F& p2 = _vertices[dwVertexIndex2];

		bool bNeedRemoved = p0 == p1 || p1 == p2 || p2 == p0;
		if (!bNeedRemoved)
		{
			for (uint32_t ix = 0; ix < 3; ++ix)
			{
				// 去除重复的 Vertex
				uint32_t& rdwVertexIndex = _indices[dwTriangleIndex * 3 + ix];
				const auto& crVertex = _vertices[rdwVertexIndex];
				uint32_t dwHash = hash(crVertex);
				for (uint32_t jx : _vertex_table[dwHash])
				{
					if (jx == rdwVertexIndex)
					{
						break;
					}
					else if (crVertex == _vertices[jx]) 
					{
						assert(_vertex_ref_count[rdwVertexIndex] > 0);
						if (--_vertex_ref_count[rdwVertexIndex] == 0)
						{
							_vertex_table.remove(dwHash, rdwVertexIndex);
							_remain_vertex_num--;
						}
						rdwVertexIndex = jx;
						if (rdwVertexIndex != INVALID_SIZE_32) _vertex_ref_count[rdwVertexIndex]++;
						break;
					}
				}
			}
			
			// 判断 Triangle 是否重复
			for (uint32_t ix : _index_table[hash(p0)])
			{
				if (
					ix != dwTriangleIndex * 3 &&
					dwVertexIndex0 == _indices[ix] && 
					dwVertexIndex1 == _indices[TriangleIndexCycle3(ix)] && 
					dwVertexIndex2 == _indices[TriangleIndexCycle3(ix, 2)] 
				)
				{
					bNeedRemoved = true;
				}
			}
		}

		if (bNeedRemoved)
		{
			_triangle_removed_array.set_true(dwTriangleIndex);
			_remain_triangle_num--;

			for (uint32_t ix = 0; ix < 3; ++ix)
			{
				uint32_t& rdwVertexIndex = _indices[dwTriangleIndex * 3 + ix];
				const auto& crVertex = _vertices[rdwVertexIndex];

				uint32_t dwHash = hash(crVertex);
				_index_table.remove(dwHash, rdwVertexIndex);

				assert(_vertex_ref_count[rdwVertexIndex] > 0);
				if (--_vertex_ref_count[rdwVertexIndex] == 0)
				{
					_vertex_table.remove(dwHash, rdwVertexIndex);
					_remain_vertex_num--;
				}
				rdwVertexIndex = INVALID_SIZE_32;
			}
		}
		else 
		{
			_triangle_surfaces[dwTriangleIndex] = QuadricSurface(p0, p1, p2);
		}
	}

	bool MeshOptimizer::compact()
	{
		uint32_t dwCount = 0;
		std::vector<uint32_t> IndicesMap(_vertices.size());
		for (uint32_t ix = 0; ix < _vertices.size(); ++ix)
		{
			if (_vertex_ref_count[ix] > 0)
			{
				if (ix != dwCount) _vertices[dwCount] = _vertices[ix];
				IndicesMap[ix] = dwCount++;
			}
		}
		ReturnIfFalse(dwCount == _remain_vertex_num);

		dwCount = 0;
		for (uint32_t ix = 0; ix < _triangle_surfaces.size(); ++ix)
		{
			if (!_triangle_removed_array[ix])
			{
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					_indices[dwCount * 3 + jx] = IndicesMap[_indices[ix * 3 + jx]];
				}
				dwCount++;
			}
		}
		return dwCount == _remain_triangle_num;
	}

	float MeshOptimizer::evaluate(const Vector3F& p0, const Vector3F& p1, bool bMerge)
	{
		if (p0 == p1) return 0.0f;

		float fError;

		std::vector<uint32_t> AdjacencyTriangleIndices;
		auto FuncBuildAdjacencyTrianlges = [this, &AdjacencyTriangleIndices](const Vector3F& crVertex, bool& rbLock)
		{
			for (uint32_t ix : _index_table[hash(crVertex)])
			{
				if (_vertices[_indices[ix]] == crVertex)
				{
					uint32_t dwTriangleIndex = ix / 3;
					if ((_flags[dwTriangleIndex * 3] & AdjacencyFlag) == 0)
					{
						_flags[dwTriangleIndex * 3] |= AdjacencyFlag;
						AdjacencyTriangleIndices.push_back(dwTriangleIndex);
					}

					if (_flags[ix] & LockFlag) rbLock = true;
				}
			}
		};

		bool bLock0 = false, bLock1 = false;
		FuncBuildAdjacencyTrianlges(p0, bLock0);
		FuncBuildAdjacencyTrianlges(p1, bLock1);

		if (AdjacencyTriangleIndices.empty()) return 0.0f;
		if (AdjacencyTriangleIndices.size() > 24) fError += 0.5f * (AdjacencyTriangleIndices.size() - 24);

		QuadricSurface Surface;
		for (uint32_t ix : AdjacencyTriangleIndices) Surface = merge(Surface, _triangle_surfaces[ix]);

		Vector3F p = (p0 + p1) * 0.5f;
		if (bLock0 && bLock1) fError += 1e8;
		else if (bLock0 && !bLock1) p = p0;
		else if (!bLock0 && bLock1) p = p1;
		else if (
			!Surface.get_vertex_position(p) || 
			Distance(p, p0) + Distance(p, p1) > 2.0f * Distance(p0, p1)	// invalid
		)
		{
			p = (p0 + p1) * 0.5f;
		}

		fError += Surface.distance_to_surface(p);

		if (bMerge)
		{
			merge_begin(p0);
			merge_begin(p1);

			for (uint32_t ix : AdjacencyTriangleIndices)
			{
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					uint32_t dwVertexIndex = ix * 3 + jx;
					Vector3F& rPos = _vertices[_indices[dwVertexIndex]];
					if (rPos == p0 || rPos == p1)
					{
						rPos = p;
						if (bLock0 || bLock1)
						{
							_flags[dwVertexIndex] |= LockFlag;
						}
					}
				}
			}
			for (uint32_t ix : moved_edge_indices)
			{
				auto& rEdge = _edges[ix];
				if (rEdge.first == p0 || rEdge.first == p1) rEdge.first = p;
				if (rEdge.second == p0 || rEdge.second == p1) rEdge.second = p;
			}

			mergen_end();

			std::vector<uint32_t> AdjacencyVertexIndices;
			AdjacencyVertexIndices.reserve((AdjacencyTriangleIndices.size() * 3));
			for (uint32_t ix : AdjacencyTriangleIndices)
			{
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					AdjacencyVertexIndices.push_back(_indices[ix * 3 + jx]);
				}
			}
			std::sort(AdjacencyVertexIndices.begin(), AdjacencyVertexIndices.end());
			AdjacencyVertexIndices.erase(
				std::unique(AdjacencyVertexIndices.begin(), AdjacencyVertexIndices.end()), 
				AdjacencyVertexIndices.end()
			);

			for (uint32_t dwVertexIndex : AdjacencyVertexIndices)
			{
				const auto& crVertex = _vertices[dwVertexIndex];
				uint32_t dwHash = hash(crVertex);
				for (uint32_t ix : _edges_begin_table[dwHash])
				{
					if (_edges[ix].first == crVertex && _heap.is_valid(ix))
					{
						_heap.remove(ix);
						edge_need_reevaluate_indices.push_back(ix);
					}
				}
				for (uint32_t ix : _edges_end_table[dwHash])
				{
					if (_edges[ix].second == crVertex && _heap.is_valid(ix))
					{
						_heap.remove(ix);
						edge_need_reevaluate_indices.push_back(ix);
					}
				}
			}
			
			for (uint32_t dwTriangleIndex : AdjacencyTriangleIndices) fix_triangle(dwTriangleIndex);
		}	

		for (uint32_t dwTriangleIndex : AdjacencyTriangleIndices) _flags[dwTriangleIndex * 3] &= ~AdjacencyFlag;

		return fError;
	}

	void MeshOptimizer::merge_begin(const Vector3F& p)
	{
		uint32_t dwHash = hash(p);
		for (uint32_t ix : _vertex_table[dwHash])
		{
			if (_vertices[ix] == p)
			{
				_vertex_table.remove(dwHash, ix);
				moved_vertex_indices.push_back(ix);
			}
		}
		for (uint32_t ix : _index_table[dwHash])
		{
			if (_vertices[_indices[ix]] == p)
			{
				_index_table.remove(dwHash, ix);
				moved_index_indices.push_back(ix);
			}
		}
		for (uint32_t ix : _edges_begin_table[dwHash])
		{
			if (_edges[ix].first == p)
			{
				_edges_begin_table.remove(dwHash, ix);
				moved_edge_indices.push_back(ix);
			}
		}
		for (uint32_t ix : _edges_end_table[dwHash])
		{
			if (_edges[ix].second == p)
			{
				_edges_end_table.remove(dwHash, ix);
				moved_edge_indices.push_back(ix);
			}
		}
	}

	void MeshOptimizer::mergen_end()
	{
		for (uint32_t ix : moved_vertex_indices)
		{
			_vertex_table.insert(hash(_vertices[ix]), ix);
		}
		for (uint32_t ix : moved_index_indices)
		{
			_index_table.insert(hash(_vertices[_indices[ix]]), ix);
		}
		for (uint32_t ix : moved_edge_indices)
		{
			auto& rEdge = _edges[ix];
			
			uint32_t h0 = hash(rEdge.first);
			uint32_t h1 = hash(rEdge.second);
			if (h0 > h1)
			{
				std::swap(h0, h1);
				std::swap(rEdge.first, rEdge.second);
			}

			bool bAlreadyExist = false;
			for (uint32_t ix : _edges_begin_table[h0])
			{
				const auto& crEdge = _edges[ix];
				if (crEdge.first == crEdge.first && crEdge.second == crEdge.second) 
				{
					bAlreadyExist = true;
					break;
				}
			}
			if (!bAlreadyExist)
			{
				_edges_begin_table.insert(h0, static_cast<uint32_t>(_edges.size()));
				_edges_end_table.insert(h1, static_cast<uint32_t>(_edges.size()));
			}

			if (rEdge.first == rEdge.second && bAlreadyExist)
			{
				_heap.remove(ix);
			}
		}
		
		moved_edge_indices.clear();
		moved_vertex_indices.clear();
		moved_index_indices.clear();
	}



		struct Graph
		{
			std::vector<std::map<uint32_t,int32_t>> g;

			void init(uint32_t n)
			{
				g.resize(n);
			}
			void add_node()
			{
				g.push_back({});
			}
			void add_edge(uint32_t from,uint32_t to,int32_t cost)
			{
				g[from][to]=cost;
			}
			void increase_edge_cost(uint32_t from,uint32_t to,int32_t i_cost)
			{
				g[from][to]+=i_cost;
			}
		};

		struct MetisGraph;

		class Partitioner
		{
		private:
			uint32_t bisect_graph(MetisGraph* graph_data,MetisGraph* child_graphs[2],uint32_t start,uint32_t end);
			void recursive_bisect_graph(MetisGraph* graph_data,uint32_t start,uint32_t end);
		public:
			void init(uint32_t num_node);
			void partition(const Graph& graph,uint32_t min_part_size,uint32_t max_part_size);

			std::vector<uint32_t> node_id;
			std::vector<std::pair<uint32_t,uint32_t>> ranges;
			std::vector<uint32_t> sort_to;
			uint32_t min_part_size;
			uint32_t max_part_size;
		};


        void create_triangle_cluster(
            const std::vector<Vector3F>& verts,
            const std::vector<uint32_t>& indexes,
            std::vector<Cluster>& clusters
        )
		{

		}

        void build_cluster_groups(
            std::vector<Cluster>& clusters,
            uint32_t offset,
            uint32_t num_cluster,
            std::vector<ClusterGroup>& cluster_groups,
            uint32_t mip_level
        )
		{

		}

        void build_cluster_group_parent_clusters(
            ClusterGroup& cluster_group,
            std::vector<Cluster>& clusters
        )
		{

		}


	namespace Geometry
	{
		Mesh create_box(float width, float height, float depth, uint32_t numSubdivisions)
		{
			Mesh Mesh;
			auto& rSubmesh = Mesh.submeshes.emplace_back();

			Vertex v[24];

			float w2 = 0.5f * width;
			float h2 = 0.5f * height;
			float d2 = 0.5f * depth;

			v[0] = Vertex({ -w2, -h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[1] = Vertex({ -w2, +h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[2] = Vertex({ +w2, +h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[3] = Vertex({ +w2, -h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });
			v[4] = Vertex({ -w2, -h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });
			v[5] = Vertex({ +w2, -h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[6] = Vertex({ +w2, +h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[7] = Vertex({ -w2, +h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[8] = Vertex({ -w2, +h2, -d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[9] = Vertex({ -w2, +h2, +d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[10] = Vertex({ +w2, +h2, +d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[11] = Vertex({ +w2, +h2, -d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });
			v[12] = Vertex({ -w2, -h2, -d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });
			v[13] = Vertex({ +w2, -h2, -d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[14] = Vertex({ +w2, -h2, +d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[15] = Vertex({ -w2, -h2, +d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[16] = Vertex({ -w2, -h2, +d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 0.0f, 1.0f });
			v[17] = Vertex({ -w2, +h2, +d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 0.0f, 0.0f });
			v[18] = Vertex({ -w2, +h2, -d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 1.0f, 0.0f });
			v[19] = Vertex({ -w2, -h2, -d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 1.0f, 1.0f });
			v[20] = Vertex({ +w2, -h2, -d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 1.0f });
			v[21] = Vertex({ +w2, +h2, -d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f });
			v[22] = Vertex({ +w2, +h2, +d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f });
			v[23] = Vertex({ +w2, -h2, +d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f });

			rSubmesh.vertices.assign(&v[0], &v[24]);

			uint16_t i[36];

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

			rSubmesh.indices.assign(&i[0], &i[36]);

			// Put a cap on the number of subdivisions.
			numSubdivisions = std::min<uint32_t>(numSubdivisions, 6u);

			for (uint32_t i = 0; i < numSubdivisions; ++i) subdivide(rSubmesh);

			return Mesh;
		}

		void subdivide(Mesh::Submesh& mesh_data)
		{
			Mesh::Submesh inputCopy = mesh_data;


			mesh_data.vertices.clear();
			mesh_data.indices.clear();

			//       v1
			//       *
			//      / \
            //     /   \
            //  m0*-----*m1
			//   / \   / \
            //  /   \ /   \
            // *-----*-----*
			// v0    m2     v2

			uint32_t numTris = (uint32_t)inputCopy.indices.size() / 3;
			for (uint32_t i = 0; i < numTris; ++i)
			{
				Vertex v0 = inputCopy.vertices[inputCopy.indices[i * 3 + 0]];
				Vertex v1 = inputCopy.vertices[inputCopy.indices[i * 3 + 1]];
				Vertex v2 = inputCopy.vertices[inputCopy.indices[i * 3 + 2]];

				//
				// Generate the midpoints.
				//

				Vertex m0 = get_mid_point(v0, v1);
				Vertex m1 = get_mid_point(v1, v2);
				Vertex m2 = get_mid_point(v0, v2);

				//
				// add new geometry.
				//

				mesh_data.vertices.push_back(v0); // 0
				mesh_data.vertices.push_back(v1); // 1
				mesh_data.vertices.push_back(v2); // 2
				mesh_data.vertices.push_back(m0); // 3
				mesh_data.vertices.push_back(m1); // 4
				mesh_data.vertices.push_back(m2); // 5

				mesh_data.indices.push_back(i * 6 + 0);
				mesh_data.indices.push_back(i * 6 + 3);
				mesh_data.indices.push_back(i * 6 + 5);

				mesh_data.indices.push_back(i * 6 + 3);
				mesh_data.indices.push_back(i * 6 + 4);
				mesh_data.indices.push_back(i * 6 + 5);

				mesh_data.indices.push_back(i * 6 + 5);
				mesh_data.indices.push_back(i * 6 + 4);
				mesh_data.indices.push_back(i * 6 + 2);

				mesh_data.indices.push_back(i * 6 + 3);
				mesh_data.indices.push_back(i * 6 + 1);
				mesh_data.indices.push_back(i * 6 + 4);
			}
		}

		Vertex get_mid_point(const Vertex& v0, const Vertex& v1)
		{
			Vector3F pos = 0.5f * (v0.position + v1.position);
			Vector3F normal = normalize(0.5f * (v0.normal + v1.normal));
			Vector4F tangent = normalize(0.5f * (v0.tangent + v1.tangent));
			Vector2F tex = 0.5f * (v0.uv + v1.uv);

			Vertex v;
			v.position = pos;
			v.normal = normal;
			v.tangent = tangent;
			v.uv = tex;

			return v;
		}


		Mesh create_sphere(float radius, uint32_t slice_count, uint32_t stack_count)
		{
			Mesh mesh;
			auto& submesh = mesh.submeshes.emplace_back();
			//
			// Compute the vertices stating at the top pole and moving down the stacks.
			//

			// Poles: note that there will be texture coordinate distortion as there is
			// not a unique point on the texture map to assign to the pole when mapping
			// a rectangular texture onto a sphere.
			Vertex topVertex({ 0.0f, +radius, 0.0f }, { 0.0f, +1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			Vertex bottomVertex({ 0.0f, -radius, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });

			submesh.vertices.push_back(topVertex);

			float phiStep = PI / stack_count;
			float thetaStep = 2.0f * PI / slice_count;

			// Compute vertices for each stack ring (do not count the poles as rings).
			for (uint32_t i = 1; i <= stack_count - 1; ++i)
			{
				float phi = i * phiStep;

				// vertices of ring.
				for (uint32_t j = 0; j <= slice_count; ++j)
				{
					float theta = j * thetaStep;

					Vertex v;

					// spherical to cartesian
					v.position.x = radius * sinf(phi) * cosf(theta);
					v.position.y = radius * cosf(phi);
					v.position.z = radius * sinf(phi) * sinf(theta);

					// Partial derivative of P with respect to theta
					v.tangent.x = -radius * sinf(phi) * sinf(theta);
					v.tangent.y = 0.0f;
					v.tangent.z = +radius * sinf(phi) * cosf(theta);

					v.tangent = normalize(v.tangent);
					v.normal = normalize(v.normal);

					v.uv.x = theta / 2.0f * PI;
					v.uv.y = phi / PI;

					submesh.vertices.push_back(v);
				}
			}

			submesh.vertices.push_back(bottomVertex);

			//
			// Compute indices for top stack.  The top stack was written first to the Vertex buffer
			// and connects the top pole to the first ring.
			//

			for (uint32_t i = 1; i <= slice_count; ++i)
			{
				submesh.indices.push_back(0);
				submesh.indices.push_back(i + 1);
				submesh.indices.push_back(i);
			}

			//
			// Compute indices for inner stacks (not connected to poles).
			//

			// offset the indices to the index of the first Vertex in the first ring.
			// This is just skipping the top pole Vertex.
			uint32_t baseIndex = 1;
			uint32_t ringFVerUVount = slice_count + 1;
			for (uint32_t i = 0; i < stack_count - 2; ++i)
			{
				for (uint32_t j = 0; j < slice_count; ++j)
				{
					submesh.indices.push_back(baseIndex + i * ringFVerUVount + j);
					submesh.indices.push_back(baseIndex + i * ringFVerUVount + j + 1);
					submesh.indices.push_back(baseIndex + (i + 1) * ringFVerUVount + j);

					submesh.indices.push_back(baseIndex + (i + 1) * ringFVerUVount + j);
					submesh.indices.push_back(baseIndex + i * ringFVerUVount + j + 1);
					submesh.indices.push_back(baseIndex + (i + 1) * ringFVerUVount + j + 1);
				}
			}

			//
			// Compute indices for bottom stack.  The bottom stack was written last to the Vertex buffer
			// and connects the bottom pole to the bottom ring.
			//

			// South pole Vertex was added last.
			uint32_t southPoleIndex = (uint32_t)submesh.vertices.size() - 1;

			// offset the indices to the index of the first Vertex in the last ring.
			baseIndex = southPoleIndex - ringFVerUVount;

			for (uint32_t i = 0; i < slice_count; ++i)
			{
				submesh.indices.push_back(southPoleIndex);
				submesh.indices.push_back(baseIndex + i);
				submesh.indices.push_back(baseIndex + i + 1);
			}

			return mesh;
		}


		Mesh create_geosphere(float radius, uint32_t subdivision_count)
		{
			Mesh mesh;
			auto& submesh = mesh.submeshes.emplace_back();

			// Put a cap on the number of subdivisions.
			subdivision_count = std::min<uint32_t>(subdivision_count, 6u);

			// Approximate a sphere by tessellating an icosahedron.

			const float X = 0.525731f;
			const float Z = 0.850651f;

			Vector3F pos[12] =
			{
				Vector3F(-X, 0.0f, Z),  Vector3F(X, 0.0f, Z),
				Vector3F(-X, 0.0f, -Z), Vector3F(X, 0.0f, -Z),
				Vector3F(0.0f, Z, X),   Vector3F(0.0f, Z, -X),
				Vector3F(0.0f, -Z, X),  Vector3F(0.0f, -Z, -X),
				Vector3F(Z, X, 0.0f),   Vector3F(-Z, X, 0.0f),
				Vector3F(Z, -X, 0.0f),  Vector3F(-Z, -X, 0.0f)
			};

			uint32_t k[60] =
			{
				1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,
				1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,
				3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0,
				10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7
			};

			submesh.vertices.resize(12);
			submesh.indices.assign(&k[0], &k[60]);

			for (uint32_t i = 0; i < 12; ++i)
				submesh.vertices[i].position = pos[i];

			for (uint32_t i = 0; i < subdivision_count; ++i)
				subdivide(submesh);

			// Project vertices onto sphere and scale.
			for (uint32_t i = 0; i < submesh.vertices.size(); ++i)
			{
				// Project onto unit sphere.
				submesh.vertices[i].position = normalize(submesh.vertices[i].position);

				// Project onto sphere.
				submesh.vertices[i].normal = radius * submesh.vertices[i].position;


				// Derive texture coordinates from spherical coordinates.
				float theta = atan2f(submesh.vertices[i].position.z, submesh.vertices[i].position.x);

				// Put in [0, 2pi].
				if (theta < 0.0f)
					theta += 2.0f * PI;

				float phi = acosf(submesh.vertices[i].position.y / radius);

				submesh.vertices[i].uv.x = theta / 2.0f * PI;
				submesh.vertices[i].uv.y = phi / PI;

				// Partial derivative of P with respect to theta
				submesh.vertices[i].tangent.x = -radius * sinf(phi) * sinf(theta);
				submesh.vertices[i].tangent.y = 0.0f;
				submesh.vertices[i].tangent.z = +radius * sinf(phi) * cosf(theta);

				submesh.vertices[i].tangent = normalize(submesh.vertices[i].tangent);
			}

			return mesh;
		}

		Mesh create_cylinder(float bottom_radius, float top_radius, float height, uint32_t slice_count, uint32_t stack_count)
		{
			Mesh mesh;
			auto& submesh = mesh.submeshes.emplace_back();

			//
			// build Stacks.
			// 

			float stack_height = height / stack_count;

			// Amount to increment radius as we move up each stack level from bottom to top.
			float radius_step = (top_radius - bottom_radius) / stack_count;

			uint32_t ring_count = stack_count + 1;

			// Compute vertices for each stack ring starting at the bottom and moving up.
			for (uint32_t i = 0; i < ring_count; ++i)
			{
				float y = -0.5f * height + i * stack_height;
				float r = bottom_radius + i * radius_step;

				// vertices of ring
				float dTheta = 2.0f * PI / slice_count;
				for (uint32_t j = 0; j <= slice_count; ++j)
				{
					Vertex vertex;

					float c = cosf(j * dTheta);
					float s = sinf(j * dTheta);

					vertex.position = Vector3F(r * c, y, r * s);

					vertex.uv.x = (float)j / slice_count;
					vertex.uv.y = 1.0f - (float)i / stack_count;

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
					vertex.tangent = Vector4F(-s, 0.0f, c, 1.0f);

					float dr = bottom_radius - top_radius;
					Vector4F bitangent(dr * c, -height, dr * s, 1.0f);

					vertex.normal = normalize(cross(Vector3F(vertex.tangent), Vector3F(bitangent)));
					submesh.vertices.push_back(vertex);
				}
			}

			// add one because we duplicate the first and last vertex per ring
			// since the texture coordinates are different.
			uint32_t ringVerUVount = slice_count + 1;

			// Compute indices for each stack.
			for (uint32_t i = 0; i < stack_count; ++i)
			{
				for (uint32_t j = 0; j < slice_count; ++j)
				{
					submesh.indices.push_back(i * ringVerUVount + j);
					submesh.indices.push_back((i + 1) * ringVerUVount + j);
					submesh.indices.push_back((i + 1) * ringVerUVount + j + 1);

					submesh.indices.push_back(i * ringVerUVount + j);
					submesh.indices.push_back((i + 1) * ringVerUVount + j + 1);
					submesh.indices.push_back(i * ringVerUVount + j + 1);
				}
			}

			build_cylinder_top_cap(bottom_radius, top_radius, height, slice_count, stack_count, submesh);
			build_cylinder_bottom_cap(bottom_radius, top_radius, height, slice_count, stack_count, submesh);

			return mesh;
		}

		void build_cylinder_top_cap(float bottom_radius, float top_radius, float height, uint32_t slice_count, uint32_t stack_count, Mesh::Submesh& mesh_data)
		{
			uint32_t baseIndex = (uint32_t)mesh_data.vertices.size();

			float y = 0.5f * height;
			float theta = 2.0f * PI / slice_count;

			// Duplicate cap ring vertices because the texture coordinates and normals differ.
			for (uint32_t i = 0; i <= slice_count; ++i)
			{
				float x = top_radius * cosf(i * theta);
				float z = top_radius * sinf(i * theta);

				// scale down by the height to try and make top cap texture coord area
				// proportional to base.
				float u = x / height + 0.5f;
				float v = z / height + 0.5f;

				mesh_data.vertices.push_back(Vertex({ x, y, z }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { u, v }));
			}

			// Cap center vertex.
			mesh_data.vertices.push_back(Vertex({ 0.0f, y, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }));

			// Index of center vertex.
			uint32_t centerIndex = (uint32_t)mesh_data.vertices.size() - 1;

			for (uint32_t i = 0; i < slice_count; ++i)
			{
				mesh_data.indices.push_back(centerIndex);
				mesh_data.indices.push_back(baseIndex + i + 1);
				mesh_data.indices.push_back(baseIndex + i);
			}
		}

		void build_cylinder_bottom_cap(float bottom_radius, float top_radius, float height, uint32_t slice_count, uint32_t stack_count, Mesh::Submesh& mesh_data)
		{
			// 
			// build bottom cap.
			//

			uint32_t base_index = (uint32_t)mesh_data.vertices.size();
			float y = -0.5f * height;

			// vertices of ring
			float dTheta = 2.0f * PI / slice_count;
			for (uint32_t i = 0; i <= slice_count; ++i)
			{
				float x = bottom_radius * cosf(i * dTheta);
				float z = bottom_radius * sinf(i * dTheta);

				// scale down by the height to try and make top cap texture coord area
				// proportional to base.
				float u = x / height + 0.5f;
				float v = z / height + 0.5f;

				mesh_data.vertices.push_back(Vertex({ x, y, z }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { u, v }));
			}

			// Cap center vertex.
			mesh_data.vertices.push_back(Vertex({ 0.0f, y, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }));

			// Cache the index of center vertex.
			uint32_t centerIndex = (uint32_t)mesh_data.vertices.size() - 1;

			for (uint32_t i = 0; i < slice_count; ++i)
			{
				mesh_data.indices.push_back(centerIndex);
				mesh_data.indices.push_back(base_index + i);
				mesh_data.indices.push_back(base_index + i + 1);
			}
		}

		Mesh create_grid(float width, float depth, uint32_t m, uint32_t n)
		{
			Mesh mesh;
			auto& submesh = mesh.submeshes.emplace_back();

			uint32_t verUVount = m * n;
			uint32_t faceCount = (m - 1) * (n - 1) * 2;

			//
			// Create the vertices.
			//

			float halfWidth = 0.5f * width;
			float halfDepth = 0.5f * depth;

			float dx = width / (n - 1);
			float dz = depth / (m - 1);

			float du = 1.0f / (n - 1);
			float dv = 1.0f / (m - 1);

			submesh.vertices.resize(verUVount);
			for (uint32_t i = 0; i < m; ++i)
			{
				float z = halfDepth - i * dz;
				for (uint32_t j = 0; j < n; ++j)
				{
					float x = -halfWidth + j * dx;

					submesh.vertices[i * n + j].position = Vector3F(x, 0.0f, z);
					submesh.vertices[i * n + j].normal = Vector3F(0.0f, 1.0f, 0.0f);
					submesh.vertices[i * n + j].tangent = Vector4F(1.0f, 0.0f, 0.0f, 1.0f);

					// Stretch texture over grid.
					submesh.vertices[i * n + j].uv.x = j * du;
					submesh.vertices[i * n + j].uv.y = i * dv;
				}
			}

			//
			// Create the indices.
			//

			submesh.indices.resize(faceCount * 3); // 3 indices per face

			// Iterate over each quad and compute indices.
			uint32_t k = 0;
			for (uint32_t i = 0; i < m - 1; ++i)
			{
				for (uint32_t j = 0; j < n - 1; ++j)
				{
					submesh.indices[k] = i * n + j;
					submesh.indices[k + 1] = i * n + j + 1;
					submesh.indices[k + 2] = (i + 1) * n + j;

					submesh.indices[k + 3] = (i + 1) * n + j;
					submesh.indices[k + 4] = i * n + j + 1;
					submesh.indices[k + 5] = (i + 1) * n + j + 1;

					k += 6; // next quad
				}
			}

			return mesh;
		}

		Mesh create_quad(float x, float y, float w, float h, float depth)
		{
			Mesh mesh;
			auto& submesh = mesh.submeshes.emplace_back();


			submesh.vertices.resize(4);
			submesh.indices.resize(6);

			// position coordinates specified in NDC space.
			submesh.vertices[0] = Vertex(
				{ x, y - h, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 0.0f, 1.0f }
			);

			submesh.vertices[1] = Vertex(
				{ x, y, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 0.0f, 0.0f }
			);

			submesh.vertices[2] = Vertex(
				{ x + w, y, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 1.0f, 0.0f }
			);

			submesh.vertices[3] = Vertex(
				{ x + w, y - h, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
				{ 1.0f, 1.0f }
			);

			submesh.indices[0] = 0;
			submesh.indices[1] = 1;
			submesh.indices[2] = 2;

			submesh.indices[3] = 0;
			submesh.indices[4] = 2;
			submesh.indices[5] = 3;

			return mesh;
		}

	}


}
