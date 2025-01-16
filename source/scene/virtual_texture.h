#ifndef SCENE_VIRTUAL_Table_H
#define SCENE_VIRTUAL_Table_H

#include <cstdint>
#include <vector>
#include <string>
#include "../core/math/rectangle.h"
#include "../core/tools/lru_cache.h"
#include "geometry.h"

namespace fantasy 
{
    const static uint32_t page_size = 128u;
    static const uint32_t physical_texture_slice_size = 1024u;
    static const uint32_t physical_texture_resolution = 4096u;

    struct VTPage
    {
        enum class LoadFlag : uint8_t
        {
            None,
            Loading,
            Loaded
        };

        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t morton_id = 0;

        uint32_t mip = 0;
        uint32_t mip_mask = 0; 
        
        LoadFlag flag = LoadFlag::None;
        bool always_in_cache = false;
    };

    struct VTPageInfo
    {
        uint32_t geometry_id;
        uint32_t page_id_mip_level;
    };


    class Mipmap
    {
    public:
        Mipmap(uint32_t mip_level, uint32_t mip0_size_in_page, uint32_t page_size);

        VTPage GetPage(uint32_t morton) const;
        VTPage GetPage(uint32_t x, uint32_t y) const;

    private:
        uint32_t _mip_level;
        uint32_t _size_in_page; // 单位为 page.
        uint32_t _real_resolution;
        std::vector<VTPage> _pages;
    };

    class VTIndirectTexture
    {

    };

    class VTPhysicalTexture
    {
    public:
        struct Tile
        {
            Rectangle rect;
            VTPage* cache_page;
        };

    private:
        uint32_t _real_width;
        uint32_t _real_height;

        uint32_t _tile_count_x;
        uint32_t _tile_count_y;
        
        LruCache<Tile> _tiles;

        std::array<std::string, Material::TextureType_Num> _physical_textures;
    };
}



#endif