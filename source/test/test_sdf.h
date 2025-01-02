#ifndef TEST_SDF_MODE_H
#define TEST_SDF_MODE_H

#include "../render_graph/render_graph.h"
#include "../core/math/vector.h"
#include "../render_pass/sdf/sdf_generate.h"
#include "../render_pass/sdf/global_sdf.h"
#include <memory>


namespace fantasy 
{
    namespace constant
    {
        struct GlobalSdfData
        {
            float scene_grid_size = 0.0f;
            float3 scene_grid_origin; 

            uint32_t max_trace_steps = 1024;
            float abs_threshold = 0.01f;
            float default_march = 0.0f;
        };

		struct SdfDebugPassConstants
		{
			float3 frustum_a;     float pad0 = 0.0f;      
			float3 frustum_b;     float pad1 = 0.0f;      
			float3 frustum_c;     float pad2 = 0.0f;      
            float3 frustum_d;     float pad3 = 0.0f;
            float3 camera_position; float pad4 = 0.0f;

            GlobalSdfData sdf_data;         
            float pad5;
		};
    }


    class SdfDebugPass : public RenderPassInterface
    {
	public:
		SdfDebugPass() { type = RenderPassType::Graphics; }

        bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
        bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

    private:
        std::vector<float> sdf_data;
        constant::GlobalSdfData _global_sdf_data;         
        constant::SdfDebugPassConstants _pass_constants;
        
        std::shared_ptr<TextureInterface> _sdf_texture;

        std::unique_ptr<BindingLayoutInterface> _binding_layout;
        
        std::unique_ptr<Shader> _vs;
        std::unique_ptr<Shader> _ps;
        
        std::unique_ptr<FrameBufferInterface> _frame_buffer;
        std::unique_ptr<GraphicsPipelineInterface> _pipeline;

        std::unique_ptr<BindingSetInterface> _binding_set;
        GraphicsState _graphics_state;
    };

	class SdfTest
	{
    public:
        bool setup(RenderGraph* render_graph);
        RenderPassInterface* get_last_pass() { return sdf_debug_pass.get(); }

    private:
        std::shared_ptr<SdfGeneratePass> _sdf_generate_pass;
        std::shared_ptr<GlobalSdfPass> _global_sdf_pass;
        std::shared_ptr<SdfDebugPass> sdf_debug_pass;
	};
}










#endif