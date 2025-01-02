#ifndef RENDER_PASS_SUN_DISK_H
#define RENDER_PASS_SUN_DISK_H

#include "../../render_graph/render_pass.h"
#include "../../core/math/matrix.h"
#include <memory>


namespace fantasy
{
	namespace constant
	{
		struct SunDiskPassConstant
		{
			float4x4 world_view_proj;

			float3 sun_radius;
			float sun_theta = 0.0f;

			float camera_height = 0.0f;
			float3 pad;
		};
	}

	class SunDiskPass : public RenderPassInterface
	{
	public:
		SunDiskPass() { type = RenderPassType::Graphics; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

		bool finish_pass() override;

		friend class AtmosphereTest;

	private:
		void GenerateSunDiskVertices();

	private:
		struct Vertex
		{
			float2 position;
		};

		bool _resource_writed = false;
		float _sun_disk_size = 0.01f;
		std::vector<Vertex> _sun_disk_vertices;
		constant::SunDiskPassConstant _pass_constant;

		std::shared_ptr<BufferInterface> _pass_constant_buffer;
		std::shared_ptr<BufferInterface> _vertex_buffer;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;
		std::unique_ptr<InputLayoutInterface> _input_layout;

		std::unique_ptr<Shader> _vs;
		std::unique_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		GraphicsState _graphics_state;
	};
}





#endif