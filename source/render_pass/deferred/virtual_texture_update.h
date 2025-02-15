#ifndef RENDER_PASS_VIRTUAL_TEXTURE_UPDATE_H
#define RENDER_PASS_VIRTUAL_TEXTURE_UPDATE_H

#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_texture.h"
#include "../../scene/geometry.h"
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

	class VirtualTextureUpdatePass : public RenderPassInterface
	{
	public:
		VirtualTextureUpdatePass() { type = RenderPassType::Compute | RenderPassType::Immediately; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		constant::VirtualGeometryTexturePassConstant _pass_constant;
		uint32_t _shadow_tile_num = 0;

		VTIndirectTable _vt_indirect_table;
		VTPhysicalTable _vt_physical_table;
		VTPhysicalTable _physical_shadow_table;

		std::shared_ptr<BufferInterface> _shadow_tile_info_buffer;
		std::shared_ptr<TextureInterface> _vt_indirect_texture;
		std::array<std::shared_ptr<TextureInterface>, Material::TextureType_Num> _vt_physical_textures;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif







