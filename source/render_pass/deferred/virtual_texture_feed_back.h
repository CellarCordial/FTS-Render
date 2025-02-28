#ifndef RENDER_PASS_VIRTUAL_TEXTURE_FEED_BACK_H
#define RENDER_PASS_VIRTUAL_TEXTURE_FEED_BACK_H

#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_texture.h"
#include "../../scene/geometry.h"
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace fantasy
{
	namespace constant
	{
		struct VirtualTextureFeedBackPassConstant
		{
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
		

		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif







