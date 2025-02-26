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
		struct VirtualGeometryTexturePassConstant
		{
			uint2 client_resolution = { CLIENT_WIDTH, CLIENT_HEIGHT };
            uint32_t vt_page_size = VT_PAGE_SIZE;
            uint32_t vt_physical_texture_size = VT_PHYSICAL_TEXTURE_RESOLUTION;
		};
	}

	

	class VirtualTextureUpdatePass : public RenderPassInterface
	{
	public:
		VirtualTextureUpdatePass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;
		bool finish_pass(RenderResourceCache* cache) override;

	private:
		constant::VirtualGeometryTexturePassConstant _pass_constant;
		uint64_t _finish_pass_thread_id = INVALID_SIZE_64;
		
		VTPhysicalTable _vt_physical_table;
		VTIndirectTable _vt_indirect_table;

		uint2 _vt_feed_back_resolution;
		std::vector<uint2> _vt_feed_back_data;
		bool _update_texture_region_cache = false;
		std::unordered_map<uint64_t, std::pair<TextureTilesMapping::Region, uint32_t>> _geometry_texture_region_cache;


		std::shared_ptr<HeapInterface> _geometry_texture_heap;
		
		std::shared_ptr<BufferInterface> _vt_page_read_back_buffer;
		
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







