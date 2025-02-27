#ifndef RENDER_PASS_VIRTUAL_SHADOW_MAP_H
#define RENDER_PASS_VIRTUAL_SHADOW_MAP_H

#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_texture.h"
#include "../../core/math/matrix.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct VirtualShadowMapPassConstant
		{
			float4x4 view_proj;
			float4x4 view_matrix;
			uint2 page_size = uint2(VT_VIRTUAL_SHADOW_RESOLUTION / VT_SHADOW_PAGE_SIZE);
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
		constant::VirtualShadowMapPassConstant _pass_constant;

		std::shared_ptr<TextureInterface> _physical_shadow_map_texture;
		std::shared_ptr<TextureInterface> _fake_render_target_texture;
		
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