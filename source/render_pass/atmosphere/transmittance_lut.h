#ifndef RENDER_PASS_ATOMSPHERE_TRANSMITTANCE_LUT_H
#define RENDER_PASS_ATOMSPHERE_TRANSMITTANCE_LUT_H

#include "../../render_graph/render_pass.h"
#include "atmosphere_properties.h"
#include <memory>

namespace fantasy 
{
    class TransmittanceLUTPass : public RenderPassInterface
    {
    public:
        TransmittanceLUTPass() { type = RenderPassType::Precompute; }

        bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
        bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

		friend class AtmosphereTest;

    private:
		float _world_scale = 200.0f;
        constant::AtmosphereProperties _standard_atomsphere_properties;

		std::shared_ptr<BufferInterface> _atomsphere_properties_buffer;
        std::shared_ptr<TextureInterface> _transmittance_texture;

        std::shared_ptr<BindingLayoutInterface> _binding_layout;
        
        std::shared_ptr<Shader> _cs;
        std::unique_ptr<ComputePipelineInterface> _pipeline;
        
        std::unique_ptr<BindingSetInterface> _binding_set;
        ComputeState _compute_state;
    };

}
















#endif