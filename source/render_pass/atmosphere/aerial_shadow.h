#ifndef RENDER_PASS_AERIAL_SHADOW_H
#define RENDER_PASS_AERIAL_SHADOW_H

#include "../../render_graph/render_pass.h"
#include "../../core/math/matrix.h"
#include "../../scene/image.h"


namespace fantasy 
{
    namespace constant 
    {
        struct AerialShadowConstant
        {
            float4x4 shadow_view_proj;
        
            float3 sun_dir;
            float sun_theta;
        
            float3 sun_intensity;
            float max_aerial_distance;
        
            float2 jitter_factor;
            float2 blue_noise_uv_factor;
        
            float3 camera_position;
            float world_scale;
            
            uint2 client_resolution = { CLIENT_WIDTH, CLIENT_HEIGHT };
        };
    }

    class AerialShadowPass : public RenderPassInterface
    {
    public:
        AerialShadowPass() { type = RenderPassType::Compute; }

        bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
        bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

    private:
        constant::AerialShadowConstant _pass_constant;
        float _jitter_radius = 1.0f;
		Image _blue_noise_image;

		std::shared_ptr<TextureInterface> _blue_noise_texture;
        std::shared_ptr<TextureInterface> _final_texture;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;
		std::shared_ptr<InputLayoutInterface> _input_layout;

		std::shared_ptr<Shader> _cs;

		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
    };
}








#endif