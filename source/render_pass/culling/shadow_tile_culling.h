#ifndef RENDER_SHADOW_TILE_CULLING_PASS_H
#define RENDER_SHADOW_TILE_CULLING_PASS_H

#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_mesh.h"
#include "../../scene/virtual_texture.h"
#include <memory>
#include <vector>

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
            
            uint32_t cluster_tirangle_num = MeshCluster::cluster_tirangle_num;
			uint32_t axis_shadow_tile_num = VT_VIRTUAL_SHADOW_RESOLUTION / VT_SHADOW_PAGE_SIZE;
			float shadow_orthographic_length = 0.0f;
		};
	}

	class ShadowTileCullingPass : public RenderPassInterface
	{
	public:
		ShadowTileCullingPass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		bool _mesh_update = false;
		constant::ShadowTileCullingConstant _pass_constant;
		std::vector<uint2>* _update_shadow_pages = nullptr;

        std::shared_ptr<BufferInterface> _vt_shadow_draw_indirect_buffer;
        std::shared_ptr<BufferInterface> _vt_shadow_visible_cluster_buffer;

        BindingSetItemArray _binding_set_items;
        std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif
