#ifndef RENDER_PASS_VIRTUAL_TEXTURE_UPDATE_H
#define RENDER_PASS_VIRTUAL_TEXTURE_UPDATE_H

#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_texture.h"
#include "../../scene/geometry.h"
#include <array>
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct VirtualGeometryTexturePassConstant
		{
			uint2 client_resolution = { CLIENT_WIDTH, CLIENT_HEIGHT };
            uint32_t vt_page_size;
            uint32_t vt_physical_texture_size;
		};
	}

	
	struct ShadowTileInfo
	{
		uint2 id;
		float4x4 view_matrix;
	};

	class VirtualTextureUpdatePass : public RenderPassInterface
	{
	public:
		VirtualTextureUpdatePass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;
		bool finish_pass(RenderResourceCache* cache) override;

	private:
		std::vector<std::pair<const std::string*, uint32_t>> _submesh_name_cache;
		bool _update_submesh_name_cache = false;

		uint64_t _finish_pass_thread_id = INVALID_SIZE_64;
		constant::VirtualGeometryTexturePassConstant _pass_constant;

		PhysicalTileLruCache* _physical_tile_lru_cache = nullptr;


		std::shared_ptr<BufferInterface> _vt_physical_tile_lru_cache_read_back_buffer;
		
		std::array<std::shared_ptr<TextureInterface>, Material::TextureType_Num> _vt_physical_textures;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif







