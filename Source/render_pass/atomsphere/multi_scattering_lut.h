#ifndef RENDER_PASS_ATOMSPHERE_MULTISCATTERING_LUT_H
#define RENDER_PASS_ATOMSPHERE_MULTISCATTERING_LUT_H

#include "../../render_graph/render_graph.h"
#include "../../core/math/vector.h"
#include <memory>

namespace fantasy 
{
    namespace constant
    {
        struct MultiScatteringPassConstant
        {
            Vector3F sun_intensity = Vector3F(1.0f, 1.0f, 1.0f);
            int32_t ray_march_step_count = 256;

            Vector3F ground_albedo;
            float pad = 0.0f;
        };
    };


    class MultiScatteringLUTPass : public RenderPassInterface
    {
    public:
        MultiScatteringLUTPass()
        {
            type = RenderPassType::Precompute;
            create_poisson_disk_samples();
        }

        bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
        bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

        bool finish_pass() override;

		friend class AtmosphereDebugRender;

    private:
        void create_poisson_disk_samples();

    private:
        bool _resource_writed = false;
        std::vector<Vector2F> _dir_samples;
        constant::MultiScatteringPassConstant _pass_constants;

        std::shared_ptr<BufferInterface> _pass_constant_buffer;
        std::shared_ptr<BufferInterface> _dir_sample_buffer;
        std::shared_ptr<TextureInterface> _multi_scattering_texture;

        std::unique_ptr<BindingLayoutInterface> _binding_layout;

        std::unique_ptr<Shader> _cs;
        std::unique_ptr<ComputePipelineInterface> _pipeline;

        std::unique_ptr<BindingSetInterface> _binding_set;
        ComputeState _compute_state;
    };

}












#endif