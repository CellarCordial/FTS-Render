
#include <vector>
#include <map>

namespace fantasy 
{
    using SimpleGraph = std::vector<std::map<uint32_t, int32_t>>; 

	class GraphPartitionar
	{
	public:
		bool partition_graph(const SimpleGraph& graph, uint32_t min_part_size, uint32_t max_part_size);

		std::vector<std::pair<uint32_t, uint32_t>> part_ranges;
		std::vector<uint32_t> node_indices;	// new index.
		std::vector<uint32_t> node_map;		// [new index] = old index.
	
	private:
		struct MetisGraph
		{
			int32_t node_count = 0;
			std::vector<int32_t> node_adjacency_start_index;
			std::vector<int32_t> adjandency_nodes;
			std::vector<int32_t> adjandency_node_weights;
		};

		bool bisect_graph_resursive(MetisGraph* metis_graph, uint32_t range_start, uint32_t range_end);
		uint32_t bisect_graph(
			MetisGraph* metis_graph, 
			MetisGraph* child_metis_graph[2], 
			uint32_t range_start, 
			uint32_t range_end
		);

	private:
		uint32_t _min_part_size = 0;
		uint32_t _max_part_size = 0;
	};

}