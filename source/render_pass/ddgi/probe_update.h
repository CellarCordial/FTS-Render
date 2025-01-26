#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "../../render_graph/render_pass.h"
#include "ddgi_volume.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct ProbeUpdatePassConstant
		{
			uint32_t ray_count_per_probe = 0;
			float history_alpha = 0.0f;
			float history_gamma = 0.0f;
			uint32_t first_frame = 0;

			float depth_sharpness = 0.0f;
		};
	}

	class ProbeUpdatePass : public RenderPassInterface
	{
	public:
		ProbeUpdatePass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		bool _resource_writed = false;
		constant::ProbeUpdatePassConstant _pass_constant;
		DDGIVolumeDataGpu* _volume_data = nullptr;
		uint32_t* _frame_count = nullptr;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _depth_update_cs;
		std::unique_ptr<Shader> _irradiance_update_cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _depth_update_binding_set;
		std::unique_ptr<BindingSetInterface> _irradiance_update_binding_set;
		ComputeState _compute_state;
	};
}
#endif


