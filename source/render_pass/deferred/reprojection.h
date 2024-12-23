#ifndef RENDER_PASS_REPROJECTION_H
#define RENDER_PASS_REPROJECTION_H

#include "../../render_graph/render_pass.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct ReprojectionPassConstant
		{
            Matrix4x4 view_matrix;
            Matrix4x4 proj_matrix;
		};
	}

	class ReprojectionPass : public RenderPassInterface
	{
	public:
		ReprojectionPass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;


	private:
		constant::ReprojectionPassConstant _pass_constant;

		std::shared_ptr<TextureInterface> _reprojection_texture;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif