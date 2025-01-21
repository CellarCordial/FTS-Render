#ifndef RENDER_PASS_VIRTUAL_SHADOW_MAP_H
#define RENDER_PASS_VIRTUAL_SHADOW_MAP_H




#include "../../render_graph/render_pass.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct VirtualShadowMapPassConstant
		{
			float4x4 shadow_view_proj;

			float3 camera_position;
			uint32_t virtual_shadow_resolution;

			uint32_t virtual_shadow_page_size;
			uint32_t client_width;
		};
	}

	class VirtualShadowMapPass : public RenderPassInterface
	{
	public:
		VirtualShadowMapPass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		bool _resource_writed = false;
		constant::VirtualShadowMapPassConstant _pass_constant;

		std::shared_ptr<BufferInterface> _virtual_shadow_page_buffer;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}






#endif