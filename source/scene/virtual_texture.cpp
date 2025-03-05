#include "virtual_texture.h"
#include <cstdint>

#include "geometry.h"

namespace fantasy 
{
    // {
    //     // mip 0 的 node_size_in_page 为 1, 逐级往上指数递增.
    //     uint32_t mip_node_size_in_page = std::pow(2, mip_level);
    //     return &_mips[mip_level].pages[morton_encode(page_id / mip_node_size_in_page)];
    // }

    void VTIndirectTable::initialize(uint32_t width, uint32_t height)
    {
        _resolution = { width, height };
        physical_page_pointers.resize(width * height, uint2(INVALID_SIZE_32));
    }

    void VTIndirectTable::set_page(uint2 pixel_id, uint2 physical_pos_in_page)
    {
        physical_page_pointers[pixel_id.y * _resolution.x + pixel_id.x] = physical_pos_in_page;
    }
    
    void VTIndirectTable::set_page(uint32_t pixel_index, uint2 physical_pos_in_page)
    {
        physical_page_pointers[pixel_index] = physical_pos_in_page;
    }

    void VTIndirectTable::set_page_null(uint2 pixel_id) 
    { 
        set_page(pixel_id, uint2(INVALID_SIZE_32)); 
    }

    void VTIndirectTable::reset()
    {
        std::fill(physical_page_pointers.begin(), physical_page_pointers.end(), uint2(INVALID_SIZE_32));
    }

    uint2* VTIndirectTable::get_data() 
    { 
        return physical_page_pointers.data(); 
    }

    uint64_t VTIndirectTable::get_data_size() const 
    { 
        return physical_page_pointers.size() * sizeof(uint2); 
    }

    VTPhysicalTable::VTPhysicalTable(uint32_t resolution, uint32_t page_size) : 
        _resolution_in_page(resolution / page_size),
        _pages(_resolution_in_page * _resolution_in_page)
    {
        assert(is_power_of_2(_resolution_in_page));
        reset();
    }

    bool VTPhysicalTable::check_page_loaded(VTPage& page)
    {
        return _pages.check_cache(create_page_key(page), page);
    }

    uint2 VTPhysicalTable::get_new_position()
    {
        return _pages.evict().physical_position_in_page;
    }

    void VTPhysicalTable::add_page(const VTPage& page)
    {
        _pages.insert(create_page_key(page), page);
    }

    void VTPhysicalTable::add_pages(std::span<VTPage> pages)
    {
        for (const auto& page : pages)
        {
            _pages.insert(create_page_key(page), page);
        }
    }

    void VTPhysicalTable::reset() 
    { 
        _pages.reset(); 

        uint32_t page_num = _resolution_in_page * _resolution_in_page;
        for (uint32_t ix = 0; ix < page_num; ++ix)
        {
            uint2 postion(ix % _resolution_in_page, ix / _resolution_in_page); 
            
            VTPage page;
            page.physical_position_in_page = postion;
            page.coordinate_mip_level = (postion.x << 20) | (postion.y << 8) | 0xff;    // 使每个的键值均不同.

            _pages.insert(create_page_key(page), page);
        }
    }

    uint64_t VTPhysicalTable::create_page_key(const VTPage& page)
    {
        return (uint64_t(page.geometry_id) << 32) | page.coordinate_mip_level;
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


    VTPhysicalShadowTable::VTPhysicalShadowTable(uint32_t resolution, uint32_t page_size) : 
        _resolution_in_page(resolution / page_size),
        _pages(_resolution_in_page * _resolution_in_page)
    {
        assert(is_power_of_2(_resolution_in_page));
        reset();
    }

    bool VTPhysicalShadowTable::check_page_loaded(VTShadowPage& page)
    {
        return _pages.check_cache(create_tile_key(page), page);
    }

    uint2 VTPhysicalShadowTable::get_new_position()
    {
        return _pages.evict().physical_position_in_page;
    }

    void VTPhysicalShadowTable::add_pages(std::span<VTShadowPage> pages)
    {
        for (const auto& page : pages)
        {
            _pages.insert(create_tile_key(page), page);
        }
    }

    void VTPhysicalShadowTable::add_page(const VTShadowPage& page_id)
    {
        _pages.insert(create_tile_key(page_id), page_id);
    }

    void VTPhysicalShadowTable::reset()
    {
        _pages.reset(); 

        uint32_t page_num = _resolution_in_page * _resolution_in_page;
        for (uint32_t ix = 0; ix < page_num; ++ix)
        {
            uint2 pos(ix % _resolution_in_page, ix / _resolution_in_page); 
            uint32_t axis_shadow_page_num = VT_VIRTUAL_SHADOW_RESOLUTION / VT_SHADOW_PAGE_SIZE;

            VTShadowPage page;
            page.physical_position_in_page = pos;
            page.tile_id = pos + axis_shadow_page_num;  // 偏移后, 初始化数据将不会是任何有效的 virtual shadow page.
            
            _pages.insert(create_tile_key(page), page);
        }
    }

    uint64_t VTPhysicalShadowTable::create_tile_key(const VTShadowPage& page)
    {
        return (uint64_t(page.tile_id.x) << 32) | uint64_t(page.tile_id.y);
    }
    
}