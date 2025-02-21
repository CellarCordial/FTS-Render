#ifndef RENDER_MESH_CLUSTER_CULLING_PASS_H
#define RENDER_MESH_CLUSTER_CULLING_PASS_H

#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_mesh.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct MeshClusterCullingPassConstant
		{
            float4x4 view_matrix;
            float4x4 reverse_z_proj_matrix;

            uint32_t client_width = CLIENT_WIDTH;
            uint32_t client_height = CLIENT_HEIGHT;
            uint32_t camera_fov_y = 0;
            uint32_t hzb_resolution = 0;

            uint32_t group_count = 0;
			uint32_t cluster_size = MeshCluster::cluster_size;
			float near_plane;
			float far_plane;
		};
	}

	class MeshClusterCullingPass : public RenderPassInterface
	{
	public:
		MeshClusterCullingPass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;
        bool finish_pass() override;

	private:
		bool _mesh_update = false;
		bool _resource_writed = false;
        uint32_t _hzb_resolution = 1024u;
		uint32_t _cluster_group_count = 0;
		constant::MeshClusterCullingPassConstant _pass_constant;

        std::vector<MeshClusterGroupGpu> _mesh_cluster_groups;
        std::vector<MeshClusterGpu> _mesh_clusters;

		std::shared_ptr<BufferInterface> _mesh_cluster_group_buffer;
		std::shared_ptr<BufferInterface> _mesh_cluster_buffer;
		std::shared_ptr<BufferInterface> _visible_cluster_id_buffer;
		std::shared_ptr<BufferInterface> _virtual_gbuffer_draw_indirect_buffer;
		std::shared_ptr<TextureInterface> _hierarchical_zbuffer_texture;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

        BindingSetItemArray _binding_set_items;
		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif

