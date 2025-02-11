#ifndef RENDER_PASS_H
#define RENDER_PASS_H
 
#include "../render_graph/render_pass.h"
#include "../core/math/matrix.h"
#include "../scene/geometry.h"
 
namespace fantasy
{
	namespace constant
	{
		struct VirtualGeometryTestPassConstant
		{

		};
	}

	class VirtualGeometryTestPass : public RenderPassInterface
	{
	public:
		VirtualGeometryTestPass() { type = RenderPassType::Graphics; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		constant::VirtualGeometryTestPassConstant _pass_constant;

		std::shared_ptr<BufferInterface> _buffer;
		std::shared_ptr<TextureInterface> _texture;
		
		std::shared_ptr<BindingLayoutInterface> _binding_layout;
		std::unique_ptr<InputLayoutInterface> _input_layout;

		std::shared_ptr<Shader> _vs;
		std::shared_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		GraphicsState _graphics_state;
        DrawArguments _draw_arguments;
	};

}
 
#endif


