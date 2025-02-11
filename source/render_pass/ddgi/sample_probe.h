#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "../../render_graph/render_pass.h"
#include "ddgi_volume.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct SampleProbePassConstant
		{
            float3 camera_position;
            float pad = 0.0f;

            DDGIVolumeDataGpu volume_data;
		};
	}

	class SampleProbePass : public RenderPassInterface
	{
	public:
		SampleProbePass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		bool _resource_writed = false;
		constant::SampleProbePassConstant _pass_constant;

		std::shared_ptr<TextureInterface> _ddgi_irradiance_texture;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif


