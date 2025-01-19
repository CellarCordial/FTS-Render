#ifndef SCENE_VIRTUAL_Table_H
#define SCENE_VIRTUAL_Table_H

#include <cstdint>
#include <vector>
#include <string>
#include "../core/tools/lru_cache.h"
#include "../core/tools/delegate.h"
#include "../core/math/bounds.h"
#include "../core/tools/ecs.h"

namespace fantasy 
{
    const static uint32_t page_size = 128u;
    static const uint32_t physical_texture_slice_size = 1024u;
    static const uint32_t physical_texture_resolution = 4096u;

    namespace event
	{
		DELCARE_DELEGATE_EVENT(GenerateMipmap, Entity*);
	};

    struct VTPage
    {
        enum class LoadFlag : uint8_t
        {
            None,
            Loading,
            Loaded
        };

        Bounds2I bounds;
        uint32_t mip_level = INVALID_SIZE_32;
        
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
        bool initialize(uint32_t mip_levels, uint32_t mip0_resolution);
        VTPage* query_page(uint2 uv, uint32_t mip_level);

    private:
        struct Mip
        {
            uint32_t resolution = 0;
            std::vector<VTPage> pages;  // 以 morton code 排序.
        };

        class QuadTree
        {
        public:
            bool initialize(uint32_t mip0_resolution_in_page, uint32_t mip_levels);
            uint32_t query_page_morton_code(uint2 page_id, uint32_t mip_level);

        private:
            // 每个 Node 代表一个 Page.
            struct Node
            {
                std::array<std::unique_ptr<Node>, 4> children;
                Node* parent_node = nullptr;
                
                Bounds2I virtual_bounds;    // 代表该 node 实际映射 virtual texture 的部分 (单位是 page).
                uint32_t mip_level = INVALID_SIZE_32;
                uint32_t morton_code = INVALID_SIZE_32;

                bool is_leaf() const { return children[0] == nullptr; }
            };

            std::unique_ptr<Node> _root;
        };

        QuadTree _quad_tree;
        std::vector<Mip> _mips;
    };

    class VTIndirectTexture
    {
    public:
        VTIndirectTexture() : 
            page_pointers(CLIENT_WIDTH * CLIENT_HEIGHT, uint2(INVALID_SIZE_32)),
            resolution(CLIENT_WIDTH, CLIENT_HEIGHT) 
        {
        }

        void set_page(uint2 uv, uint2 page_pos)
        {
            assert(uv < resolution);
            page_pointers[uv.y * resolution.x + uv.x] = page_pos;
        }

        void set_page_null(uint2 uv) { set_page(uv, uint2(INVALID_SIZE_32)); }

        uint2* get_data() { return page_pointers.data(); }

    private:
        std::vector<uint2> page_pointers;
        std::string texture_name;
        uint2 resolution;
    };

    class VTPhysicalTexture
    {
    public:
        VTPhysicalTexture() : _tiles(_resolution_in_tile * _resolution_in_tile)
        {
        }

        uint2 add_page(VTPage* page);

        static std::string get_slice_name(uint32_t texture_type, uint2 uv);

    private:
        uint32_t _resolution = physical_texture_resolution;
        uint32_t _resolution_in_tile = physical_texture_resolution / page_size;

        // 一个 Tile 对应 一个 Page.
        struct Tile
        {
            VTPage* cache_page = nullptr;
            uint2 position = uint2(INVALID_SIZE_32);
        };

        LruCache<Tile> _tiles;
    };
}



#endif