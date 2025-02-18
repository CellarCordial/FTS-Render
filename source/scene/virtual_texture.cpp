#include "virtual_texture.h"
#include "../core/tools/morton_code.h"
#include "../core/parallel/parallel.h"
#include <cstdint>

#include "geometry.h"

namespace fantasy 
{
    bool MipmapLUT::initialize(uint32_t mip0_resolution, uint32_t max_mip_resolution)
    {
        ReturnIfFalse(is_power_of_2(mip0_resolution));
        
        _mip0_resolution = mip0_resolution;
        
        uint32_t mip0_resolution_in_page = mip0_resolution / VT_PAGE_SIZE;
        uint32_t max_mip_resolution_in_page = max_mip_resolution / VT_PAGE_SIZE;
        uint32_t mip_levels = std::log2(mip0_resolution_in_page / max_mip_resolution_in_page) + 1;

        _mips.resize(mip_levels);
        for (uint32_t ix = 0; ix < mip_levels; ++ix)
        {
            auto& mip = _mips[ix];
            
            mip.resolution = ix == 0 ? mip0_resolution : _mips[ix - 1].resolution >> 1;
            
            uint32_t resolution_in_page = mip.resolution / VT_PAGE_SIZE;
            mip.pages.resize(resolution_in_page * resolution_in_page);
            
            parallel::parallel_for(
                [&](uint64_t x, uint64_t y)
                {
                    uint32_t morton_code = MortonEncode(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
                    uint2 position = uint2(x * VT_PAGE_SIZE, y * VT_PAGE_SIZE);

                    auto& page = mip.pages[morton_code];
                    page.mip_level = ix;
                    page.base_position = position;
                }, 
                resolution_in_page, 
                resolution_in_page
            );
        }
        return true;
    }

    VTPage* MipmapLUT::query_page(uint2 page_id, uint32_t mip_level)
    {
        // mip 0 的 node_size_in_page 为 1, 逐级往上指数递增.
        uint32_t mip_node_size_in_page = std::pow(2, mip_level);
        return &_mips[mip_level].pages[MortonEncode(page_id / mip_node_size_in_page)];
    }

    uint2 VTPhysicalTable::add_page(VTPage* page)
    {
        uint64_t key = reinterpret_cast<uint64_t>(page);
        Tile* tile = _tiles.get(key);

        uint2 ret;
        if (tile == nullptr)
        {
            Tile evict_tile = _tiles.insert(key, Tile{ .cache_page = page, .position = _current_avaible_pos});
            ret = _current_avaible_pos;

            if (evict_tile.is_valid())
            {
                _current_avaible_pos = evict_tile.position;
            }
            else
            {
                _current_avaible_pos.x = (_current_avaible_pos.x + 1) % _resolution_in_tile;
                if (_current_avaible_pos.x == 0) _current_avaible_pos.y++;

                assert(_current_avaible_pos.y <= _resolution_in_tile);
            }
        }
        else 
        {
            // 将其提到 LruCache 最前面 (即更新其 least recently used time).
            _tiles.insert(key, *tile);
            ret = tile->position;
        }

        page->flag = VTPage::LoadFlag::Loaded;

        return ret;
    }


    std::string VTPhysicalTable::get_texture_name(uint32_t texture_type)
    {
        std::string ret;
        switch (texture_type) 
		{
		case Material::TextureType_BaseColor: 
			ret = "vt_physical_texture_base_color"; break;
		case Material::TextureType_Normal:  
			ret = "vt_physical_texture_normal";; break;
		case Material::TextureType_PBR:  
			ret = "vt_physical_texture_pbr";; break;
		case Material::TextureType_Emissive:  
			ret = "vt_physical_texture_emissive";; break;
		default: break;
		}
        return ret;
    }
}