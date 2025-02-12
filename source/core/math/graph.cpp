#include "graph.h"
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>
#include <metis.h>
#include "../tools/log.h"


namespace fantasy 
{
    
	bool GraphPartitionar::partition_graph(const SimpleGraph& graph, uint32_t min_part_size, uint32_t max_part_size)
	{
        // 根据图的边权重, 将图划分为一个个分区, 每个分区所容纳的节点数量规定在 [min_part_size, max_part_size]内.
        
		uint32_t node_count = static_cast<uint32_t>(graph.size());
		node_indices.resize(node_count);
		node_map.resize(node_count);
		std::iota(node_indices.begin(), node_indices.end(), 0);
		std::iota(node_map.begin(), node_map.end(), 0);

		part_ranges.clear();
		_min_part_size = min_part_size;
		_max_part_size = max_part_size;

		MetisGraph* metis_graph = new MetisGraph();
		metis_graph->node_count = graph.size();
		for(auto& edge_end_weights : graph)
		{
			metis_graph->node_adjacency_start_index.push_back(static_cast<int32_t>(metis_graph->adjandency_nodes.size()));
			for(auto[edge_end, weight] : edge_end_weights)
			{
				metis_graph->adjandency_nodes.push_back(edge_end);
				metis_graph->adjandency_node_weights.push_back(weight);
			}
		}
		metis_graph->node_adjacency_start_index.push_back(static_cast<int32_t>(metis_graph->adjandency_nodes.size()));

        ReturnIfFalse(bisect_graph_resursive(metis_graph, 0, metis_graph->node_count));

        std::sort(part_ranges.begin(), part_ranges.end());
        for (uint32_t ix = 0; ix < node_indices.size(); ++ix)
        {
            node_map[node_indices[ix]] = ix;
        }

		return true;
	}

	bool GraphPartitionar::bisect_graph_resursive(MetisGraph* metis_graph, uint32_t range_start, uint32_t range_end)
	{
        // 递归分割图.
     
        MetisGraph* child_metis_graphs[2] = { nullptr };
        uint32_t split_pos = bisect_graph(metis_graph, child_metis_graphs, range_start, range_end);
        delete metis_graph;

        if (child_metis_graphs[0] && child_metis_graphs[1])
        {
            ReturnIfFalse(bisect_graph_resursive(child_metis_graphs[0], range_start, split_pos));
            ReturnIfFalse(bisect_graph_resursive(child_metis_graphs[1], split_pos, range_end));
        }
        else 
        {
            ReturnIfFalse(child_metis_graphs[0] == nullptr && child_metis_graphs[1] == nullptr);
        }
		return true;
	}

	uint32_t GraphPartitionar::bisect_graph(
		MetisGraph* metis_graph, 
		MetisGraph* child_metis_graph[2], 
		uint32_t range_start, 
		uint32_t range_end
	)
	{
        assert(range_end - range_start == metis_graph->node_count);

        if (metis_graph->node_count <= _max_part_size)
        {
            part_ranges.push_back(std::make_pair(range_start, range_end));
            return range_end;
        }

        uint32_t part_size_expectation = (_min_part_size + _max_part_size) / 2u;
        uint32_t part_count_expectation = std::max(2u, (metis_graph->node_count + part_size_expectation - 1) / part_size_expectation);

        std::vector<int32_t> partition_result(metis_graph->node_count);
        int32_t node_weight_dimension = 1, part_num = 2, edge_cut_num = 0;

        // 若 part_count_expectation 为偶数, 则 part_weight = { 0.5, 0.5 };
        // 若为奇数, 则当 part_count_expectation = 3 时, part_weight = { 1 / 3, 2 / 3 }, 
        // 随着 part_count_expectation 增大, part_weight 越来越接近 { 0.5, 0.5 }.
        double part_weight[]={
            static_cast<double>(part_count_expectation >> 1) / part_count_expectation,
            1.0 - static_cast<double>(part_count_expectation >> 1) / part_count_expectation
        };

        // 其结果存储在 partition_result 内, 若为 0, 则属于左区, 为 1 属于右区.
        ReturnIfFalse(
            METIS_PartGraphRecursive(
                &metis_graph->node_count, 
                &node_weight_dimension, 
                metis_graph->node_adjacency_start_index.data(), 
                metis_graph->adjandency_nodes.data(), 
                nullptr, 
                nullptr, 
                metis_graph->adjandency_node_weights.data(), 
                &part_num, 
                part_weight, 
                nullptr, 
                nullptr, 
                &edge_cut_num, 
                partition_result.data()
            ) == METIS_OK
        );

        uint32_t left = 0;
        uint32_t right = metis_graph->node_count - 1;
        std::vector<uint32_t> swap_node_map(metis_graph->node_count);

        while (left <= right)
        {
            while (left <= right && partition_result[left] == 0)
            {
                swap_node_map[left] = left;
                left++;
            }
            while (left <= right && partition_result[right] == 1)
            {
                swap_node_map[right] = right;
                right--;
            }

            if (left < right)
            {
                std::swap(node_indices[range_start + left], node_indices[range_start + right]);
                swap_node_map[left] = right;
                swap_node_map[right] = left;
                left++;
                right--;
            }
        }

        uint32_t split_pos = left;

        uint32_t child_graph_size[2] = { split_pos, metis_graph->node_count - split_pos };
        ReturnIfFalse(child_graph_size[0] >= 1 && child_graph_size[1] >= 1);


        if (child_graph_size[0] <= _max_part_size && child_graph_size[1] <= _max_part_size)
        {
            part_ranges.push_back(std::make_pair(range_start, range_start + split_pos));
            part_ranges.push_back(std::make_pair(range_start + split_pos, range_end));
        }
        else 
        {
            for (uint32_t ix = 0; ix < 2; ++ix)
            {
                child_metis_graph[ix] = new MetisGraph();
                child_metis_graph[ix]->node_count = child_graph_size[ix];
                child_metis_graph[ix]->node_adjacency_start_index.reserve(child_graph_size[ix] + 1);
                child_metis_graph[ix]->adjandency_nodes.reserve(metis_graph->adjandency_nodes.size() >> 1);
                child_metis_graph[ix]->adjandency_node_weights.reserve(metis_graph->adjandency_node_weights.size() >> 1);
            }

            // 填充 child_metis_graph.
            for (uint32_t ix = 0; ix < metis_graph->node_count; ++ix)
            {
                // 判断左区还是右区.
                uint32_t current_child_graph_index = ix >= child_graph_size[0] ? 1 : 0;
                
                uint32_t mapped_index = swap_node_map[ix];
                MetisGraph* current_child_graph = child_metis_graph[current_child_graph_index];
                current_child_graph->node_adjacency_start_index.push_back(current_child_graph->adjandency_nodes.size());
                for (
                    uint32_t jx = metis_graph->node_adjacency_start_index[mapped_index]; 
                    jx < metis_graph->node_adjacency_start_index[mapped_index + 1]; 
                    ++jx
                )
                {
                    uint32_t adjancency_node_weight = metis_graph->adjandency_node_weights[jx];
                    uint32_t adjancency_node_index = metis_graph->adjandency_nodes[jx];
                    adjancency_node_index = swap_node_map[adjancency_node_index] - (current_child_graph_index == 1 ? child_graph_size[0] : 0);
                    if (adjancency_node_index >= 0 && adjancency_node_index < child_graph_size[current_child_graph_index])
                    {
                        current_child_graph->adjandency_nodes.push_back(adjancency_node_index);
                        current_child_graph->adjandency_node_weights.push_back(adjancency_node_weight);
                    }
                }
            }
            child_metis_graph[0]->node_adjacency_start_index.push_back(child_metis_graph[0]->adjandency_nodes.size());
            child_metis_graph[1]->node_adjacency_start_index.push_back(child_metis_graph[1]->adjandency_nodes.size());
        }
        
		return range_start + split_pos;
	}

}