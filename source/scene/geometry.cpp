#include "geometry.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

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
		assert(Is_valid(index));

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
		assert(Is_valid(index));

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

	MeshOptimizer::MeshOptimizer(std::vector<Vector3F>& vertices, std::vector<uint32_t>& indices) :
		_vertices(vertices), 
		_indices(indices),
		_vertex_ref_count(vertices.size()),
		_vertex_table(vertices.size()),
		_index_table(indices.size()),
		_flags(indices.size()),
		_triangle_surfaces(indices.size() / 3.0f),
		_triangle_removed_array(indices.size() / 3.0f, false)
	{
		_remain_vertex_num = static_cast<uint32_t>(_vertices.size());
		_remain_triangle_num = static_cast<uint32_t>(_triangle_surfaces.size());

		for (uint32_t ix = 0; ix < _vertices.size(); ++ix)
		{
			_vertex_table.insert(hash(_vertices[ix]), ix);
		}

		uint64_t edge_num = std::min(std::min(_indices.size(), 3 * _vertices.size() - 6), _triangle_surfaces.size() + _vertices.size());
		_edges.resize(edge_num);
		_edges_begin_table.resize(edge_num);
		_edges_end_table.resize(edge_num);

		for (uint32_t ix = 0; ix < _indices.size(); ++ix)
		{
			_vertex_ref_count[_indices[ix]]++;
			const auto& vertex_position = _vertices[_indices[ix]];

			_index_table.insert(hash(vertex_position), ix);
			
			Vector3F p0 = vertex_position;
			Vector3F p1 = _vertices[_indices[triangle_index_cycle3(ix)]];

			uint32_t key0 = hash(p0);
			uint32_t key1 = hash(p1);
			if (key0 > key1)
			{
				std::swap(key0, key1);
				std::swap(p0, p1);
			}

			bool already_exist = false;
			for (uint32_t ix : _edges_begin_table[key0])
			{
				const auto& edge = _edges[ix];
				if (edge.first == p0 && edge.second == p1) 
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

		if (_remain_triangle_num <= target_triangle_num)
		{
			bool ret = compact();

			_vertices.resize(_remain_vertex_num);
			_indices.resize(_remain_triangle_num * 3);

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
			_edges_begin_table.remove(hash(edge.first), edge_index);
			_edges_end_table.remove(hash(edge.second), edge_index);

			_max_error = std::max(evaluate(edge.first, edge.second, true), _max_error);

			if (_remain_triangle_num <= target_triangle_num) break;

			for (uint32_t ix : edge_need_reevaluate_indices)
			{
				const auto& reevaluate_edge = _edges[ix];
				float error = evaluate(reevaluate_edge.first, reevaluate_edge.second, false);
				_heap.insert(error, ix++);
			}

			edge_need_reevaluate_indices.clear();
		}

		bool ret = compact();

		_vertices.resize(_remain_vertex_num);
		_indices.resize(_remain_triangle_num * 3);

		return ret;
	}

	void MeshOptimizer::lock_position(Vector3F position)
	{
		for (uint32_t ix : _index_table[hash(position)])
		{
			if (_vertices[_indices[ix]] == position)
			{
				_flags[ix] |= LockFlag;
			}
		}
	}

	void MeshOptimizer::fix_triangle(uint32_t triangle_index)
	{
		if (_triangle_removed_array[triangle_index]) return;

		uint32_t vertex_index0 = _indices[triangle_index * 3 + 0];
		uint32_t vertex_index1 = _indices[triangle_index * 3 + 1];
		uint32_t vertex_index2 = _indices[triangle_index * 3 + 2];

		const Vector3F& p0 = _vertices[vertex_index0];	
		const Vector3F& p1 = _vertices[vertex_index1];	
		const Vector3F& p2 = _vertices[vertex_index2];

		bool need_removed = p0 == p1 || p1 == p2 || p2 == p0;
		if (!need_removed)
		{
			for (uint32_t ix = 0; ix < 3; ++ix)
			{
				// Remove vertices at the same position.
				uint32_t& vertex_index = _indices[triangle_index * 3 + ix];
				const auto& vertex_position = _vertices[vertex_index];
				uint32_t key = hash(vertex_position);
				for (uint32_t jx : _vertex_table[key])
				{
					if (jx == vertex_index)
					{
						break;
					}
					else if (vertex_position == _vertices[jx]) 
					{
						assert(_vertex_ref_count[vertex_index] > 0);
						if (--_vertex_ref_count[vertex_index] == 0)
						{
							_vertex_table.remove(key, vertex_index);
							_remain_vertex_num--;
						}
						vertex_index = jx;
						if (vertex_index != INVALID_SIZE_32) _vertex_ref_count[vertex_index]++;
						break;
					}
				}
			}
			
			// Check if the triangle is duplicated.
			for (uint32_t ix : _index_table[hash(p0)])
			{
				if (
					ix != triangle_index * 3 &&
					vertex_index0 == _indices[ix] && 
					vertex_index1 == _indices[triangle_index_cycle3(ix)] && 
					vertex_index2 == _indices[triangle_index_cycle3(ix, 2)] 
				)
				{
					need_removed = true;
				}
			}
		}

		if (need_removed)
		{
			// Remove the whole triangle.
			_triangle_removed_array.set_true(triangle_index);
			_remain_triangle_num--;

			for (uint32_t ix = 0; ix < 3; ++ix)
			{
				uint32_t index_index = triangle_index * 3 + ix;
				uint32_t& vertex_index = _indices[index_index];
				const auto& vertex_position = _vertices[vertex_index];

				uint32_t key = hash(vertex_position);
				_index_table.remove(key, index_index);

				assert(_vertex_ref_count[vertex_index] > 0);
				if (--_vertex_ref_count[vertex_index] == 0)
				{
					_vertex_table.remove(key, vertex_index);
					_remain_vertex_num--;
				}
				vertex_index = INVALID_SIZE_32;
			}
		}
		else 
		{
			_triangle_surfaces[triangle_index] = QuadricSurface(p0, p1, p2);
		}
	}

	bool MeshOptimizer::compact()
	{
		// key: vertex_index_before_compact; value: vertex_index_after_compact.
		std::vector<uint32_t> indices_map(_vertices.size());	
		
		uint32_t count = 0;
		for (uint32_t ix = 0; ix < _vertices.size(); ++ix)
		{
			if (_vertex_ref_count[ix] > 0)
			{
				if (ix != count) _vertices[count] = _vertices[ix];
				indices_map[ix] = count++;
			}
		}
		ReturnIfFalse(count == _remain_vertex_num);

		count = 0;
		for (uint32_t ix = 0; ix < _triangle_surfaces.size(); ++ix)
		{
			if (!_triangle_removed_array[ix])
			{
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					_indices[count * 3 + jx] = indices_map[_indices[ix * 3 + jx]];
				}
				count++;
			}
		}
		return count == _remain_triangle_num;
	}

	float MeshOptimizer::evaluate(const Vector3F& p0, const Vector3F& p1, bool if_merge)
	{
		if (p0 == p1) return 0.0f;

		float error;

		std::vector<uint32_t> adjacency_triangle_indices;
		auto BuildAdjacencyTrianlges = [this, &adjacency_triangle_indices](const Vector3F& vertex, bool& lock)
		{
			for (uint32_t ix : _index_table[hash(vertex)])
			{
				if (_vertices[_indices[ix]] == vertex)
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
		BuildAdjacencyTrianlges(p0, lock0);
		BuildAdjacencyTrianlges(p1, lock1);

		if (adjacency_triangle_indices.empty()) return 0.0f;
		if (adjacency_triangle_indices.size() > 24) error += 0.5f * (adjacency_triangle_indices.size() - 24);

		QuadricSurface surface;
		for (uint32_t ix : adjacency_triangle_indices) surface = merge(surface, _triangle_surfaces[ix]);

		Vector3F p = (p0 + p1) * 0.5f;
		if (lock0 && lock1) error += 1e8;
		else if (lock0 && !lock1) p = p0;
		else if (!lock0 && lock1) p = p1;
		else if (
			!surface.get_vertex_position(p) || 
			distance(p, p0) + distance(p, p1) > 2.0f * distance(p0, p1)	// invalid
		)
		{
			p = (p0 + p1) * 0.5f;
		}

		error += surface.distance_to_surface(p);

		if (if_merge)
		{
			merge_begin(p0);
			merge_begin(p1);

			for (uint32_t ix : adjacency_triangle_indices)
			{
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					uint32_t vertex_index = ix * 3 + jx;
					Vector3F& vertex_position = _vertices[_indices[vertex_index]];
					if (vertex_position == p0 || vertex_position == p1)
					{
						vertex_position = p;
						if (lock0 || lock1)
						{
							_flags[vertex_index] |= LockFlag;
						}
					}
				}
			}
			for (uint32_t ix : moved_edge_indices)
			{
				auto& edge = _edges[ix];
				if (edge.first == p0 || edge.first == p1) edge.first = p;
				if (edge.second == p0 || edge.second == p1) edge.second = p;
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
			adjacency_vertex_indices.erase(
				std::unique(adjacency_vertex_indices.begin(), adjacency_vertex_indices.end()), 
				adjacency_vertex_indices.end()
			);

			for (uint32_t vertex_index : adjacency_vertex_indices)
			{
				const auto& crVertex = _vertices[vertex_index];
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
			
			for (uint32_t dwTriangleIndex : adjacency_triangle_indices) fix_triangle(dwTriangleIndex);
		}	

		for (uint32_t triangle_index : adjacency_triangle_indices) _flags[triangle_index * 3] &= ~AdjacencyFlag;

		return error;
	}

	void MeshOptimizer::merge_begin(const Vector3F& p)
	{
		uint32_t key = hash(p);
		for (uint32_t ix : _vertex_table[key])
		{
			if (_vertices[ix] == p)
			{
				_vertex_table.remove(key, ix);
				moved_vertex_indices.push_back(ix);
			}
		}
		for (uint32_t ix : _index_table[key])
		{
			if (_vertices[_indices[ix]] == p)
			{
				_index_table.remove(key, ix);
				moved_index_indices.push_back(ix);
			}
		}
		for (uint32_t ix : _edges_begin_table[key])
		{
			if (_edges[ix].first == p)
			{
				_edges_begin_table.remove(key, ix);
				moved_edge_indices.push_back(ix);
			}
		}
		for (uint32_t ix : _edges_end_table[key])
		{
			if (_edges[ix].second == p)
			{
				_edges_end_table.remove(key, ix);
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
			auto& edge = _edges[ix];
			
			uint32_t h0 = hash(edge.first);
			uint32_t h1 = hash(edge.second);
			if (h0 > h1)
			{
				std::swap(h0, h1);
				std::swap(edge.first, edge.second);
			}

			bool alread_exist = false;
			for (uint32_t ix : _edges_begin_table[h0])
			{
				const auto& edge_copy = _edges[ix];
				if (edge_copy.first == edge_copy.first && edge_copy.second == edge_copy.second) 
				{
					alread_exist = true;
					break;
				}
			}
			if (!alread_exist)
			{
				_edges_begin_table.insert(h0, static_cast<uint32_t>(_edges.size()));
				_edges_end_table.insert(h1, static_cast<uint32_t>(_edges.size()));
			}

			if (edge.first == edge.second && alread_exist)
			{
				_heap.remove(ix);
			}
		}
		
		moved_edge_indices.clear();
		moved_vertex_indices.clear();
		moved_index_indices.clear();
	}

	bool VirtualGeometry::build(const Mesh* mesh)
	{
		for (const auto& submesh : mesh->submeshes)
		{
			_indices.insert(_indices.end(), submesh.indices.begin(), submesh.indices.end());
			
			uint32_t start_index = _vertex_positons.size();
			_vertex_positons.resize(_vertex_positons.size() + submesh.vertices.size());

			for (uint32_t ix = 0; ix < submesh.vertices.size(); ++ix)
			{
				_vertex_positons[start_index + ix] = submesh.vertices[ix].position;
			}
		}

		MeshOptimizer optimizer(_vertex_positons, _indices);
		ReturnIfFalse(optimizer.optimize(_indices.size()));

		ReturnIfFalse(cluster_triangles());
		uint32_t level_offset = 0;
		uint32_t mip_level = 0;
		while (true)
		{
			uint32_t level_cluster_count = _clusters.size() - level_offset;
			if(level_cluster_count<=1) break;

			uint32_t old_cluster_num = _clusters.size();
			uint32_t old_group_num = _cluster_groups.size();

			ReturnIfFalse(build_cluster_groups(level_offset, level_cluster_count, mip_level));

			for(uint32_t ix = old_group_num; ix < _cluster_groups.size(); ix++)
			{
				ReturnIfFalse(build_parent_clusters(ix));
			}

			level_offset = old_cluster_num;
			mip_level++;
		}
		_mip_level_num = mip_level + 1;
		
		return true;
	}

	uint32_t VirtualGeometry::edge_hash(const Vector3F& p0, const Vector3F& p1)
	{
		uint32_t key0 = hash(p0);
		uint32_t key1 = hash(p1);
		return murmur_mix(murmur_add(key0, key1));
	}

 
	void VirtualGeometry::build_adjacency_graph(SimpleGraph& edge_link_graph, SimpleGraph& adjacency_graph)
	{
		HashTable edge_table(static_cast<uint32_t>(_indices.size()));
		edge_link_graph.resize(_indices.size());

		for (uint32_t edge_start_index = 0; edge_start_index < _indices.size(); ++edge_start_index)
		{
			Vector3F p0 = _vertex_positons[_indices[edge_start_index]];
			Vector3F p1 = _vertex_positons[_indices[triangle_index_cycle3(edge_start_index)]];

			// Find shared vertices and opposite edges, which represent adjacent triangles.
			edge_table.insert(edge_hash(p0, p1), edge_start_index);
			for (uint32_t edge_end_index : edge_table[edge_hash(p1, p0)])
			{
				if (
					p1 == _vertex_positons[_indices[edge_end_index]] && 
					p0 == _vertex_positons[_indices[triangle_index_cycle3(edge_end_index)]]
				)
				{
					// Increase weight.
					edge_link_graph[edge_start_index][edge_end_index] += 1;
					edge_link_graph[edge_end_index][edge_start_index] += 1;
				}
			}
		}

		adjacency_graph.resize(edge_link_graph.size() / 3);
		uint32_t edge_start_node = 0;
		for (const auto& edge_end_node_weights : edge_link_graph)
		{
			for (const auto& [edge_end_node, weight] : edge_end_node_weights)
			{
				adjacency_graph[edge_start_node / 3][edge_end_node / 3] += weight;
			}
			edge_start_node++;
		}
	}

	bool VirtualGeometry::cluster_triangles()
	{
		SimpleGraph edge_link_graph;
		SimpleGraph triangle_adjacency_graph;
		build_adjacency_graph(edge_link_graph, triangle_adjacency_graph);

		GraphPartitionar partitionar;
		partitionar.partition_graph(triangle_adjacency_graph, MeshClusterGroup::group_size - 4, MeshClusterGroup::group_size);

		for (const auto& [left, right] : partitionar._part_ranges)
		{
			auto& cluster = _clusters.emplace_back();

			// Map the vertices in _vertices to the _clusters.
			std::unordered_map<uint32_t, uint32_t> cluster_vertex_index_map;
			for (uint32_t ix = left; ix < right; ++ix)
			{
				uint32_t triangle_index = partitionar._node_indices[ix];
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					uint32_t edge_start_index = triangle_index * 3 + jx;
					uint32_t vertex_index = _indices[edge_start_index];
					if (cluster_vertex_index_map.find(vertex_index) == cluster_vertex_index_map.end())
					{
						cluster_vertex_index_map[vertex_index] = static_cast<uint32_t>(cluster.vertices.size());
						cluster.vertices.push_back(_vertex_positons[vertex_index]);
					}

					bool is_external = false;
					for (const auto& [edge_end_index, weight] : edge_link_graph[edge_start_index])
					{
						uint32_t remapped_edge_end_index = partitionar._node_map[edge_end_index / 3];
						
						// Edge_end_index(remapped) in different partitions indicate a boundary
						if (remapped_edge_end_index < left || remapped_edge_end_index >= right)
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
			cluster.bounding_box = Bounds3F(cluster.vertices);
			cluster.bounding_sphere = Sphere(cluster.vertices);
			cluster.lod_bounding_sphere = cluster.bounding_sphere;
		}

		return true;
	}

	bool VirtualGeometry::build_cluster_groups(uint32_t level_offset, uint32_t level_cluster_count, uint32_t mip_level)
	{
		const std::span<MeshCluster> clusters_view(_clusters.begin() + level_offset, level_cluster_count);

		// Extract the boundary of each cluster and establish a mapping from edge index to cluster index.
		std::vector<uint32_t> edge_cluster_map;
		std::vector<uint32_t> cluster_edge_offset_map;
		std::vector<std::pair<uint32_t, uint32_t>> cluster_edge_map;

		
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
			cluster_index++;
		}

		SimpleGraph edge_link_graph(cluster_edge_map.size());

		HashTable edge_table(static_cast<uint32_t>(cluster_edge_map.size()));
		for (uint32_t ix = 0; ix < cluster_edge_map.size(); ++ix)
		{
			const auto& [cluster_index, edge_start_index] = cluster_edge_map[ix];
			const auto& vertices = clusters_view[cluster_index].vertices;
			const auto& indices = clusters_view[cluster_index].indices;

			const auto& p0 = vertices[indices[edge_start_index]];
			const auto& p1 = vertices[indices[triangle_index_cycle3(edge_start_index)]];
			edge_table.insert(edge_hash(p0, p1), ix);
			for (uint32_t jx : edge_table[edge_hash(p1, p0)])
			{
				const auto& [another_cluster_index, another_edge_start_index] = cluster_edge_map[jx];
				const auto& another_vertices = clusters_view[another_cluster_index].vertices;
				const auto& another_indices = clusters_view[another_cluster_index].indices;

				if (
					p1 == another_vertices[another_indices[another_edge_start_index]] && 
					p0 == another_vertices[another_indices[triangle_index_cycle3(another_edge_start_index)]]
				)
				{
					edge_link_graph[ix][jx]++;
					edge_link_graph[jx][ix]++;
				}
			}
		}

		SimpleGraph cluster_graph(level_cluster_count);

		
		for (uint32_t edge_start_index = 0; edge_start_index < edge_link_graph.size(); ++edge_start_index)
		{
			const auto& edge_end_node_weights = edge_link_graph[edge_start_index];
			for (const auto& [edge_end_node, weight] : edge_end_node_weights)
			{
				cluster_graph[edge_cluster_map[edge_start_index]][edge_cluster_map[edge_end_node]] += weight;
			}
		}


		GraphPartitionar partitionar;
		partitionar.partition_graph(cluster_graph, MeshClusterGroup::group_size - 4, MeshClusterGroup::group_size);

		for (auto [left, right] : partitionar._part_ranges)
		{
			auto& cluster_group = _cluster_groups.emplace_back();
			cluster_group.mip_level = mip_level;

			for (uint32_t ix = left; ix < right; ++ix)
			{
				uint32_t cluster_index = partitionar._node_indices[ix];
				_clusters[cluster_index + level_offset].group_id = static_cast<uint32_t>(_cluster_groups.size() - 1);
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
						uint32_t remapped_cluster_index = partitionar._node_map[edge_cluster_map[edge_end_index]];
						
						// Edge_end_index(remapped) in different partitions indicate a boundary
						if (remapped_cluster_index < left || remapped_cluster_index >= right)
						{
							is_external = true;
							break;
						}
					}
					if (is_external)
					{
						uint32_t edge_end_index = cluster_edge_map[edge_start_index].second;
						cluster_group.external_edges.push_back(std::make_pair(cluster_index, edge_end_index));
					}
				}
			}
		}

		return true;
	}

	bool VirtualGeometry::build_parent_clusters(uint32_t cluster_group_index)
	{
		auto& cluster_group = _cluster_groups[cluster_group_index];

		uint32_t index_offset = 0;
		float parent_lod_error = 0.0f;
		Sphere parent_lod_bounding_sphere;
		std::vector<uint32_t> indices;
		std::vector<Vector3F> vertex_positions;
		for (auto cluster_index : cluster_group.cluster_indices)
		{
			const auto& cluster = _clusters[cluster_index];
			for (const auto& p : cluster.vertices) vertex_positions.push_back(p);
			for (const auto& i : cluster.indices) indices.push_back(i + index_offset);
			index_offset += static_cast<uint32_t>(cluster.vertices.size());

			parent_lod_bounding_sphere = merge(parent_lod_bounding_sphere, cluster.lod_bounding_sphere);
			parent_lod_error = std::max(parent_lod_error, cluster.lod_error);
		}
		
		cluster_group.lod_bounding_sphere = parent_lod_bounding_sphere;
		cluster_group.parent_lod_error = parent_lod_error;

		MeshOptimizer optimizer(vertex_positions, indices);
		HashTable edge_table(static_cast<uint32_t>(cluster_group.external_edges.size()));
		for (uint32_t ix = 0; ix < cluster_group.external_edges.size(); ++ix)
		{
			const auto& [cluster_index, edge_index] = cluster_group.external_edges[ix];
			auto& positions = _clusters[cluster_index].vertices;
        	auto& vertex_indices = _clusters[cluster_index].indices;
			
			const Vector3F& p0 = positions[vertex_indices[edge_index]];
			const Vector3F& p1 = positions[vertex_indices[triangle_index_cycle3(edge_index)]];
			edge_table.insert(edge_hash(p0, p1), ix);
			optimizer.lock_position(p0);
			optimizer.lock_position(p1);
		}

		optimizer.optimize((MeshCluster::cluster_size - 2) * (static_cast<uint32_t>(cluster_group.cluster_indices.size()) / 2));
		parent_lod_error = std::max(parent_lod_error, std::sqrt(optimizer._max_error));

		SimpleGraph edge_link_graph;
		SimpleGraph triangle_adjacency_graph;
		build_adjacency_graph(edge_link_graph, triangle_adjacency_graph);

		GraphPartitionar partitionar;
		partitionar.partition_graph(triangle_adjacency_graph, MeshClusterGroup::group_size - 4, MeshClusterGroup::group_size);

		for (const auto& [left, right] : partitionar._part_ranges)
		{
			auto& cluster = _clusters.emplace_back();

			// Map the vertices in _vertices to the _clusters.
			std::unordered_map<uint32_t, uint32_t> cluster_vertex_index_map;
			for (uint32_t ix = left; ix < right; ++ix)
			{
				uint32_t triangle_index = partitionar._node_indices[ix];
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					uint32_t edge_start_index = triangle_index * 3 + jx;
					uint32_t vertex_index = indices[edge_start_index];
					if (cluster_vertex_index_map.find(vertex_index) == cluster_vertex_index_map.end())
					{
						cluster_vertex_index_map[vertex_index] = static_cast<uint32_t>(cluster.vertices.size());
						cluster.vertices.push_back(vertex_positions[vertex_index]);
					}

					bool is_external = false;
					for (const auto& [edge_end_index, weight] : edge_link_graph[edge_start_index])
					{
						uint32_t remapped_triangle_index = partitionar._node_map[edge_end_index / 3];
						
						// Edge_end_index(remapped) in different partitions indicate a boundary
						if (remapped_triangle_index < left || remapped_triangle_index >= right)
						{
							is_external = true;
							break;
						}
					}
					const Vector3F& p0 = vertex_positions[vertex_index];
					const Vector3F& p1 = vertex_positions[indices[triangle_index_cycle3(edge_start_index)]];
					if (!is_external)
					{
						for (uint32_t jx : edge_table[edge_hash(p0, p1)])
						{
							const auto& [cluster_index, edge_index] = cluster_group.external_edges[jx];
							auto& positions = _clusters[cluster_index].vertices;
        					auto& vertex_indices = _clusters[cluster_index].indices;

							if (
								p0 == positions[vertex_indices[edge_index]] && 
								p1 == positions[vertex_indices[triangle_index_cycle3(edge_index)]]
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
			cluster.mip_level = cluster_group.mip_level + 1;
			cluster.bounding_box = Bounds3F(cluster.vertices);
			cluster.bounding_sphere = Sphere(cluster.vertices);

			// The LOD bounding box of the parent node covers all the LOD bounding boxes of its child nodes.
			cluster.lod_bounding_sphere = parent_lod_bounding_sphere;
			cluster.lod_error = parent_lod_error;
		}
		return true;
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

			v[0] = Vertex({ -w2, -h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });
			v[1] = Vertex({ -w2, +h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
			v[2] = Vertex({ +w2, +h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f });
			v[3] = Vertex({ +w2, -h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f });
			v[4] = Vertex({ -w2, -h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f });
			v[5] = Vertex({ +w2, -h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });
			v[6] = Vertex({ +w2, +h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
			v[7] = Vertex({ -w2, +h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f });
			v[8] = Vertex({ -w2, +h2, -d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });
			v[9] = Vertex({ -w2, +h2, +d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
			v[10] = Vertex({ +w2, +h2, +d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f });
			v[11] = Vertex({ +w2, +h2, -d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f });
			v[12] = Vertex({ -w2, -h2, -d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f });
			v[13] = Vertex({ +w2, -h2, -d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });
			v[14] = Vertex({ +w2, -h2, +d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
			v[15] = Vertex({ -w2, -h2, +d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f });
			v[16] = Vertex({ -w2, -h2, +d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f });
			v[17] = Vertex({ -w2, +h2, +d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f });
			v[18] = Vertex({ -w2, +h2, -d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f });
			v[19] = Vertex({ -w2, -h2, -d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f });
			v[20] = Vertex({ +w2, -h2, -d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
			v[21] = Vertex({ +w2, +h2, -d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
			v[22] = Vertex({ +w2, +h2, +d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
			v[23] = Vertex({ +w2, -h2, +d2 }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });

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
			Vector3F tangent = normalize(0.5f * (v0.tangent + v1.tangent));
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
			Vertex topVertex({ 0.0f, +radius, 0.0f }, { 0.0f, +1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
			Vertex bottomVertex({ 0.0f, -radius, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });

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
					vertex.tangent = Vector3F(-s, 0.0f, c);

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

				mesh_data.vertices.push_back(Vertex({ x, y, z }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { u, v }));
			}

			// Cap center vertex.
			mesh_data.vertices.push_back(Vertex({ 0.0f, y, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.5f, 0.5f }));

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

				mesh_data.vertices.push_back(Vertex({ x, y, z }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { u, v }));
			}

			// Cap center vertex.
			mesh_data.vertices.push_back(Vertex({ 0.0f, y, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.5f, 0.5f }));

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
					submesh.vertices[i * n + j].tangent = Vector3F(1.0f, 0.0f, 0.0f);

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
				{ 1.0f, 0.0f, 0.0f },
				{ 0.0f, 1.0f }
			);

			submesh.vertices[1] = Vertex(
				{ x, y, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f }
			);

			submesh.vertices[2] = Vertex(
				{ x + w, y, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f },
				{ 1.0f, 0.0f }
			);

			submesh.vertices[3] = Vertex(
				{ x + w, y - h, depth },
				{ 0.0f, 0.0f, -1.0f },
				{ 1.0f, 0.0f, 0.0f },
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
