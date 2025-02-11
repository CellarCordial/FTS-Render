#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "../../render_graph/render_pass.h"
#include <memory>

namespace fantasy
{
	class MipmapGenerationPass : public RenderPassInterface
	{
	public:
		MipmapGenerationPass() { type = RenderPassType::Precompute | RenderPassType::Exclude; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

        bool finish_pass() override;

	private:
		Entity* _current_model = nullptr;
		uint32_t _current_image_index = 0;
		uint32_t _current_submaterial_index = 0;

		std::vector<std::shared_ptr<TextureInterface>> _textures;

        std::shared_ptr<SamplerInterface> _linear_clamp_sampler;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif
