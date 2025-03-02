#ifndef SCENE_VIRTUAL_Table_H
#define SCENE_VIRTUAL_Table_H

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>
#include <string>
#include "../core/tools/lru_cache.h"
#include "../core/tools/delegate.h"
#include "../core/math/vector.h"
#include "../core/tools/ecs.h"

namespace fantasy 
{
    const static uint32_t VT_PAGE_SIZE = 128;
	const static uint32_t LOWEST_TEXTURE_RESOLUTION = 512;
	const static uint32_t HIGHEST_TEXTURE_RESOLUTION = 2048;
    static const uint32_t VT_PHYSICAL_TEXTURE_RESOLUTION = 8192;
    static const uint32_t VT_FEED_BACK_SCALE_FACTOR = 10;

    static const uint32_t VT_SHADOW_PAGE_SIZE = 1024;
    static const uint32_t VT_VIRTUAL_SHADOW_RESOLUTION = 16384;
    static const uint32_t VT_PHYSICAL_SHADOW_RESOLUTION = 8192;

    namespace event
	{
		DELCARE_DELEGATE_EVENT(GenerateMipmap, Entity*);
	};

    struct VirtualTexture
    {
        uint32_t geometry_id = INVALID_SIZE_32;
        uint32_t mip_level = INVALID_SIZE_32;
        uint32_t resolution_in_page = INVALID_SIZE_32;
        uint2 physical_position = uint2(INVALID_SIZE_32);

        bool operator==(const VirtualTexture& other) const 
        {
            return geometry_id == other.geometry_id &&
                   mip_level == other.mip_level &&
                   physical_position == other.physical_position;
        }

        bool operator!=(const VirtualTexture& other) const
        {
            return geometry_id != other.geometry_id ||
                   mip_level != other.mip_level ||
                   physical_position != other.physical_position;
        }
    };

    class VTIndirectTable
    {
    public:
        void initialize(uint32_t width, uint32_t height);

        void set_page(uint2 pixel_id, uint2 physical_pos_in_page);
        void set_page(uint32_t pixel_index, uint2 physical_pos_in_page);
        void set_page_null(uint2 pixel_i);

        void reset();

        uint2* get_data();
        uint64_t get_data_size() const;

    private:
        uint2 _resolution;
        std::vector<uint2> physical_page_pointers;
    };


    struct VTPage
    {
        uint32_t morton_code;

    };

    class VTPhysicalTable
    {
    public:
        VTPhysicalTable(uint32_t resolution = VT_PHYSICAL_TEXTURE_RESOLUTION, uint32_t page_size = VT_PAGE_SIZE);

        bool check_texture_loaded(VirtualTexture& texture) const;
        uint2 get_new_position(uint32_t resolution_in_page);

        void add_textures(std::span<VirtualTexture> textures);
        void add_texture(const VirtualTexture& texture);
        void reset();
        
        static std::string get_texture_name(uint32_t texture_type);
        static uint64_t create_texture_key(const VirtualTexture& page);

    private:
        uint32_t _resolution_in_page;
        MortonLruCache<VTPage> _pages;

        std::unordered_map<uint64_t, VirtualTexture> _textures;
    };


    struct VTShadowPage
    {
        uint2 tile_id;
        uint2 physical_position_in_page;

        bool operator==(const VTShadowPage& other) const 
        {
            return tile_id == other.tile_id &&
                   physical_position_in_page == other.physical_position_in_page;
        }

        bool operator!=(const VTShadowPage& other) const
        {
            return tile_id != other.tile_id ||
                   physical_position_in_page != other.physical_position_in_page;
        }
    };

    class VTPhysicalShadowTable
    {
    public:
        VTPhysicalShadowTable(
            uint32_t resolution = VT_PHYSICAL_SHADOW_RESOLUTION, 
            uint32_t page_size = VT_SHADOW_PAGE_SIZE
        );

        bool check_page_loaded(VTShadowPage& page) const;
        uint2 get_new_position();

        void add_pages(std::span<VTShadowPage> pages);
        void add_page(const VTShadowPage& page);
        void reset();

        static uint64_t create_tile_key(const VTShadowPage& page);

    private:
        uint32_t _resolution_in_page;
        LruCache<VTShadowPage> _pages;
    };
}



#endif