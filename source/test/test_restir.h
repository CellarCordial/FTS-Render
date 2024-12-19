#ifndef TEST_RESTIR_H
#define TEST_RESTIR_H

#include "../render_graph/render_graph.h"
#include "../render_pass/deferred/gbuffer.h"
#include <cstdint>
#include <memory>


namespace fantasy 
{
    namespace constant 
    {
        struct RestirTestPassConstant
        {
            uint32_t show_type;
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

		std::unique_ptr<BindingLayoutInterface> _binding_layout;
		std::unique_ptr<InputLayoutInterface> _input_layout;

		std::unique_ptr<Shader> _vs;
		std::unique_ptr<Shader> _ps;

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
        RenderPassInterface* get_last_pass() { return _test_pass.get(); }

    private:
        std::shared_ptr<GBufferPass> _gbuffer_pass;
        std::shared_ptr<RestirTestPass> _test_pass; 
    };

}








#endif