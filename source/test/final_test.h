#ifndef TEST_RESTIR_H
#define TEST_RESTIR_H

#include "../render_graph/render_graph.h"
#include "../core/math/vector.h"
#include "test_base.h"
#include <cstdint>
#include <memory>


namespace fantasy 
{
    namespace constant 
    {
        struct FinalTestPassConstant
        {
            int32_t show_type = 0;
            uint2 client_resolution = { CLIENT_WIDTH, CLIENT_HEIGHT };
        };
    }

    class FinalTestPass : public RenderPassInterface
    {
    public:
		FinalTestPass() { type = RenderPassType::Compute; }

        bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
        bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

    private:
		constant::FinalTestPassConstant _pass_constant;

        std::shared_ptr<TextureInterface> _final_texture;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;
		std::shared_ptr<InputLayoutInterface> _input_layout;

		std::shared_ptr<Shader> _cs;

		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
    };


    class FinalTest : public TestBase
    {
    public:
        FinalTest(GraphicsAPI api);
        RenderPassInterface* init_render_pass(RenderGraph* render_graph) override;

    private:
    };

}








#endif