#ifndef RENDER_SHADOW_TILE_CULLING_PASS_H
#define RENDER_SHADOW_TILE_CULLING_PASS_H

#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_mesh.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct ShadowTileCullingConstant
		{
            uint32_t shadow_tile_num;
            uint32_t group_count;
            float near_plane;
            float far_plane;
            
            uint32_t cluster_size = MeshCluster::cluster_size;
		};
	}

	class ShadowTileCullingPass : public RenderPassInterface
	{
	public:
		ShadowTileCullingPass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		bool _resource_writed = false;
		constant::ShadowTileCullingConstant _pass_constant;

        std::shared_ptr<BufferInterface> _virtual_shadow_draw_indirect_buffer;
        std::shared_ptr<BufferInterface> _virtual_shadow_visible_cluster_id_buffer;

        BindingSetItemArray _binding_set_items;
        std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif
