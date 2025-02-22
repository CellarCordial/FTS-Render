#ifndef SHADER_COMMON_VIRTUAL_TEXTURE_HLSL
#define SHADER_COMMON_VIRTUAL_TEXTURE_HLSL

#include "../common/math.hlsl"

#define VT_PAGE_SIZE 1
#define VT_PHYSICAL_TEXTURE_RESOLUTION 1

#if defined(VT_PAGE_SIZE) && defined(VT_PHYSICAL_TEXTURE_RESOLUTION)

static uint page_num = (VT_PHYSICAL_TEXTURE_RESOLUTION / VT_PAGE_SIZE) * (VT_PHYSICAL_TEXTURE_RESOLUTION / VT_PAGE_SIZE);

struct PhysicalTile
{
    uint geometry_id;
    uint vt_page_id_mip_level;
    uint2 physical_postion;

    uint previous;
    uint next;
};

struct PhysicalTileLruCache
{
    PhysicalTile tiles[page_num];
    
    uint update_tiles[page_num];
    uint update_tile_count;

    PhysicalTile new_tiles[page_num];
    uint new_tile_count;

    uint current_lru;
    uint current_evict;

    static PhysicalTileLruCache construct()
    {
        PhysicalTileLruCache ret;
        ret.current_lru = 0;
        ret.current_evict = 0;
        uint size = page_num;
        for (uint ix = 0; ix < size; ++ix)
        {
            ret.tiles[ix].physical_postion = uint2(INVALID_SIZE_32, INVALID_SIZE_32);
        }
        return ret;
    }

    void insert(PhysicalTile tile)
    {
        bool found = false;
        uint index = current_lru;
        for (; index != current_evict; index = tiles[index].next)
        {
            PhysicalTile tmp = tiles[index];
            if (tmp.geometry_id == tile.geometry_id && 
                tmp.vt_page_id_mip_level == tile.vt_page_id_mip_level)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            uint current_pos;
            InterlockedAdd(new_tile_count, 1, current_pos);
            new_tiles[current_pos] = tile;
        }
        else
        {
            uint current_pos;
            InterlockedAdd(update_tile_count, 1, current_pos);
            update_tiles[current_pos] = index;
        }
    }
};




#endif

#endif