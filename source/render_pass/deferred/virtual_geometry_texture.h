#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "../../render_graph/render_pass.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct VirtualGeometryTexturePassConstant
		{
            uint32_t vt_page_size;
            uint32_t vt_physical_texture_size;
		};
	}

	class VirtualGeometryTexturePass : public RenderPassInterface
	{
	public:
		VirtualGeometryTexturePass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		bool _resource_writed = false;
		constant::VirtualGeometryTexturePassConstant _pass_constant;

		std::shared_ptr<BufferInterface> _buffer;
		std::shared_ptr<TextureInterface> _texture;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif







