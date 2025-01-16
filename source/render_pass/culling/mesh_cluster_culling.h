#ifndef RENDER_PASS_H
#define RENDER_PASS_H

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
            float4x4 proj_matrix;

            uint32_t client_width = CLIENT_WIDTH;
            uint32_t client_height = CLIENT_HEIGHT;
            uint32_t camera_fov_y = 0;
            uint32_t hzb_resolution = 0;

            uint32_t group_count = 0;
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
		bool _resource_writed = false;
        uint32_t _hzb_resolution = 1024u;
		constant::MeshClusterCullingPassConstant _pass_constant;

        std::vector<MeshClusterGroup> _mesh_cluster_groups;
        std::vector<MeshCluster> _mesh_clusters;

		std::shared_ptr<BufferInterface> _mesh_cluster_group_buffer;
		std::shared_ptr<BufferInterface> _mesh_cluster_buffer;
		std::shared_ptr<BufferInterface> _visible_cluster_id_buffer;
		std::shared_ptr<BufferInterface> _virtual_gbuffer_indirect_buffer;
		std::shared_ptr<TextureInterface> _hierarchical_zbuffer_texture;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

        BindingSetItemArray _binding_set_items;
		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif

