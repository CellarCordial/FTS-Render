#ifndef SHADER_COMMON_VIRTUAL_TEXTURE_HLSL
#define SHADER_COMMON_VIRTUAL_TEXTURE_HLSL

#include "../common/math.hlsl"

#define VT_PAGE_SIZE 1
#define VT_PHYSICAL_TEXTURE_RESOLUTION 1

#if defined(VT_PAGE_SIZE) && defined(VT_PHYSICAL_TEXTURE_RESOLUTION)

static const uint page_num = (VT_PHYSICAL_TEXTURE_RESOLUTION / VT_PAGE_SIZE) * (VT_PHYSICAL_TEXTURE_RESOLUTION / VT_PAGE_SIZE);

struct PhysicalTile
{
    uint geometry_id;
    uint vt_page_id_mip_level;
    uint2 physical_postion;

    uint previous;
    uint next;
    uint state;   // 0: 初始状态; 1: 被 update_tiles 或 new_tiles 捕捉.
};

// PhysicalTileLruCache 在初始状态下, tiles 就是一个双向链表, 头节点的 previous 和 尾节点的 next 都为 INVALID_SIZE_32, 
// current_lru 和 current_evict 都指向末尾节点. 使用 insert() 进行插入时, 首先遍历这一帧所添加的 new_tiles, 若缓存命中, 则直接返回.
// 反之继续在 tiles 中从 current_lru 向 current_evict 中寻找该插入 tile 是否已添加到 physical texture 中.
// 若没有添加, 则更新 new_tile_count, 再在 tiles 中从 current_evict 向 current_lru 寻找目标更新 tile 进行更新, 
// 并将 tile 缓存至 new_tiles. 若已添加, 则更新 update_tile_count, 并将该 tile 在 tiles 中的 index 缓存至 update_tiles 中.
// 在 virtual_texture_update pass 中可以根据 new_tiles 进行纹理复制.
// 在该帧结束时, 可以根据 update_tiles, new_tiles 对 PhysicalTileLruCache 进行更新.

struct PhysicalTileLruCache
{
    PhysicalTile tiles[page_num];
    
    uint update_tiles[page_num];
    uint update_tile_count;

    uint new_tiles[page_num];
    uint new_tile_count;

    uint current_lru;
    uint current_evict;


    uint2 insert(uint geometry_id, uint vt_page_id_mip_level)
    {
        bool found = false;
        uint index = 0;
        for (; index < new_tile_count; ++index)
        {
            if (tiles[new_tiles[index]].geometry_id == geometry_id && 
                tiles[new_tiles[index]].vt_page_id_mip_level == vt_page_id_mip_level)
            {
                found = true;
                break;
            }
        }
        if (found) return tiles[new_tiles[index]].physical_postion;

        index = current_lru;
        for (; index != current_evict; index = tiles[index].next)
        {
            PhysicalTile tmp = tiles[index];
            if (tmp.geometry_id == geometry_id && 
                tmp.vt_page_id_mip_level == vt_page_id_mip_level)
            {
                uint state;
                InterlockedExchange(tiles[index].state, 1, state);
                if (state == 0) { found = true; break; }
            }
        }

        if (!found)
        {
            uint old_tile_count;
            InterlockedAdd(new_tile_count, 1, old_tile_count);

            uint current_pos = current_evict;
            while (old_tile_count > 0) current_pos = tiles[current_pos].previous;
            
            while (true)
            {
                uint state;
                InterlockedExchange(tiles[current_pos].state, 1, state);
                if (state == 0) break;
                current_pos = tiles[current_pos].previous;
            }

            tiles[current_pos].geometry_id = geometry_id;
            tiles[current_pos].vt_page_id_mip_level = vt_page_id_mip_level;

            new_tiles[old_tile_count] = current_pos;
            return tiles[current_pos].physical_postion;
        }
        else
        {
            uint current_pos;
            InterlockedAdd(update_tile_count, 1, current_pos);
            update_tiles[current_pos] = index;
            return tiles[index].physical_postion;
        }
    }
};




#endif

#endif