
#ifndef RENDER_PASS_BRDF_LUT_H
#define RENDER_PASS_BRDF_LUT_H

#include "../../render_graph/render_pass.h"
#include "../../core/math/vector.h"
#include <memory>

namespace fantasy
{
	class BrdfLUTPass : public RenderPassInterface
	{
	public:
		BrdfLUTPass() { type = RenderPassType::Precompute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
        uint2 _brdf_lut_resolution = { 64, 64 };
		std::shared_ptr<TextureInterface> _brdf_lut_texture;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}

#endif
