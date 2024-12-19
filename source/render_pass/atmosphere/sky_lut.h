#ifndef RENDER_PASS_SKY_LUT_H
#define RENDER_PASS_SKY_LUT_H

#include "../../render_graph/render_graph.h"
#include "../../core/math/vector.h"
#include <memory>


namespace fantasy
{
	namespace constant
	{
		struct SkyLUTPassConstant
		{
			Vector3F camera_position;
			int32_t march_step_count = 40;

			Vector3F sun_direction;
			uint32_t enable_multi_scattering = 1;

			Vector3F sun_intensity;
			float pad = 0.0f;
		};
	}

	class SkyLUTPass : public RenderPassInterface
	{
	public:
		SkyLUTPass() { type = RenderPassType::Graphics; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

		friend class AtmosphereTest;

	private:
		constant::SkyLUTPassConstant _pass_constant;

		std::shared_ptr<BufferInterface> _pass_constant_buffer;
		std::shared_ptr<TextureInterface> _sky_lut_texture;

		std::shared_ptr<TextureInterface> _transmittance_texture;
		std::shared_ptr<TextureInterface> _multi_scattering_texture;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _vs;
		std::unique_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;
		
		std::unique_ptr<BindingSetInterface> _binding_set;
		GraphicsState _graphics_state;
	};
}





#endif