#ifndef RENDER_PASS_VIRTUAL_TEXTURE_UPDATE_H
#define RENDER_PASS_VIRTUAL_TEXTURE_UPDATE_H

#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_texture.h"
#include "../../scene/geometry.h"
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace fantasy
{
	namespace constant
	{
		struct VirtualTextureUpdatePassConstant
		{
			uint2 client_resolution = { CLIENT_WIDTH, CLIENT_HEIGHT };
            uint32_t vt_page_size = VT_PAGE_SIZE;
            uint32_t vt_physical_texture_size = VT_PHYSICAL_TEXTURE_RESOLUTION;

			uint32_t vt_texture_id_offset;
			uint32_t vt_texture_mip_offset[VT_TEXTURE_MIP_LEVELS];
			uint32_t vt_axis_mip_tile_num[VT_TEXTURE_MIP_LEVELS];
		};
	}

	class VirtualTextureUpdatePass : public RenderPassInterface
	{
	public:
		VirtualTextureUpdatePass() {  type = RenderPassType::Compute | RenderPassType::Immediately; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private: 
		bool update_texture_region_cache(DeviceInterface* device, RenderResourceCache* cache);

	private:
		constant::VirtualTextureUpdatePassConstant _pass_constant;

		std::vector<uint32_t> _vt_indirect_data;

		std::vector<VTShadowPage> _vt_new_shadow_pages;
		VTPhysicalShadowTable _vt_physical_shadow_table;

		bool _update_texture_region_cache = false;
		VTPhysicalTable _vt_physical_table;

		uint2 _vt_feed_back_resolution;
		std::vector<uint3> _vt_feed_back_data;
		std::unordered_map<uint64_t, std::pair<TextureTilesMapping::Region, uint32_t>> _geometry_texture_region_cache;

		std::shared_ptr<HeapInterface> _geometry_texture_heap;
		
		std::shared_ptr<BufferInterface> _vt_feed_back_read_back_buffer;
		
		std::shared_ptr<TextureInterface> _vt_shadow_indirect_texture;
		std::shared_ptr<BufferInterface> _vt_indirect_buffer;
		std::array<std::shared_ptr<TextureInterface>, Material::TextureType_Num> _vt_physical_textures;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		BindingSetItemArray _binding_set_items;
		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif







