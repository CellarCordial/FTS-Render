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

	struct VirtualTexturePositionInfo
	{
		VTPage* page = nullptr;
		uint2 page_physical_pos_in_page;
		std::array<TextureInterface*, Material::TextureType_Num> texture;
	};


	class VirtualTextureUpdatePass : public RenderPassInterface
	{
	public:
		VirtualTextureUpdatePass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;
		bool finish_pass(RenderResourceCache* cache) override;

	private:
		struct MaterialCache
		{
			const Material::SubMaterial* submaterial = nullptr;
			const std::string* model_name = nullptr;
			uint32_t submesh_id = INVALID_SIZE_32;
			uint32_t texture_resolution = 0;
		};
		std::vector<MaterialCache> _material_cache;
		constant::VirtualGeometryTexturePassConstant _pass_constant;
		uint64_t _finish_pass_thread_id = INVALID_SIZE_64;
		// VTMipmapLUT _virtual_shadow_page_lut;
		// uint32_t _shadow_tile_num = 0;
		bool _update_material_cache = false;

		VTIndirectTable _vt_indirect_table;
		VTPhysicalTable _vt_physical_table;
		// VTPhysicalTable _physical_shadow_table;

		std::vector<VTMipmapLUT> _vt_mipmap_luts;
		std::vector<VirtualTexturePositionInfo> _virtual_texture_position_infos;
		// std::vector<ShadowTileInfo> _shadow_tile_infos;


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







