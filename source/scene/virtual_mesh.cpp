#include "virtual_mesh.h"
#include "geometry.h"
#include "scene.h"
#include "../core/tools/file.h"
#include <cstdint>

namespace fantasy
{
	void MeshOptimizer::BinaryHeap::resize(uint32_t index_count)
	{
		_current_size = 0;
		_index_count = index_count;
		_heap.clear(); _heap.resize(index_count);
		_keys.clear(); _keys.resize(index_count);
		_heap_indices.clear(); _heap_indices.resize(index_count);

		memset(_heap_indices.data(), 0xff, index_count * sizeof(uint32_t));
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
		assert(is_valid(index));

		float key = _keys[index];
		uint32_t curr_index = _heap_indices[index];
		if (curr_index == _current_size - 1)
		{
			_current_size--;
			_heap_indices[index] = INVALID_SIZE_32;
			return;
		}

		_heap[curr_index] = _heap[--_current_size];
		_heap_indices[_heap[curr_index]] = curr_index;
		_heap_indices[index] = INVALID_SIZE_32;

		if (key < _keys[_heap[curr_index]]) push_down(curr_index);
		else push_up(curr_index);
	}
	
	void MeshOptimizer::BinaryHeap::insert(float key,uint32_t index)
	{
		assert(!is_valid(index));

		uint32_t old_size = _current_size++;
		_heap[old_size] = index;
		_keys[index] = key;
		_heap_indices[index] = old_size;
		push_up(old_size);
	}

	void MeshOptimizer::BinaryHeap::push_up(uint32_t index)
	{
		uint32_t ix=_heap[index];
		uint32_t father_index=(index-1)>>1;
		while (index > 0 && _keys[ix] < _keys[_heap[father_index]])
		{
			_heap[index] = _heap[father_index];
			_heap_indices[_heap[index]] = index;
			index = father_index;
			father_index = (index-1) >> 1;
		}
		_heap[index] = ix;
		_heap_indices[_heap[index]] = index;
	}

	void MeshOptimizer::BinaryHeap::push_down(uint32_t index)
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

	MeshOptimizer::MeshOptimizer(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) :
		_vertices(vertices), 
		_indices(indices),
		_vertex_ref_count(vertices.size()),
		_vertex_table(vertices.size()),
		_index_table(indices.size()),
		_flags(indices.size()),
		_triangle_surfaces(indices.size() / 3.0f),
		_triangle_removed_set(indices.size() / 3.0f, false)
	{
		_remain_vertex_count = static_cast<uint32_t>(_vertices.size());
		_remain_triangle_count = static_cast<uint32_t>(_triangle_surfaces.size());

		for (uint32_t ix = 0; ix < _vertices.size(); ++ix)
		{
			_vertex_table.insert(hash(_vertices[ix].position), ix);
		}

		uint64_t edge_num = std::min(std::min(_indices.size(), 3 * _vertices.size() - 6), _triangle_surfaces.size() + _vertices.size());
		_edges.reserve(edge_num);
		_edges_begin_table.resize(edge_num);
		_edges_end_table.resize(edge_num);

		for (uint32_t ix = 0; ix < _indices.size(); ++ix)
		{
			_vertex_ref_count[_indices[ix]]++;
			const auto& vertex = _vertices[_indices[ix]];

			_index_table.insert(hash(vertex.position), ix);
			
			Vertex p0 = vertex;
			Vertex p1 = _vertices[_indices[triangle_edge_index_cycle(ix)]];

			uint32_t key0 = hash(p0.position);
			uint32_t key1 = hash(p1.position);
			if (key0 > key1)
			{
				std::swap(key0, key1);
				std::swap(p0, p1);
			}

			bool already_exist = false;
			for (uint32_t jx : _edges_begin_table[key0])
			{
				const auto& edge = _edges[jx];
				if (edge.first.position == p0.position && edge.second.position == p1.position) 
				{
					already_exist = true;
					break;
				}
			}
			if (!already_exist)
			{
				_edges_begin_table.insert(key0, static_cast<uint32_t>(_edges.size()));
				_edges_end_table.insert(key1, static_cast<uint32_t>(_edges.size()));
				_edges.emplace_back(std::make_pair(p0, p1));
			}
		}
	}


	bool MeshOptimizer::optimize(uint32_t target_triangle_num)
	{
		for (uint32_t ix = 0; ix < _triangle_surfaces.size(); ++ix) fix_triangle(ix);

		if (_remain_triangle_count <= target_triangle_num)
		{
			bool ret = compact();

			_vertices.resize(_remain_vertex_count);
			_indices.resize(_remain_triangle_count * 3);

			return ret;
		}

		_heap.resize(_edges.size());

		uint32_t ix = 0;
		for (const auto& edge : _edges)
		{
			float error = evaluate(edge.first, edge.second, false);
			_heap.insert(error, ix++);
		}

		_max_error = 0.0f;
		while (!_heap.empty())
		{
			uint32_t edge_index = _heap.top();

			// Excessive error.
			if (_heap.get_key(edge_index) >= 1e6) break;

			_heap.pop();
			
			const auto& edge = _edges[edge_index];
			_edges_begin_table.remove(hash(edge.first.position), edge_index);
			_edges_end_table.remove(hash(edge.second.position), edge_index);

			_max_error = std::max(evaluate(edge.first, edge.second, true), _max_error);

			if (_remain_triangle_count <= target_triangle_num) break;

			for (uint32_t ix : edge_need_reevaluate_indices)
			{
				const auto& reevaluate_edge = _edges[ix];
				float error = evaluate(reevaluate_edge.first, reevaluate_edge.second, false);
				_heap.insert(error, ix);
			}

			edge_need_reevaluate_indices.clear();
		}

		bool ret = compact();

		_vertices.resize(_remain_vertex_count);
		_indices.resize(_remain_triangle_count * 3);

		return ret;
	}

	void MeshOptimizer::lock_position(const float3& position)
	{
		for (uint32_t ix : _index_table[hash(position)])
		{
			if (_vertices[_indices[ix]].position == position)
			{
				_flags[ix] |= LockFlag;
			}
		}
	}

	void MeshOptimizer::fix_triangle(uint32_t triangle_index)
	{
		// 将需要去除的三角形在 _triangle_removed_set 设置为 true,
		// 将不需要去除的三角形生成 QuadricSurface 并存入 _triangle_surfaces,
		// 顺便将重复的三角形顶点合并.

		if (_triangle_removed_set[triangle_index]) return;

		uint32_t vertex_index0 = _indices[triangle_index * 3 + 0];
		uint32_t vertex_index1 = _indices[triangle_index * 3 + 1];
		uint32_t vertex_index2 = _indices[triangle_index * 3 + 2];

		// 第 triangle_index 个三角形的三个顶点.
		const float3& p0 = _vertices[vertex_index0].position;	
		const float3& p1 = _vertices[vertex_index1].position;	
		const float3& p2 = _vertices[vertex_index2].position;

		bool need_removed = p0 == p1 || p1 == p2 || p2 == p0;
		if (!need_removed)
		{
			// 若找到与该三角形顶点位置相同的其它顶点, 则将两个顶点合并, 
			// 合并方式就是把 _indices 中该顶点的 index 设置为位置相同顶点的 index.
			for (uint32_t ix = 0; ix < 3; ++ix)
			{
				uint32_t& vertex_index = _indices[triangle_index * 3 + ix];
				const auto& vertex_position = _vertices[vertex_index].position;
				uint32_t key = hash(vertex_position);
				for (uint32_t jx : _vertex_table[key])
				{
					if (jx == vertex_index)
					{
						break;
					}
					else if (vertex_position == _vertices[jx].position) 
					{
						assert(_vertex_ref_count[vertex_index] > 0);
						if (--_vertex_ref_count[vertex_index] == 0)
						{
							_vertex_table.remove(key, vertex_index);
							_remain_vertex_count--;
						}
						vertex_index = jx;
						if (vertex_index != INVALID_SIZE_32) _vertex_ref_count[vertex_index]++;
						break;
					}
				}
			}
			
			// 查找是否有其他三个顶点位置都完全相同的三角形.
			// ix != vertex_index_offset: 不能是该三角形本身.
			for (uint32_t ix : _index_table[hash(p0)])
			{
				if (
					ix != triangle_index * 3 &&
					vertex_index0 == _indices[ix] && 
					vertex_index1 == _indices[triangle_edge_index_cycle(ix)] && 
					vertex_index2 == _indices[triangle_edge_index_cycle(ix, 2)] 
				)
				{
					need_removed = true;
				}
			}
		}

		if (need_removed)
		{
			_triangle_removed_set.set_true(triangle_index);
			_remain_triangle_count--;

			// 将 _index_table 和 _vertex_table 内该三角形顶点哈希值全部去除.
			// 并 _indices 内该三角形顶点 index 设置为 INVALID_SIZE_32.
			for (uint32_t ix = 0; ix < 3; ++ix)
			{
				uint32_t index_index = triangle_index * 3 + ix;
				uint32_t& vertex_index = _indices[index_index];
				const auto& vertex_position = _vertices[vertex_index].position;

				uint32_t key = hash(vertex_position);
				_index_table.remove(key, index_index);

				if (vertex_index == INVALID_SIZE_32) continue;
				assert(_vertex_ref_count[vertex_index] > 0);
				if (--_vertex_ref_count[vertex_index] == 0)
				{
					_vertex_table.remove(key, vertex_index);
					_remain_vertex_count--;
				}
				vertex_index = INVALID_SIZE_32;
			}
		}
		else 
		{
			_triangle_surfaces[triangle_index] = QuadricSurface(
				Vector3<double>(p0), 
				Vector3<double>(p1), 
				Vector3<double>(p2)
			);
		}
	}

	bool MeshOptimizer::compact()
	{
		// 根据 _vertex_ref_count 和 _triangle_removed_set 来重新分配 _vertices 和 _indices.
		// 即将 _vertex_ref_count 为 0 的顶点和 _triangle_removed_set 为 true 的顶点 index 给去除.

		uint32_t count = 0;
		for (uint32_t ix = 0; ix < _vertices.size(); ++ix)
		{
			if (_vertex_ref_count[ix] > 0)
			{
				if (ix != count) _vertices[count] = _vertices[ix];
				_vertex_ref_count[ix] = count++;
			}
		}
		ReturnIfFalse(count == _remain_vertex_count);

		count = 0;
		for (uint32_t ix = 0; ix < _triangle_surfaces.size(); ++ix)
		{
			if (!_triangle_removed_set[ix])
			{
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					_indices[count * 3 + jx] = _vertex_ref_count[_indices[ix * 3 + jx]];
				}
				count++;
			}
		}
		return count == _remain_triangle_count;
	}

	float MeshOptimizer::evaluate(const Vertex& p0, const Vertex& p1, bool if_merge)
	{
		if (p0.position == p1.position) return 0.0f;

		// error 是一个综合误差值，用于量化合并两个顶点 (p0 和 p1) 的代价, 由以下几方面进行量化:
		// 1. 如果合并后影响的邻接三角形数量超过 24 个，误差会增加, 防止合并导致过于密集的几何区域.
		// 2. 如果 p0 或 p1 被锁定 (lock0 或 lock1 为 true), 误差会显著增加. (例如边界的顶点或需要保持位置的关键顶点)
		// 3. 通过二次曲面 (QuadricSurface) 计算合并后顶点位置 p 到原始曲面的几何偏差.
		// 4. 如果合并后的顶点位置 p 距离原始顶点 p0 和 p1 的总距离超过两者间距的 2 倍，强制使用平均位置 average_p.
		

		float error = 0.0f;

		std::vector<uint32_t> adjacency_triangle_indices;
		auto func_build_adjacency_triangles = [this, &adjacency_triangle_indices](const float3& vertex, bool& lock)
		{
			for (uint32_t ix : _index_table[hash(vertex)])
			{
				if (_vertices[_indices[ix]].position == vertex)
				{
					uint32_t triangle_index = ix / 3;
					if ((_flags[triangle_index * 3] & AdjacencyFlag) == 0)
					{
						_flags[triangle_index * 3] |= AdjacencyFlag;
						adjacency_triangle_indices.push_back(triangle_index);
					}

					if (_flags[ix] & LockFlag) lock = true;
				}
			}
		};

		bool lock0 = false, lock1 = false;
		func_build_adjacency_triangles(p0.position, lock0);
		func_build_adjacency_triangles(p1.position, lock1);

		if (adjacency_triangle_indices.empty()) return 0.0f;
		if (adjacency_triangle_indices.size() > 24u) error += 0.5f * (adjacency_triangle_indices.size() - 24u);

		QuadricSurface surface;
		for (uint32_t ix : adjacency_triangle_indices) surface = merge(surface, _triangle_surfaces[ix]);

		Vertex average_p;
		average_p.position = (p0.position + p1.position) * 0.5f;
		average_p.normal = normalize(p0.normal + p1.normal);
		average_p.tangent = normalize(p0.tangent + p1.tangent);
		average_p.uv = (p0.uv + p1.uv) * 0.5f;

		Vertex p = average_p;
		if (lock0 && lock1) error += 1e8;
		if (lock0 && !lock1) p = p0;
		else if (!lock0 && lock1) p = p1;
		else if (!surface.get_vertex(p.position)) p = average_p;
		
		if (distance(p.position, p0.position) + distance(p.position, p1.position) > 2.0f * distance(p0.position, p1.position)) p = average_p; 

		error += surface.distance_to_surface(p.position);

		if (if_merge)
		{
			merge_begin(p0.position);
			merge_begin(p1.position);

			// 将邻接的顶点全部合并.
			for (uint32_t ix : adjacency_triangle_indices)
			{
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					uint32_t vertex_index = ix * 3 + jx;
					Vertex& vertex = _vertices[_indices[vertex_index]];
					if (vertex.position == p0.position || vertex.position == p1.position)
					{
						vertex = p;
						if (lock0 || lock1)
						{
							_flags[vertex_index] |= LockFlag;
						}
					}
				}
			}
			// 根据 removed_edge_indices 中的已经去除的边, 将 _edges 的首尾顶点全部与 p 合并.
			for (uint32_t ix : removed_edge_indices)
			{
				auto& edge = _edges[ix];
				if (edge.first.position == p0.position || edge.first.position == p1.position) edge.first = p;
				if (edge.second.position == p0.position || edge.second.position == p1.position) edge.second = p;
			}

			mergen_end();

			std::vector<uint32_t> adjacency_vertex_indices;
			adjacency_vertex_indices.reserve((adjacency_triangle_indices.size() * 3));
			for (uint32_t ix : adjacency_triangle_indices)
			{
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					adjacency_vertex_indices.push_back(_indices[ix * 3 + jx]);
				}
			}

			std::sort(adjacency_vertex_indices.begin(), adjacency_vertex_indices.end());

			// 去除重复值.
			adjacency_vertex_indices.erase(
				std::unique(adjacency_vertex_indices.begin(), adjacency_vertex_indices.end()), 
				adjacency_vertex_indices.end()
			);

			for (uint32_t vertex_index : adjacency_vertex_indices)
			{
				const auto& vertex_position = _vertices[vertex_index].position;
				uint32_t key = hash(vertex_position);
				for (uint32_t ix : _edges_begin_table[key])
				{
					if (_edges[ix].first.position == vertex_position && _heap.is_valid(ix))
					{
						_heap.remove(ix);
						edge_need_reevaluate_indices.push_back(ix);
					}
				}
				for (uint32_t ix : _edges_end_table[key])
				{
					if (_edges[ix].second.position == vertex_position && _heap.is_valid(ix))
					{
						_heap.remove(ix);
						edge_need_reevaluate_indices.push_back(ix);
					}
				}
			}
			
			for (uint32_t triangle_index : adjacency_triangle_indices) fix_triangle(triangle_index);
		}	

		for (uint32_t triangle_index : adjacency_triangle_indices) _flags[triangle_index * 3] &= ~AdjacencyFlag;

		return error;
	}

	void MeshOptimizer::merge_begin(const float3& p)
	{
		// 将该顶点在 _vertex_table, _index_table, _edges_begin_table, _edges_end_table 中去除
		// 并添加到 removed_vertex_indices, removed_index_indices, removed_edge_indices.

		uint32_t key = hash(p);
		for (uint32_t ix : _vertex_table[key])
		{
			if (_vertices[ix].position == p)
			{
				_vertex_table.remove(key, ix);
				removed_vertex_indices.push_back(ix);
			}
		}
		for (uint32_t ix : _index_table[key])
		{
			if (_vertices[_indices[ix]].position == p)
			{
				_index_table.remove(key, ix);
				removed_index_indices.push_back(ix);
			}
		}
		for (uint32_t ix : _edges_begin_table[key])
		{
			if (_edges[ix].first.position == p)
			{
				_edges_begin_table.remove(key, ix);
				_edges_end_table.remove(hash(_edges[ix].second.position), ix);
				removed_edge_indices.push_back(ix);
			}
		}
		for (uint32_t ix : _edges_end_table[key])
		{
			if (_edges[ix].second.position == p)
			{
				_edges_begin_table.remove(hash(_edges[ix].first.position), ix);
				_edges_end_table.remove(key, ix);
				removed_edge_indices.push_back(ix);
			}
		}
	}

	void MeshOptimizer::mergen_end()
	{
		for (uint32_t ix : removed_vertex_indices)
		{
			_vertex_table.insert(hash(_vertices[ix].position), ix);
		}
		for (uint32_t ix : removed_index_indices)
		{
			_index_table.insert(hash(_vertices[_indices[ix]].position), ix);
		}
		for (uint32_t ix : removed_edge_indices)
		{
			auto& edge = _edges[ix];
			
			uint32_t h0 = hash(edge.first.position);
			uint32_t h1 = hash(edge.second.position);
			if (h0 > h1)
			{
				std::swap(h0, h1);
				std::swap(edge.first, edge.second);
			}

			if (edge.first == edge.second)
			{
				_heap.remove(ix);
				continue;
			}

			bool alread_exist = false;
			for (uint32_t ix : _edges_begin_table[h0])
			{
				const auto& edge_copy = _edges[ix];
				if (edge_copy.first == edge.first && edge_copy.second == edge.second) 
				{
					alread_exist = true;
					break;
				}
			}
			if (!alread_exist)
			{
				_edges_begin_table.insert(h0, ix);
				_edges_end_table.insert(h1, ix);
			}
			else 
			{
				_heap.remove(ix);
			}

		}
		
		removed_edge_indices.clear();
		removed_vertex_indices.clear();
		removed_index_indices.clear();
	}

	bool VirtualMesh::build(const Mesh* mesh)
	{
		for (const auto& submesh : mesh->submeshes)
		{
			_indices.insert(_indices.end(), submesh.indices.begin(), submesh.indices.end());
			_vertices.insert(_vertices.end(), submesh.vertices.begin(), submesh.vertices.end());

			auto& virtual_submesh = _submeshes.emplace_back();
			
			MeshOptimizer optimizer(_vertices, _indices);
			ReturnIfFalse(optimizer.optimize(_indices.size()));
			
			ReturnIfFalse(cluster_triangles(virtual_submesh));
			uint32_t level_offset = 0;
			uint32_t mip_level = 0;
			while (true)
			{
				uint32_t level_cluster_count = virtual_submesh.clusters.size() - level_offset;
				if(level_cluster_count<=1) break;
			
				uint32_t old_cluster_num = virtual_submesh.clusters.size();
				uint32_t old_group_num = virtual_submesh.cluster_groups.size();
			
				ReturnIfFalse(build_cluster_groups(virtual_submesh, level_offset, level_cluster_count, mip_level));
			
				for(uint32_t ix = old_group_num; ix < virtual_submesh.cluster_groups.size(); ix++)
				{
					ReturnIfFalse(build_parent_clusters(virtual_submesh, ix));
				}
				
				level_offset = old_cluster_num;
				mip_level++;
			}
			virtual_submesh.mip_levels = mip_level + 1;
			
			_indices.clear();
			_vertices.clear();
		}
		_indices.shrink_to_fit();
		_vertices.shrink_to_fit();
		
		return true;
	}
	
	uint32_t VirtualMesh::edge_hash(const float3& p0, const float3& p1)
	{
		uint32_t key0 = hash(p0);
		uint32_t key1 = hash(p1); 
		return murmur_mix(murmur_add(key0, key1));
	}

 
	void VirtualMesh::build_adjacency_graph(
		const std::vector<Vertex>& vertices, 
		const std::vector<uint32_t>& indices, 
		SimpleGraph& edge_link_graph, 
		SimpleGraph& adjacency_graph
	)
	{
		// 这里 triangle_adjacency_graph 存储的是三角形的连接情况, 而非顶点.

		HashTable edge_table(static_cast<uint32_t>(indices.size()));
		edge_link_graph.resize(indices.size());

		for (uint32_t edge_start_index = 0; edge_start_index < indices.size(); ++edge_start_index)
		{
			const float3& p0 = vertices[indices[edge_start_index]].position;
			const float3& p1 = vertices[indices[triangle_edge_index_cycle(edge_start_index)]].position;

			// 查找表示相邻三角形的共享顶点和相对边.
			edge_table.insert(edge_hash(p0, p1), edge_start_index);
			for (uint32_t edge_end_index : edge_table[edge_hash(p1, p0)])
			{
				if (
					p1 == vertices[indices[edge_end_index]].position && 
					p0 == vertices[indices[triangle_edge_index_cycle(edge_end_index)]].position
				)
				{
					// 增加权重.
					edge_link_graph[edge_start_index][edge_end_index]++;
					edge_link_graph[edge_end_index][edge_start_index]++;
				}
			}
		}

		// 根据每个三角形所邻接三角形数增加权重.
		adjacency_graph.resize(edge_link_graph.size() / 3);
		uint32_t edge_start_node = 0;
		for (const auto& edge_end_node_weights : edge_link_graph)
		{
			for (const auto& [edge_end_node, weight] : edge_end_node_weights)
			{
				adjacency_graph[edge_start_node / 3][edge_end_node / 3] += 1;
			}
			edge_start_node++;
		}
	}

	bool VirtualMesh::cluster_triangles(VirtualSubmesh& submesh)
	{
		// 用 GraphPartitionar 将网格三角形划分为一个个分区, 每个分区即是一个 cluster.

		SimpleGraph edge_link_graph;
		SimpleGraph triangle_adjacency_graph;
		build_adjacency_graph(_vertices, _indices, edge_link_graph, triangle_adjacency_graph);

		GraphPartitionar partitionar;
		partitionar.partition_graph(triangle_adjacency_graph, MeshCluster::cluster_size - 4, MeshCluster::cluster_size);

		for (const auto& [start, end] : partitionar.part_ranges)
		{
			auto& cluster = submesh.clusters.emplace_back();

			// key: 在 _indices 中该 vertex 的 index 值; 
			// hash value: 在 cluster.vertices 中该 vertex 所在的位置序号.
			std::unordered_map<uint32_t, uint32_t> cluster_vertex_index_map;
			for (uint32_t ix = start; ix < end; ++ix)
			{
				uint32_t triangle_index = partitionar.node_indices[ix];
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					// 填充 cluster.vertices 和 cluster.indices.
					uint32_t edge_start_index = triangle_index * 3 + jx;
					uint32_t vertex_index = _indices[edge_start_index];
					if (cluster_vertex_index_map.find(vertex_index) == cluster_vertex_index_map.end())
					{
						cluster_vertex_index_map[vertex_index] = static_cast<uint32_t>(cluster.vertices.size());
						cluster.vertices.push_back(_vertices[vertex_index]);
					}

					// edge_link_graph 存储的未分区网格的边, 若其中边的首顶点和尾顶点不在同一个分区内, 
					// 说明该条边为边界, 将其加入到 cluster.external_edges.
					bool is_external = false;
					for (const auto& [edge_end_index, weight] : edge_link_graph[edge_start_index])
					{
						uint32_t remapped_edge_end_index = partitionar.node_map[edge_end_index / 3];
						
						// Edge_end_index(remapped) in different partitions indicate a boundary
						if (remapped_edge_end_index < start || remapped_edge_end_index >= end)
						{
							is_external = true;
							break;
						}
					}
					if (is_external)
					{
						cluster.external_edges.push_back(cluster.indices.size());
					}
					cluster.indices.push_back(cluster_vertex_index_map[vertex_index]);
				}
			}

			std::vector<float3> vertex_positions(cluster.vertices.size());
			for (uint32_t ix = 0; ix < vertex_positions.size(); ++ix) 
			{
				vertex_positions[ix] = cluster.vertices[ix].position;
			}

			cluster.bounding_sphere = Sphere(vertex_positions);
			cluster.lod_bounding_sphere = cluster.bounding_sphere;
		}

		return true;
	}

	bool VirtualMesh::build_cluster_groups(VirtualSubmesh& submesh, uint32_t level_offset, uint32_t level_cluster_count, uint32_t mip_level)
	{
		// 即创建 clusters 的 LOD.

		const std::span<MeshCluster> clusters_view(submesh.clusters.begin() + level_offset, level_cluster_count);

		std::vector<uint32_t> edge_cluster_map;	// 含有边界的 cluster 的 index.
		std::vector<uint32_t> cluster_edge_offset_map;	// 划分 cluster_edge_map 每个 cluster.
		std::vector<std::pair<uint32_t, uint32_t>> cluster_edge_map;	// [cluster_index, edge_start_index]

		
		for (uint32_t cluster_index = 0; cluster_index < clusters_view.size(); ++cluster_index)
		{
			const auto& cluster = clusters_view[cluster_index];
			ReturnIfFalse(cluster.mip_level == mip_level);

			cluster_edge_offset_map.push_back(static_cast<uint32_t>(edge_cluster_map.size()));
			for (uint32_t edge_start_index : cluster.external_edges)
			{
				cluster_edge_map.push_back(std::make_pair(cluster_index, edge_start_index));
				edge_cluster_map.push_back(cluster_index);
			}
		}

		SimpleGraph edge_link_graph(cluster_edge_map.size());

		HashTable edge_table(static_cast<uint32_t>(cluster_edge_map.size()));
		for (uint32_t ix = 0; ix < cluster_edge_map.size(); ++ix)
		{
			const auto& [cluster_index, edge_start_index] = cluster_edge_map[ix];
			const auto& vertices = clusters_view[cluster_index].vertices;
			const auto& indices = clusters_view[cluster_index].indices;

			const auto& p0 = vertices[indices[edge_start_index]].position;
			const auto& p1 = vertices[indices[triangle_edge_index_cycle(edge_start_index)]].position;
			edge_table.insert(edge_hash(p0, p1), ix);
			for (uint32_t jx : edge_table[edge_hash(p1, p0)])
			{
				const auto& [another_cluster_index, another_edge_start_index] = cluster_edge_map[jx];
				const auto& another_vertices = clusters_view[another_cluster_index].vertices;
				const auto& another_indices = clusters_view[another_cluster_index].indices;

				if (
					p1 == another_vertices[another_indices[another_edge_start_index]].position && 
					p0 == another_vertices[another_indices[triangle_edge_index_cycle(another_edge_start_index)]].position
				)
				{
					edge_link_graph[ix][jx]++;
					edge_link_graph[jx][ix]++;
				}
			}
		}

		// 根据各个 cluster 边界的边首尾所在的 cluster, 所创建的图, 其节点为 cluster.
		SimpleGraph cluster_graph(level_cluster_count);
		
		for (uint32_t edge_start_index = 0; edge_start_index < edge_link_graph.size(); ++edge_start_index)
		{
			const auto& edge_end_node_weights = edge_link_graph[edge_start_index];
			for (const auto& [edge_end_node, weight] : edge_end_node_weights)
			{
				cluster_graph[edge_cluster_map[edge_start_index]][edge_cluster_map[edge_end_node]] += weight;
			}
		}


		// 以 cluster 进行划分, 原理与 cluster_triangles() 差不多.
		GraphPartitionar partitionar;
		partitionar.partition_graph(cluster_graph, MeshClusterGroup::group_size - 4, MeshClusterGroup::group_size);

		for (auto [start, end] : partitionar.part_ranges)
		{
			auto& cluster_group = submesh.cluster_groups.emplace_back();
			cluster_group.mip_level = mip_level;

			for (uint32_t ix = start; ix < end; ++ix)
			{
				uint32_t cluster_index = partitionar.node_indices[ix];
				submesh.clusters[cluster_index + level_offset].group_id = static_cast<uint32_t>(submesh.cluster_groups.size() - 1);
            	cluster_group.cluster_indices.push_back(cluster_index + level_offset);

				for (
					uint32_t edge_start_index = cluster_edge_offset_map[cluster_index]; 
					edge_start_index < edge_cluster_map.size() && cluster_index == edge_cluster_map[edge_start_index]; 
					++edge_start_index
				)
				{
					bool is_external = false;
					for (const auto& [edge_end_index, weight] : edge_link_graph[edge_start_index])
					{
						uint32_t remapped_cluster_index = partitionar.node_map[edge_cluster_map[edge_end_index]];
						
						// Edge_end_index(remapped) in different partitions indicate a boundary
						if (remapped_cluster_index < start || remapped_cluster_index >= end)
						{
							is_external = true;
							break;
						}
					}
					if (is_external)
					{
						uint32_t edge_end_index = cluster_edge_map[edge_start_index].second;
						cluster_group.external_edges.push_back(std::make_pair(cluster_index + level_offset, edge_end_index));
					}
				}
			}
		}

		return true;
	}

	bool VirtualMesh::build_parent_clusters(VirtualSubmesh& submesh, uint32_t cluster_group_index)
	{
		// 根据上一级 cluster group 建立下一级 mip_level 的 clusters.

		auto& cluster_group = submesh.cluster_groups[cluster_group_index];

		uint32_t index_offset = 0;
		float parent_lod_error = 0.0f;
		std::vector<Sphere> parent_lod_bounding_spheres;
		std::vector<uint32_t> cluster_indices;
		std::vector<Vertex> cluster_vertices;
		for (auto cluster_index : cluster_group.cluster_indices)
		{
			const auto& cluster = submesh.clusters[cluster_index];
			for (const auto& p : cluster.vertices) cluster_vertices.push_back(p);
			for (const auto& i : cluster.indices) cluster_indices.push_back(i + index_offset);
			index_offset += static_cast<uint32_t>(cluster.vertices.size());

			parent_lod_bounding_spheres.push_back(cluster.lod_bounding_sphere);
			parent_lod_error = std::max(parent_lod_error, cluster.lod_error);
		}
		
		cluster_group.bounding_sphere = merge(parent_lod_bounding_spheres);

		// 将边界顶点加入 edge_table 中, 并在 optimizer 中锁住, 以进行 optimize().
		MeshOptimizer optimizer(cluster_vertices, cluster_indices);
		HashTable edge_table(static_cast<uint32_t>(cluster_group.external_edges.size()));
		for (uint32_t ix = 0; ix < cluster_group.external_edges.size(); ++ix)
		{
			const auto& [cluster_index, edge_index] = cluster_group.external_edges[ix];
			const auto& vertices = submesh.clusters[cluster_index].vertices;
        	const auto& vertex_indices = submesh.clusters[cluster_index].indices;
			
			const float3& p0 = vertices[vertex_indices[edge_index]].position;
			const float3& p1 = vertices[vertex_indices[triangle_edge_index_cycle(edge_index)]].position;
			edge_table.insert(edge_hash(p0, p1), ix);
			optimizer.lock_position(p0);
			optimizer.lock_position(p1);
		}
		// 接下来就是如上的流程, optimize() 后分割图建立新的下一级 cluster.

		ReturnIfFalse(optimizer.optimize(
			(MeshCluster::cluster_size - 2) * (static_cast<uint32_t>(cluster_group.cluster_indices.size()) / 2)
		));
		parent_lod_error = std::max(parent_lod_error, std::sqrt(optimizer._max_error));
		cluster_group.parent_lod_error = parent_lod_error;

		SimpleGraph edge_link_graph;
		SimpleGraph triangle_adjacency_graph;
		build_adjacency_graph(
			cluster_vertices,
			cluster_indices,
			edge_link_graph, 
			triangle_adjacency_graph
		);

		GraphPartitionar partitionar;
		partitionar.partition_graph(triangle_adjacency_graph, MeshCluster::cluster_size - 4, MeshCluster::cluster_size);

		for (const auto& [start, end] : partitionar.part_ranges)
		{
			auto& cluster = submesh.clusters.emplace_back();

			// Map the vertices in _vertices to the _clusters.
			std::unordered_map<uint32_t, uint32_t> cluster_vertex_index_map;
			for (uint32_t ix = start; ix < end; ++ix)
			{
				uint32_t triangle_index = partitionar.node_indices[ix];
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					uint32_t edge_start_index = triangle_index * 3 + jx;
					uint32_t vertex_index = cluster_indices[edge_start_index];
					if (cluster_vertex_index_map.find(vertex_index) == cluster_vertex_index_map.end())
					{
						cluster_vertex_index_map[vertex_index] = static_cast<uint32_t>(cluster.vertices.size());
						cluster.vertices.push_back(cluster_vertices[vertex_index]);
					}

					bool is_external = false;
					for (const auto& [edge_end_index, weight] : edge_link_graph[edge_start_index])
					{
						uint32_t remapped_triangle_index = partitionar.node_map[edge_end_index / 3];
						
						// Edge_end_index(remapped) in different partitions indicate a boundary
						if (remapped_triangle_index < start || remapped_triangle_index >= end)
						{
							is_external = true;
							break;
						}
					}
					const float3& p0 = cluster_vertices[vertex_index].position;
					const float3& p1 = cluster_vertices[cluster_indices[triangle_edge_index_cycle(edge_start_index)]].position;
					if (!is_external)
					{
						for (uint32_t jx : edge_table[edge_hash(p0, p1)])
						{
							const auto& [cluster_index, edge_index] = cluster_group.external_edges[jx];
							auto& positions = submesh.clusters[cluster_index].vertices;
        					auto& vertex_indices = submesh.clusters[cluster_index].indices;

							if (
								p0 == positions[vertex_indices[edge_index]].position && 
								p1 == positions[vertex_indices[triangle_edge_index_cycle(edge_index)]].position
							)
							{
								is_external = true;
								break;
							}
						}
					}

					if (is_external)
					{
						cluster.external_edges.push_back(cluster.indices.size());
					}
					cluster.indices.push_back(cluster_vertex_index_map[vertex_index]);
				}
			}

			std::vector<float3> vertex_positions(cluster.vertices.size());
			for (uint32_t ix = 0; ix < vertex_positions.size(); ++ix) 
			{
				vertex_positions[ix] = cluster.vertices[ix].position;
			}

			cluster.mip_level = cluster_group.mip_level + 1;
			cluster.bounding_sphere = Sphere(vertex_positions);

			cluster.lod_bounding_sphere = cluster_group.bounding_sphere;
			cluster.lod_error = parent_lod_error;
		}
		return true;
	}

    bool SceneSystem::publish(World* world, const event::OnComponentAssigned<VirtualMesh>& event)
	{
		VirtualMesh* virtual_mesh = event.component;
		Mesh* mesh = event.entity->get_component<Mesh>();
		
		std::string* name = event.entity->get_component<std::string>();
		std::string cache_path = std::string(PROJ_DIR) + "asset/cache/virtual_mesh/" + *name + ".vm";

		bool loaded_from_cache = false;

		if (is_file_exist(cache_path.c_str()))
		{
			serialization::BinaryInput input(cache_path);
			
			uint32_t cluster_size = 0, group_size = 0;
			input(cluster_size, group_size);
			
			if (cluster_size == MeshCluster::cluster_size && group_size == MeshClusterGroup::group_size)
			{
				uint64_t submesh_size = 0;
				input(submesh_size);
				virtual_mesh->_submeshes.resize(submesh_size);

				for (auto& virtual_submesh : virtual_mesh->_submeshes)
				{
					uint64_t cluster_size = 0, cluster_group_size = 0;
					input(virtual_submesh.mip_levels, cluster_size, cluster_group_size);

					virtual_submesh.clusters.resize(cluster_size);
					virtual_submesh.cluster_groups.resize(cluster_group_size);

					for (auto& cluster : virtual_submesh.clusters)
					{
						uint64_t vertex_count = 0;
						input(vertex_count);
						cluster.vertices.resize(vertex_count);
						for (auto& vertex : cluster.vertices)
						{
							input(
								vertex.position.x,
								vertex.position.y,
								vertex.position.z,
								vertex.normal.x,
								vertex.normal.y,
								vertex.normal.z,
								vertex.tangent.x,
								vertex.tangent.y,
								vertex.tangent.z,
								vertex.uv.x,
								vertex.uv.y
							);
						}

						input(
							cluster.indices,
							cluster.external_edges,
							cluster.group_id,
							cluster.mip_level,
							cluster.lod_error,
							cluster.bounding_sphere.center.x,
							cluster.bounding_sphere.center.y,
							cluster.bounding_sphere.center.z,
							cluster.bounding_sphere.radius,
							cluster.lod_bounding_sphere.center.x,
							cluster.lod_bounding_sphere.center.y,
							cluster.lod_bounding_sphere.center.z,
							cluster.lod_bounding_sphere.radius
						);
					}
					for (auto& cluster_group : virtual_submesh.cluster_groups)
					{
						input(cluster_group.mip_level, cluster_group.cluster_indices);

						uint64_t external_edge_count = 0;
						input(external_edge_count);
						cluster_group.external_edges.resize(external_edge_count);
						for (auto& external_edge : cluster_group.external_edges)
						{
							input(external_edge.first, external_edge.second);
						}

						input(
							cluster_group.bounding_sphere.center.x,
							cluster_group.bounding_sphere.center.y,
							cluster_group.bounding_sphere.center.z,
							cluster_group.bounding_sphere.radius,
							cluster_group.parent_lod_error
						);
					}
				}
				loaded_from_cache = true;
			}
		}

		if (!loaded_from_cache)
		{
			ReturnIfFalse(virtual_mesh->build(mesh));

			serialization::BinaryOutput output(cache_path);

			output(
				MeshCluster::cluster_size, 
				MeshClusterGroup::group_size,
				virtual_mesh->_submeshes.size()
			);

			for (const auto& virtual_submesh : virtual_mesh->_submeshes)
			{
				output(
					virtual_submesh.mip_levels, 
					virtual_submesh.clusters.size(), 
					virtual_submesh.cluster_groups.size()
				);

				for (const auto& cluster : virtual_submesh.clusters)
				{
					for (const auto& vertex : cluster.vertices)
					{
						output(
							vertex.position.x,
							vertex.position.y,
							vertex.position.z,
							vertex.normal.x,
							vertex.normal.y,
							vertex.normal.z,
							vertex.tangent.x,
							vertex.tangent.y,
							vertex.tangent.z,
							vertex.uv.x,
							vertex.uv.y
						);
					}

					output(
						cluster.indices,
						cluster.external_edges,
						cluster.group_id,
						cluster.mip_level,
						cluster.lod_error,
						cluster.bounding_sphere.center.x,
						cluster.bounding_sphere.center.y,
						cluster.bounding_sphere.center.z,
						cluster.bounding_sphere.radius,
						cluster.lod_bounding_sphere.center.x,
						cluster.lod_bounding_sphere.center.y,
						cluster.lod_bounding_sphere.center.z,
						cluster.lod_bounding_sphere.radius
					);
				}
				for (auto& cluster_group : virtual_submesh.cluster_groups)
				{
					output(
						cluster_group.mip_level, 
						cluster_group.cluster_indices,
						cluster_group.external_edges.size()
					);

					for (const auto& external_edge : cluster_group.external_edges)
					{
						output(external_edge.first, external_edge.second);
					}

					output(
						cluster_group.bounding_sphere.center.x,
						cluster_group.bounding_sphere.center.y,
						cluster_group.bounding_sphere.center.z,
						cluster_group.bounding_sphere.radius,
						cluster_group.parent_lod_error
					);
				}
			}
		}

		ReturnIfFalse(virtual_mesh->_submeshes.size() == mesh->submeshes.size());
		
		uint32_t current_geometry_id = mesh->submesh_global_base_id;
		for (auto& virtual_submesh : virtual_mesh->_submeshes)
		{
			for (auto& cluster : virtual_submesh.clusters)
			{
				cluster.geometry_id = current_geometry_id;
			}
			current_geometry_id++;
		}
		return true;
	}
}