#ifndef RENDER_PASS_VIRTUAL_SHADOW_MAP_H
#define RENDER_PASS_VIRTUAL_SHADOW_MAP_H

#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_texture.h"
#include "../../scene/virtual_mesh.h"
#include "../../scene/light.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct ShadowCullingConstant
		{
			float4x4 shadow_view_matrix;
			float4x4 shadow_proj_matrix;
			
			uint32_t packed_shadow_page_id = 0;
            uint32_t group_count = 0;
            float near_plane = 0.0f;
            float far_plane = 0.0f;
			
			float shadow_orthographic_length = 0.0f;
            uint32_t cluster_tirangle_num = MeshCluster::cluster_tirangle_num;
			uint32_t shadow_map_resolution = 0;
			uint32_t hzb_resolution = 0;
		};

		struct ShadowHiZUpdatePassConstant
		{
            uint2 shadow_map_resolution;
            uint32_t hzb_resolution = 0;
            uint32_t last_mip_level = 0;
		};

		struct ShadowPassConstant
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

		DirectionalLight* _directional_light = nullptr;
		uint32_t* _cluster_group_count = nullptr;

		constant::ShadowPassConstant _pass_constant;


		// Culling Pass.
		std::vector<float4x4> _shadow_tile_view_matrixs;
		std::vector<uint2>* _update_shadow_pages = nullptr;
		std::vector<constant::ShadowCullingConstant> _cull_pass_constants;

        std::shared_ptr<BufferInterface> _vt_shadow_draw_indirect_buffer;
        std::shared_ptr<BufferInterface> _vt_shadow_visible_cluster_buffer;

        BindingSetItemArray _cull_binding_set_items;
        std::shared_ptr<BindingLayoutInterface> _cull_binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _cull_pipeline;

		std::unique_ptr<BindingSetInterface> _cull_binding_set;
		ComputeState _compute_state;


		// Shadow Map Pass.
		uint32_t _shadow_map_resolution = 2048;
		constant::ShadowCullingConstant _shadow_map_cull_constant;

		std::shared_ptr<TextureInterface> _shadow_map_texture;
		
		std::unique_ptr<FrameBufferInterface> _shadow_map_frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _shadow_map_pipeline;
		GraphicsState _shadow_map_graphics_state;


		// Hi-Z Update Pass.
		uint32_t _shadow_hzb_resolution = 1024;
		std::shared_ptr<TextureInterface> _shadow_hi_z_texture;

		std::vector<constant::ShadowHiZUpdatePassConstant> _hi_z_update_pass_constants;

		std::shared_ptr<BindingLayoutInterface> _hi_z_update_binding_layout;

		std::shared_ptr<Shader> _hi_z_update_cs;
		std::unique_ptr<ComputePipelineInterface> _hi_z_update_pipeline;

		std::unique_ptr<BindingSetInterface> _hi_z_update_binding_set;
		ComputeState _hi_z_update_compute_state;
		
		
		// Virtual Shadow Pass.
		std::shared_ptr<TextureInterface> _vt_physical_shadow_texture;
		std::shared_ptr<TextureInterface> _black_render_target_texture;
		
		std::shared_ptr<BindingLayoutInterface> _virtual_shadow_binding_layout;
		
		std::shared_ptr<Shader> _virtual_shadow_vs;
		std::shared_ptr<Shader> _virtual_shadow_ps;
		
		std::unique_ptr<FrameBufferInterface> _virtual_shadow_frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _virtual_shadow_pipeline;
		
		BindingSetItemArray _virtual_shadow_binding_set_items;
		std::unique_ptr<BindingSetInterface> _virtual_shadow_binding_set;
		GraphicsState _virtual_shadow_graphics_state;
	};
}






#endif