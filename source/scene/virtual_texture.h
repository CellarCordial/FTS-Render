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
    const static uint32_t vt_page_size = 128u;
	const static uint32_t lowest_texture_resolution = 512u;
	const static uint32_t highest_texture_resolution = 4096u;
    static const uint32_t vt_physical_texture_resolution = 4096u;

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

        Bounds2I bounds;
        uint32_t mip_level = INVALID_SIZE_32;
        
        LoadFlag flag = LoadFlag::Unload;

        bool always_in_cache = false;
    };

    struct VTPageInfo
    {
        uint32_t geometry_id;
        uint32_t page_id_mip_level;
    };

    class MipmapLUT
    {
    public:
        bool initialize(uint32_t mip0_resolution, uint32_t max_mip_resolution = vt_page_size);
        VTPage* query_page(uint2 page_id, uint32_t mip_level);
        uint32_t get_mip0_resolution() const { return _mip0_resolution; }
        uint32_t get_mip_levels() const { return static_cast<uint32_t>(_mips.size()); }

    private:
        struct Mip
        {
            uint32_t resolution = 0;
            std::vector<VTPage> pages;  // 以 morton code 排序.
        };

        class QuadTree
        {
        public:
            QuadTree() = default;
            QuadTree(const QuadTree& other);
            QuadTree& operator=(const QuadTree& other);

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

                Node() = default;
                Node(const Node& other);
                Node& operator=(const Node& other);
                bool is_leaf() const { return children[0] == nullptr; }
            };

            std::unique_ptr<Node> _root;
        };

        uint32_t _mip0_resolution = 0;

        QuadTree _quad_tree;
        std::vector<Mip> _mips;
    };

    class VTIndirectTable
    {
    public:
        VTIndirectTable() : 
            page_pointers(CLIENT_WIDTH * CLIENT_HEIGHT, uint2(INVALID_SIZE_32)),
            resolution(CLIENT_WIDTH, CLIENT_HEIGHT) 
        {
        }

        void set_page(uint2 page_id, uint2 page_pos)
        {
            assert(uv < resolution);
            page_pointers[page_id.y * resolution.x + page_id.x] = page_pos;
        }

        void set_page_null(uint2 page_id) { set_page(page_id, uint2(INVALID_SIZE_32)); }
        uint2* get_data() { return page_pointers.data(); }

    private:
        std::vector<uint2> page_pointers;
        std::string texture_name;
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
        };

        static void on_page_evict(Tile& tile)
        {
            tile.cache_page->flag = VTPage::LoadFlag::Unload;
        }

    public:
        VTPhysicalTable() : _tiles(_resolution_in_tile * _resolution_in_tile, on_page_evict) {}

        uint2 add_page(VTPage* page);
        static std::string get_texture_name(uint32_t texture_type);

    private:
        uint32_t _resolution = vt_physical_texture_resolution;
        uint32_t _resolution_in_tile = vt_physical_texture_resolution / vt_page_size;

        LruCache<Tile> _tiles;
        uint2 _current_avaible_pos = uint2(0u);
    };
}



#endif