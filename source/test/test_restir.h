#ifndef TEST_RESTIR_H
#define TEST_RESTIR_H

#include "../render_graph/render_graph.h"
#include <cstdint>
#include <memory>


namespace fantasy 
{
    namespace constant 
    {
        struct RestirTestPassConstant
        {
            int32_t show_type;
        };
    }

    class RestirTestPass : public RenderPassInterface
    {
    public:
		RestirTestPass() { type = RenderPassType::Graphics; }

        bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
        bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

    private:
		constant::RestirTestPassConstant _pass_constant;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;
		std::shared_ptr<InputLayoutInterface> _input_layout;

		std::shared_ptr<Shader> _vs;
		std::shared_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		GraphicsState _graphics_state;
        DrawArguments _draw_arguments;
    };


    class RestirTest
    {
    public:
        bool setup(RenderGraph* render_graph);
        RenderPassInterface* get_last_pass() { return _last_pass; }

    private:
        RenderPassInterface* _last_pass; 
    };

}








#endif