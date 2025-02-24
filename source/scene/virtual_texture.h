#ifndef SCENE_VIRTUAL_Table_H
#define SCENE_VIRTUAL_Table_H

#include <cstdint>
#include <span>
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
	const static uint32_t HIGHEST_TEXTURE_RESOLUTION = 4096;
    static const uint32_t VT_PHYSICAL_TEXTURE_RESOLUTION = 4096;

    static const uint32_t VIRTUAL_SHADOW_PAGE_SIZE = 1024;
    static const uint32_t VIRTUAL_SHADOW_RESOLUTION = 16384;
    static const uint32_t PHYSICAL_SHADOW_RESOLUTION = 8192;

    namespace event
	{
		DELCARE_DELEGATE_EVENT(GenerateMipmap, Entity*);
	};

    struct VTPage
    {
        uint32_t geometry_id = INVALID_SIZE_32;
        uint32_t coordinate_mip_level = INVALID_SIZE_32;
        uint2 physical_position_in_page;

        uint2 get_coordinate_in_page() const 
        {
            uint32_t coordinate = coordinate_mip_level >> 8;
            return uint2(coordinate >> 12 , coordinate & 0xfff);
        }

        uint32_t get_mip_level() const
        {
            return coordinate_mip_level & 0xff;
        }
    };

    class VTIndirectTable
    {
    public:
        void initialize(uint32_t width, uint32_t height);

        void set_page(uint2 pixel_id, uint2 physical_pos_in_page);
        void set_page_null(uint2 pixel_i);

        uint2* get_data();
        uint64_t get_data_size() const;

    private:
        std::vector<uint2> physical_page_pointers;
    };

    class VTPhysicalTable
    {
    public:
        VTPhysicalTable(uint32_t resolution = VT_PHYSICAL_TEXTURE_RESOLUTION);

        bool check_page_loaded(VTPage& page) const;
        uint2 get_new_position();

        void add_pages(std::span<VTPage> pages);
        void add_page(const VTPage& page);
        void reset();
        
        static std::string get_texture_name(uint32_t texture_type);
        static uint64_t create_page_key(VTPage page);

    private:
        uint32_t _resolution_in_page;
        LruCache<VTPage> _tiles;
    };
}



#endif