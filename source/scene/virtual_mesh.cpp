#include "virtual_mesh.h"
#include "geometry.h"
#include "scene.h"

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
		assert(is_valid(index));

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
		_triangle_removed_array(indices.size() / 3.0f, false)
	{
		_remain_vertex_num = static_cast<uint32_t>(_vertices.size());
		_remain_triangle_num = static_cast<uint32_t>(_triangle_surfaces.size());

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
			Vertex p1 = _vertices[_indices[triangle_index_cycle3(ix)]];

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
			_edges_begin_table.remove(hash(edge.first.position), edge_index);
			_edges_end_table.remove(hash(edge.second.position), edge_index);

			_max_error = std::max(evaluate(edge.first, edge.second, true), _max_error);

			if (_remain_triangle_num <= target_triangle_num) break;

			for (uint32_t ix : edge_need_reevaluate_indices)
			{
				const auto& reevaluate_edge = _edges[ix];
				float error = evaluate(reevaluate_edge.first, reevaluate_edge.second, false);
				_heap.insert(error, ix);
			}

			edge_need_reevaluate_indices.clear();
		}

		bool ret = compact();

		_vertices.resize(_remain_vertex_num);
		_indices.resize(_remain_triangle_num * 3);

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
		if (_triangle_removed_array[triangle_index]) return;

		uint32_t vertex_index0 = _indices[triangle_index * 3 + 0];
		uint32_t vertex_index1 = _indices[triangle_index * 3 + 1];
		uint32_t vertex_index2 = _indices[triangle_index * 3 + 2];

		const float3& p0 = _vertices[vertex_index0].position;	
		const float3& p1 = _vertices[vertex_index1].position;	
		const float3& p2 = _vertices[vertex_index2].position;

		bool need_removed = p0 == p1 || p1 == p2 || p2 == p0;
		if (!need_removed)
		{
			for (uint32_t ix = 0; ix < 3; ++ix)
			{
				// Remove vertices at the same position.
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
				const auto& vertex_position = _vertices[vertex_index].position;

				uint32_t key = hash(vertex_position);
				_index_table.remove(key, index_index);

				if (vertex_index == INVALID_SIZE_32) continue;
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
			_triangle_surfaces[triangle_index] = QuadricSurface(
				Vector3<double>(p0), 
				Vector3<double>(p1), 
				Vector3<double>(p2)
			);
		}
	}

	bool MeshOptimizer::compact()
	{
		uint32_t count = 0;
		for (uint32_t ix = 0; ix < _vertices.size(); ++ix)
		{
			if (_vertex_ref_count[ix] > 0)
			{
				if (ix != count) _vertices[count] = _vertices[ix];
				_vertex_ref_count[ix] = count++;
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
					_indices[count * 3 + jx] = _vertex_ref_count[_indices[ix * 3 + jx]];
				}
				count++;
			}
		}
		return count == _remain_triangle_num;
	}

	float MeshOptimizer::evaluate(const Vertex& p0, const Vertex& p1, bool if_merge)
	{
		if (p0 == p1) return 0.0f;

		float error = 0.0f;

		std::vector<uint32_t> adjacency_triangle_indices;
		auto BuildAdjacencyTrianlges = [this, &adjacency_triangle_indices](const float3& vertex_position, bool& lock)
		{
			for (uint32_t ix : _index_table[hash(vertex_position)])
			{
				if (_vertices[_indices[ix]].position == vertex_position)
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
		BuildAdjacencyTrianlges(p0.position, lock0);
		BuildAdjacencyTrianlges(p1.position, lock1);

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
		else if (!surface.get_vertex(
			p.position, p.normal, p.tangent
		))
		{
			p = average_p;
		}
		
		if (
			distance(p.position, p0.position) + distance(p.position, p1.position) > 
			2.0f * distance(p0.position, p1.position)
		) 
		{
			p = average_p;
		} 


		error += surface.distance_to_surface(p.position);

		if (if_merge)
		{
			merge_begin(p0);
			merge_begin(p1);

			for (uint32_t ix : adjacency_triangle_indices)
			{
				for (uint32_t jx = 0; jx < 3; ++jx)
				{
					uint32_t vertex_index = ix * 3 + jx;
					Vertex& vertex = _vertices[_indices[vertex_index]];
					if (vertex == p0 || vertex == p1)
					{
						vertex = p;
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

			// Remove duplicate elements.
			adjacency_vertex_indices.erase(
				std::unique(adjacency_vertex_indices.begin(), adjacency_vertex_indices.end()), 
				adjacency_vertex_indices.end()
			);

			for (uint32_t vertex_index : adjacency_vertex_indices)
			{
				const auto& vertex = _vertices[vertex_index];
				uint32_t key = hash(vertex.position);
				for (uint32_t ix : _edges_begin_table[key])
				{
					if (_edges[ix].first == vertex && _heap.is_valid(ix))
					{
						_heap.remove(ix);
						edge_need_reevaluate_indices.push_back(ix);
					}
				}
				for (uint32_t ix : _edges_end_table[key])
				{
					if (_edges[ix].second == vertex && _heap.is_valid(ix))
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

	void MeshOptimizer::merge_begin(const Vertex& p)
	{
		uint32_t key = hash(p.position);
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
				_edges_end_table.remove(hash(_edges[ix].second.position), ix);
				moved_edge_indices.push_back(ix);
			}
		}
		for (uint32_t ix : _edges_end_table[key])
		{
			if (_edges[ix].second == p)
			{
				_edges_begin_table.remove(hash(_edges[ix].first.position), ix);
				_edges_end_table.remove(key, ix);
				moved_edge_indices.push_back(ix);
			}
		}
	}

	void MeshOptimizer::mergen_end()
	{
		for (uint32_t ix : moved_vertex_indices)
		{
			_vertex_table.insert(hash(_vertices[ix].position), ix);
		}
		for (uint32_t ix : moved_index_indices)
		{
			_index_table.insert(hash(_vertices[_indices[ix]].position), ix);
		}
		for (uint32_t ix : moved_edge_indices)
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
		
		moved_edge_indices.clear();
		moved_vertex_indices.clear();
		moved_index_indices.clear();
	}

	bool VirtualMesh::build(const Mesh* mesh)
	{
		_current_geometry_id = mesh->mesh_id << 16;

		for (const auto& submesh : mesh->submeshes)
		{
			_indices.insert(_indices.end(), submesh.indices.begin(), submesh.indices.end());
			
			uint32_t start_index = _vertices.size();
			_vertices.resize(_vertices.size() + submesh.vertices.size());

			for (uint32_t ix = 0; ix < submesh.vertices.size(); ++ix)
			{
				_vertices[start_index + ix] = submesh.vertices[ix];
			}

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
			virtual_submesh.mip_level_num = mip_level + 1;

			_current_geometry_id++;
			_indices.clear();
			_vertices.clear();
		}

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
		HashTable edge_table(static_cast<uint32_t>(indices.size()));
		edge_link_graph.resize(indices.size());

		for (uint32_t edge_start_index = 0; edge_start_index < indices.size(); ++edge_start_index)
		{
			const Vertex& p0 = vertices[indices[edge_start_index]];
			const Vertex& p1 = vertices[indices[triangle_index_cycle3(edge_start_index)]];

			// Find shared vertices and opposite edges, which represent adjacent triangles.
			edge_table.insert(edge_hash(p0.position, p1.position), edge_start_index);
			for (uint32_t edge_end_index : edge_table[edge_hash(p1.position, p0.position)])
			{
				if (
					p1 == vertices[indices[edge_end_index]] && 
					p0 == vertices[indices[triangle_index_cycle3(edge_end_index)]]
				)
				{
					// Increase weight.
					edge_link_graph[edge_start_index][edge_end_index]++;
					edge_link_graph[edge_end_index][edge_start_index]++;
				}
			}
		}

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
		SimpleGraph edge_link_graph;
		SimpleGraph triangle_adjacency_graph;
		build_adjacency_graph(_vertices, _indices, edge_link_graph, triangle_adjacency_graph);

		GraphPartitionar partitionar;
		partitionar.partition_graph(triangle_adjacency_graph, MeshCluster::cluster_size - 4, MeshCluster::cluster_size);

		for (const auto& [left, right] : partitionar._part_ranges)
		{
			auto& cluster = submesh.clusters.emplace_back();
			cluster.geometry_id = _current_geometry_id;

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
						cluster.vertices.push_back(_vertices[vertex_index]);
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
			std::vector<float3> vertex_positions(cluster.vertices.size());
			for (uint32_t ix = 0; ix < vertex_positions.size(); ++ix) vertex_positions[ix] = cluster.vertices[ix].position;

			cluster.bounding_box = Bounds3F(vertex_positions);
			cluster.bounding_sphere = Sphere(vertex_positions);
			cluster.lod_bounding_sphere = cluster.bounding_sphere;
		}

		return true;
	}

	bool VirtualMesh::build_cluster_groups(VirtualSubmesh& submesh, uint32_t level_offset, uint32_t level_cluster_count, uint32_t mip_level)
	{
		const std::span<MeshCluster> clusters_view(submesh.clusters.begin() + level_offset, level_cluster_count);

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
			edge_table.insert(edge_hash(p0.position, p1.position), ix);
			for (uint32_t jx : edge_table[edge_hash(p1.position, p0.position)])
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
			auto& cluster_group = submesh.cluster_groups.emplace_back();
			cluster_group.mip_level = mip_level;

			for (uint32_t ix = left; ix < right; ++ix)
			{
				uint32_t cluster_index = partitionar._node_indices[ix];
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
						cluster_group.external_edges.push_back(std::make_pair(cluster_index + level_offset, edge_end_index));
					}
				}
			}
		}

		return true;
	}

	bool VirtualMesh::build_parent_clusters(VirtualSubmesh& submesh, uint32_t cluster_group_index)
	{
		auto& cluster_group = submesh.cluster_groups[cluster_group_index];

		uint32_t index_offset = 0;
		float parent_lod_error = 0.0f;
		std::vector<Sphere> parent_lod_bounding_spheres;
		std::vector<uint32_t> indices;
		std::vector<Vertex> vertex_positions;
		for (auto cluster_index : cluster_group.cluster_indices)
		{
			const auto& cluster = submesh.clusters[cluster_index];
			for (const auto& p : cluster.vertices) vertex_positions.push_back(p);
			for (const auto& i : cluster.indices) indices.push_back(i + index_offset);
			index_offset += static_cast<uint32_t>(cluster.vertices.size());

			parent_lod_bounding_spheres.push_back(cluster.lod_bounding_sphere);
			parent_lod_error = std::max(parent_lod_error, cluster.lod_error);
		}
		
		cluster_group.lod_bounding_sphere = merge(parent_lod_bounding_spheres);
		cluster_group.parent_lod_error = parent_lod_error;

		MeshOptimizer optimizer(vertex_positions, indices);
		HashTable edge_table(static_cast<uint32_t>(cluster_group.external_edges.size()));
		for (uint32_t ix = 0; ix < cluster_group.external_edges.size(); ++ix)
		{
			const auto& [cluster_index, edge_index] = cluster_group.external_edges[ix];
			auto& positions = submesh.clusters[cluster_index].vertices;
        	auto& vertex_indices = submesh.clusters[cluster_index].indices;
			
			const Vertex& p0 = positions[vertex_indices[edge_index]];
			const Vertex& p1 = positions[vertex_indices[triangle_index_cycle3(edge_index)]];
			edge_table.insert(edge_hash(p0.position, p1.position), ix);
			optimizer.lock_position(p0.position);
			optimizer.lock_position(p1.position);
		}

		ReturnIfFalse(optimizer.optimize(
			(MeshCluster::cluster_size - 2) * (static_cast<uint32_t>(cluster_group.cluster_indices.size()) / 2)
		));
		parent_lod_error = std::max(parent_lod_error, std::sqrt(optimizer._max_error));

		SimpleGraph edge_link_graph;
		SimpleGraph triangle_adjacency_graph;
		build_adjacency_graph(
			vertex_positions,
			indices,
			edge_link_graph, 
			triangle_adjacency_graph
		);

		GraphPartitionar partitionar;
		partitionar.partition_graph(triangle_adjacency_graph, MeshCluster::cluster_size - 4, MeshCluster::cluster_size);

		for (const auto& [left, right] : partitionar._part_ranges)
		{
			auto& cluster = submesh.clusters.emplace_back();

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
					const Vertex& p0 = vertex_positions[vertex_index];
					const Vertex& p1 = vertex_positions[indices[triangle_index_cycle3(edge_start_index)]];
					if (!is_external)
					{
						for (uint32_t jx : edge_table[edge_hash(p0.position, p1.position)])
						{
							const auto& [cluster_index, edge_index] = cluster_group.external_edges[jx];
							auto& positions = submesh.clusters[cluster_index].vertices;
        					auto& vertex_indices = submesh.clusters[cluster_index].indices;

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

			
			std::vector<float3> vertex_positions(cluster.vertices.size());
			for (uint32_t ix = 0; ix < vertex_positions.size(); ++ix) vertex_positions[ix] = cluster.vertices[ix].position;

			cluster.bounding_box = Bounds3F(vertex_positions);
			cluster.bounding_sphere = Sphere(vertex_positions);

			// The LOD bounding box of the parent node covers all the LOD bounding boxes of its child nodes.
			cluster.lod_bounding_sphere = cluster_group.lod_bounding_sphere;
			cluster.lod_error = parent_lod_error;
		}
		return true;
	}

    bool SceneSystem::publish(World* world, const event::OnComponentAssigned<VirtualMesh>& event)
	{
		VirtualMesh* virtual_geometry = event.component;
		Mesh* mesh = event.entity->get_component<Mesh>();

		return virtual_geometry->build(mesh);
	}
}