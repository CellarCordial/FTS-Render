#ifndef RENDER_PASS_VIRTUAL_SHADOW_MAP_H
#define RENDER_PASS_VIRTUAL_SHADOW_MAP_H

#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_texture.h"
#include "../../scene/virtual_mesh.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct ShadowTileCullingConstant
		{
			float4x4 shadow_view_matrix;
			
			uint32_t packed_shadow_page_id = 0;
            uint32_t group_count = 0;
            float near_plane = 0.0f;
            float far_plane = 0.0f;
			
            uint32_t cluster_tirangle_num = MeshCluster::cluster_tirangle_num;
			float shadow_orthographic_length = 0.0f;
		};

		struct VirtualShadowMapPassConstant
		{
			float4x4 view_proj;
			float4x4 view_matrix;
			uint32_t page_size = VT_SHADOW_PAGE_SIZE;
		};
	}

	class VirtualShadowMapPass : public RenderPassInterface
	{
	public:
		VirtualShadowMapPass() { type = RenderPassType::Graphics; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		bool _resource_writed = false;
		uint32_t* _cluster_group_count = nullptr;
		std::vector<uint2>* _update_shadow_pages = nullptr;
		std::vector<float4x4> _shadow_tile_view_matrixs;
		constant::VirtualShadowMapPassConstant _pass_constant;
		std::vector<constant::ShadowTileCullingConstant> _cull_pass_constants;

		// Culling Pass.
        std::shared_ptr<BufferInterface> _vt_shadow_draw_indirect_buffer;
        std::shared_ptr<BufferInterface> _vt_shadow_visible_cluster_buffer;

        BindingSetItemArray _cull_binding_set_items;
        std::shared_ptr<BindingLayoutInterface> _cull_binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _cull_pipeline;

		std::unique_ptr<BindingSetInterface> _cull_binding_set;
		ComputeState _compute_state;


		// Shadow Map Pass.
		std::shared_ptr<TextureInterface> _physical_shadow_map_texture;
		std::shared_ptr<TextureInterface> _black_render_target_texture;
		
		BindingSetItemArray _binding_set_items;
		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _vs;
		std::shared_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		GraphicsState _graphics_state;
        DrawArguments _draw_arguments;
	};
}






#endif