#ifndef SCENE_VIRTUAL_Table_H
#define SCENE_VIRTUAL_Table_H

#include <cstdint>
#include "geometry.h"
#include <string>
#include "../core/tools/delegate.h"
#include "../core/math/vector.h"
#include "../core/tools/ecs.h"

namespace fantasy 
{
    const static uint32_t VT_PAGE_SIZE = 128u;
	const static uint32_t LOWEST_TEXTURE_RESOLUTION = 512u;
	const static uint32_t HIGHEST_TEXTURE_RESOLUTION = 4096u;
    static const uint32_t VT_PHYSICAL_TEXTURE_RESOLUTION = 4096u;
    static const uint32_t PHYSICAL_PAGE_NUM = (VT_PHYSICAL_TEXTURE_RESOLUTION / VT_PAGE_SIZE) * 
                                              (VT_PHYSICAL_TEXTURE_RESOLUTION / VT_PAGE_SIZE);

    static const uint32_t VIRTUAL_SHADOW_PAGE_SIZE = 1024;
    static const uint32_t VIRTUAL_SHADOW_RESOLUTION = 16384;
    static const uint32_t PHYSICAL_SHADOW_RESOLUTION = 8192;

    namespace event
	{
		DELCARE_DELEGATE_EVENT(GenerateMipmap, Entity*);
	};

    struct PhysicalTile
    {
        uint32_t geometry_id = INVALID_SIZE_32;
        uint32_t vt_page_id_mip_level = INVALID_SIZE_32;
        uint2 physical_postion = uint2(INVALID_SIZE_32);

        uint32_t previous = INVALID_SIZE_32;
        uint32_t next = INVALID_SIZE_32;
        uint32_t state;   // 0: 初始状态; 1: 被 update_tiles 或 new_tiles 捕捉.
    };

    struct PhysicalTileLruCache
    {
        PhysicalTile tiles[PHYSICAL_PAGE_NUM];
        
        uint32_t update_tiles[PHYSICAL_PAGE_NUM];
        uint32_t update_tile_count = 0;

        uint32_t new_tiles[PHYSICAL_PAGE_NUM];
        uint32_t new_tile_count = 0;

        uint32_t current_lru = PHYSICAL_PAGE_NUM - 1;
        uint32_t current_evict = PHYSICAL_PAGE_NUM - 1;


        PhysicalTileLruCache()
        {
            uint32_t row_page_num = VT_PHYSICAL_TEXTURE_RESOLUTION / VT_PAGE_SIZE;

            tiles[0].previous = INVALID_SIZE_32;
            for (uint32_t ix = 0; ix < PHYSICAL_PAGE_NUM - 1; ++ix)
            {
                tiles[ix].next = ix + 1;
                tiles[ix + 1].previous = ix;
                tiles[ix].physical_postion = uint2(ix % row_page_num, ix / row_page_num);

                update_tiles[ix] = INVALID_SIZE_32;
                new_tiles[ix] = INVALID_SIZE_32;
            }
            tiles[PHYSICAL_PAGE_NUM - 1].next = INVALID_SIZE_32;
            tiles[PHYSICAL_PAGE_NUM - 1].physical_postion = uint2(row_page_num - 1, row_page_num - 1);

            update_tiles[PHYSICAL_PAGE_NUM - 1] = INVALID_SIZE_32;
            new_tiles[PHYSICAL_PAGE_NUM - 1] = INVALID_SIZE_32;
        }

        void update(const PhysicalTileLruCache* other)
        {
            for (uint32_t ix = 0; ix < PHYSICAL_PAGE_NUM; ++ix) tiles[ix] = other->tiles[ix];

            current_evict = tiles[other->new_tiles[other->new_tile_count - 1]].previous;
            current_lru = other->current_lru;

            auto func_update_tiles = [this](uint32_t count, const uint32_t* update_tiles)
            {
                for (uint32_t ix = 0; ix < count; ++ix)
                {
                    uint32_t old_lru_index = current_lru;
                    current_lru = update_tiles[ix];
    
                    auto& old_lru_tile = tiles[old_lru_index];
                    auto& new_lru_tile = tiles[current_lru];
    
                    if (new_lru_tile.previous == INVALID_SIZE_32) continue;
    
                    tiles[new_lru_tile.previous].next = new_lru_tile.next;
                    if (new_lru_tile.next != INVALID_SIZE_32) tiles[new_lru_tile.next].previous = new_lru_tile.previous;
    
                    old_lru_tile.previous = current_lru;
                    new_lru_tile.previous = INVALID_SIZE_32;
                    new_lru_tile.next = old_lru_index;
                }
            };

            func_update_tiles(other->new_tile_count, other->new_tiles);
            func_update_tiles(other->update_tile_count, other->update_tiles);
        }
    };

    inline std::string get_vt_physical_texture_name(uint32_t texture_type)
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



#endif