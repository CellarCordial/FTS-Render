#ifndef TEST_RESTIR_H
#define TEST_RESTIR_H

#include "../render_graph/render_graph.h"
#include "test_base.h"
#include <cstdint>
#include <memory>


namespace fantasy 
{
    namespace constant 
    {
        struct FinalTestPassConstant
        {
            int32_t show_type;
        };
    }

    class FinalTestPass : public RenderPassInterface
    {
    public:
		FinalTestPass() { type = RenderPassType::Graphics; }

        bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
        bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

    private:
		constant::FinalTestPassConstant _pass_constant;

        std::shared_ptr<TextureInterface> _final_texture;

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


    class FinalTest : public TestBase
    {
    public:
        FinalTest(GraphicsAPI api) : TestBase(api) {}
        RenderPassInterface* init_render_pass(RenderGraph* render_graph) override;

    private:
		float _world_scale = 200.0f;
    };

}








#endif