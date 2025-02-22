#ifndef SHADER_COMMON_VIRTUAL_TEXTURE_HLSL
#define SHADER_COMMON_VIRTUAL_TEXTURE_HLSL

#include "../common/math.hlsl"

#define VT_PAGE_SIZE 1
#define VT_PHYSICAL_TEXTURE_RESOLUTION 1

#if defined(VT_PAGE_SIZE) && defined(VT_PHYSICAL_TEXTURE_RESOLUTION)

static uint row_page_num = VT_PHYSICAL_TEXTURE_RESOLUTION / VT_PAGE_SIZE;
static uint col_page_num = VT_PHYSICAL_TEXTURE_RESOLUTION / VT_PAGE_SIZE;

struct PhysicalTile
{
    uint geometry_id;
    uint vt_page_id_mip_level;
    uint2 physical_postion;
};

struct PhysicalTileLruCache
{
    PhysicalTile tiles[row_page_num * col_page_num];
    uint current_lru;
    uint next_lru;

    static PhysicalTileLruCache construct()
    {
        PhysicalTileLruCache ret;
        ret.current_lru = 0;
        ret.next_lru = 0;
        uint size = row_page_num * col_page_num;
        for (uint ix = 0; ix < size; ++ix)
        {
            ret.tiles[ix].physical_postion = uint2(INVALID_SIZE_32, INVALID_SIZE_32);
        }
        return ret;
    }

    PhysicalTile insert(PhysicalTile tile)
    {
        uint index = tile.physical_postion.y * row_page_num + tile.physical_postion.x;

        if (tiles[index].physical_postion == uint2(INVALID_SIZE_32, INVALID_SIZE_32))
        {
            tiles[index] = tile;
            return tile;
        }
        PhysicalTile ret = tiles[current_lru];
        return tile;
    }
};




#endif


#endif