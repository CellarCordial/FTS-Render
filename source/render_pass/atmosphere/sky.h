#ifndef RENDER_PASS_SKY_H
#define RENDER_PASS_SKY_H

#include "../../render_graph/render_graph.h"
#include "../../core/math/vector.h"
#include <memory>


namespace fantasy
{
	namespace constant
	{
		struct SkyPassConstant
		{
			float3 frustum_a; float pad0 = 0.0f;
			float3 frustum_b; float pad1 = 0.0f;
			float3 frustum_c; float pad2 = 0.0f;
			float3 frustum_d; float pad3 = 0.0f;
		};
	}

	class SkyPass : public RenderPassInterface
	{
	public:
		SkyPass() { type = RenderPassType::Graphics; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

		friend class AtmosphereTest;

	private:
		constant::SkyPassConstant _pass_constant;

		std::shared_ptr<TextureInterface> _depth_texture;
		std::shared_ptr<SamplerInterface> _sampler; // U_Wrap VW_Clamp Linear

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