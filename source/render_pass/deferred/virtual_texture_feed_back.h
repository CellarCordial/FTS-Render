#ifndef RENDER_PASS_VIRTUAL_TEXTURE_FEED_BACK_H
#define RENDER_PASS_VIRTUAL_TEXTURE_FEED_BACK_H

#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_texture.h"
#include "../../core/math/matrix.h"
#include <cstdint>
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct VirtualTextureFeedBackPassConstant
		{
			float4x4 shadow_view_proj;

			uint32_t client_width = CLIENT_WIDTH;
			uint32_t vt_page_size = VT_PAGE_SIZE;
			uint32_t virtual_shadow_resolution = VT_VIRTUAL_SHADOW_RESOLUTION;
			uint32_t virtual_shadow_page_size = VT_SHADOW_PAGE_SIZE;
		};
	}

	class VirtualTextureFeedBackPass : public RenderPassInterface
	{
	public:
        VirtualTextureFeedBackPass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		constant::VirtualTextureFeedBackPassConstant _pass_constant;
		
		std::shared_ptr<BufferInterface> _vt_feed_back_buffer;
		std::shared_ptr<BufferInterface> _vt_feed_back_read_back_buffer;

		std::shared_ptr<TextureInterface> _vt_page_uv_texture;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif







