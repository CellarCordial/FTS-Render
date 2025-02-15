#ifndef SCENE_VIRTUAL_Table_H
#define SCENE_VIRTUAL_Table_H

#include <cstdint>
#include <vector>
#include <string>
#include "../core/tools/lru_cache.h"
#include "../core/tools/delegate.h"
#include "../core/math/vector.h"
#include "../core/tools/ecs.h"

namespace fantasy 
{
    const static uint32_t VT_PAGE_SIZE = 128u;
	const static uint32_t LOWEST_TEXTURE_RESOLUTION = 512u;
	const static uint32_t HIGHEST_TEXTURE_RESOLUTION = 4096u;
    static const uint32_t VT_PHYSICAL_TEXTURE_RESOLUTION = 4096u;

    static const uint32_t VIRTUAL_SHADOW_PAGE_SIZE = 1024;
    static const uint32_t VIRTUAL_SHADOW_RESOLUTION = 16000;
    static const uint32_t PHYSICAL_SHADOW_RESOLUTION = 8192;

    namespace event
	{
		DELCARE_DELEGATE_EVENT(GenerateMipmap, Entity*);
	};

    struct VTPage
    {
        enum class LoadFlag : uint8_t
        {
            Unload,
            Loaded
        };

        uint2 base_position;
        uint32_t mip_level = INVALID_SIZE_32;
        
        LoadFlag flag = LoadFlag::Unload;

        bool always_in_cache = false;
    };

    struct VTPageInfo
    {
        uint32_t geometry_id;
        uint32_t page_id_mip_level;

        void get_data( uint2& page_id, uint32_t& mip_level) const
        {
            page_id = uint2(
                ((page_id_mip_level >> 12) & 0xf << 8) | (page_id_mip_level >> 24) & 0xff,
                ((page_id_mip_level >> 8) & 0xf << 8) | (page_id_mip_level >> 16) & 0xff
            );
            mip_level = page_id_mip_level & 0xff;
        }
    };

    class MipmapLUT
    {
    public:
        bool initialize(
            uint32_t mip0_resolution, 
            uint32_t max_mip_resolution = VT_PAGE_SIZE
        );

        VTPage* query_page(uint2 page_id, uint32_t mip_level);
        uint32_t get_mip0_resolution() const { return _mip0_resolution; }
        uint32_t get_mip_levels() const { return static_cast<uint32_t>(_mips.size()); }

    private:
        struct Mip
        {
            uint32_t resolution = 0;
            std::vector<VTPage> pages;  // 以 morton code 排序.
        };

        uint32_t _mip0_resolution = 0;
        std::vector<Mip> _mips;
    };

    class VTIndirectTable
    {
    public:
        VTIndirectTable() : 
            physical_page_pointers(CLIENT_WIDTH * CLIENT_HEIGHT, uint2(INVALID_SIZE_32)),
            resolution(CLIENT_WIDTH, CLIENT_HEIGHT) 
        {
        }

        void set_page(uint2 page_id, uint2 page_pos_in_page)
        {
            physical_page_pointers[page_id.y * resolution.x + page_id.x] = page_pos_in_page;
        }

        void set_page_null(uint2 page_id) { set_page(page_id, uint2(INVALID_SIZE_32)); }
        uint2* get_data() { return physical_page_pointers.data(); }
        uint64_t get_data_size() const { return physical_page_pointers.size(); }

    private:
        std::vector<uint2> physical_page_pointers;
        uint2 resolution;
    };


    class VTPhysicalTable
    {
    public:
        // 一个 Tile 对应 一个 Page.
        struct Tile
        {
            VTPage* cache_page = nullptr;
            uint2 position = uint2(INVALID_SIZE_32);

            bool is_valid() const { return cache_page != nullptr && position != uint2(INVALID_SIZE_32); }
        };

    private:
        static void on_page_evict(Tile& tile)
        {
            tile.cache_page->flag = VTPage::LoadFlag::Unload;
        }

    public:
        VTPhysicalTable(uint32_t resolution = VT_PHYSICAL_TEXTURE_RESOLUTION, uint32_t tile_size = VT_PAGE_SIZE) : 
            _resolution(resolution),
            _resolution_in_tile(resolution / tile_size),
            _tiles((resolution / tile_size) * (resolution / tile_size), on_page_evict) 
        {
        }

        uint2 add_page(VTPage* page);
        static std::string get_texture_name(uint32_t texture_type);
        void reset() { _tiles.reset(); _current_avaible_pos = uint2(0u); }

    private:
        uint32_t _resolution;
        uint32_t _resolution_in_tile;

        LruCache<Tile> _tiles;
        uint2 _current_avaible_pos = uint2(0u);
    };
}



#endif