#ifndef RENDER_PASS_RAY_TRACING_TRIANGLE_TEST_H
#define RENDER_PASS_RAY_TRACING_TRIANGLE_TEST_H

#include "../render_graph/render_pass.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct RayTracingPassConstant
		{

		};
	}

	class RayTracingTestPass : public RenderPassInterface
	{
	public:
		RayTracingTestPass() { type = RenderPassType::Graphics; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		bool _resource_writed = false;
		constant::RayTracingPassConstant _pass_constant;

		std::shared_ptr<BufferInterface> _index_buffer;
		std::shared_ptr<BufferInterface> _vertex_buffer;
		std::shared_ptr<TextureInterface> _texture;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _cs;
		std::unique_ptr<ray_tracing::PipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ray_tracing::PipelineState _ray_tracing_state;
	};
}
#endif
