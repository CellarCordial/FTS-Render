#ifndef RENDER_HIERARCHICAL_ZBUFFER_PASS_H
#define RENDER_HIERARCHICAL_ZBUFFER_PASS_H
 
#include "../../render_graph/render_pass.h"
 
namespace fantasy
{
	namespace constant
	{
		struct HierarchicalZBufferPassConstant
		{
            uint2 client_resolution = uint2{ CLIENT_WIDTH, CLIENT_HEIGHT };
            uint32_t hzb_resolution = 0;
            uint32_t last_mip_level = 0;
		};
	}

	class HierarchicalZBufferPass : public RenderPassInterface
	{
	public:
		HierarchicalZBufferPass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		std::vector<constant::HierarchicalZBufferPassConstant> _pass_constants;
    
		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};

}
 
#endif

