#include "virtual_texture.h"
#include "../core/tools/morton_code.h"
#include "../core/parallel/parallel.h"
#include <cstdint>
#include <memory>

#include "geometry.h"
#include "scene.h"

namespace fantasy 
{
    bool Mipmap::initialize(uint32_t mip_levels, uint32_t mip0_resolution)
    {
        ReturnIfFalse(is_power_of_2(mip0_resolution));

        _quad_tree.initialize(mip0_resolution / page_size, mip_levels);

        _mips.resize(mip_levels);
        for (uint32_t ix = 0; ix < mip_levels; ++ix)
        {
            auto& mip = _mips[ix];
            
            mip.resolution = ix == 0 ? mip0_resolution : _mips[ix - 1].resolution >> 2;
            
            uint32_t resolution_in_page = mip.resolution / page_size;
            mip.pages.resize(resolution_in_page * resolution_in_page);
            
            parallel::parallel_for(
                [&](uint64_t x, uint64_t y)
                {
                    uint32_t morton_code = MortonEncode(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
                    auto& page = mip.pages[morton_code];
                    page.mip_level = ix;
                    
                    uint2 lower = uint2(x * page_size, y * page_size);
                    page.bounds = Bounds2I(lower, lower + page_size);
                }, 
                resolution_in_page, 
                resolution_in_page
            );
        }
        return true;
    }

    VTPage* Mipmap::query_page(uint2 uv, uint32_t mip_level)
    {
        uint2 page_id = uv / page_size;
        uint32_t morton_code = _quad_tree.query_page_morton_code(page_id, mip_level);
        return &_mips[mip_level].pages[morton_code];
    }
    
    bool Mipmap::QuadTree::initialize(uint32_t mip0_resolution_in_page, uint32_t mip_levels)
    {
        ReturnIfFalse(is_power_of_2(mip0_resolution_in_page));

        const auto func_recursive_build_tree = [&](std::unique_ptr<Node>& node, Node* parent, uint32_t child_index) 
        {
            auto build_tree_impl = [&](std::unique_ptr<Node>& node, uint32_t child_index, Node* parent, auto& build_tree_impl_ref) mutable 
            {
                if (parent->mip_level == 0) return;

                node->parent_node = parent;
                node->mip_level = parent->mip_level - 1;

                // child_index: 0 ~ 3.
                uint2 offset = uint2(child_index & 0x01, (child_index & 0x02) >> 1);

                uint2 morton = MortonDecode(parent->morton_code) * 2 + offset;
                node->morton_code = MortonEncode(morton);

                uint32_t resolution = parent->virtual_bounds.width() / 2;
                uint2 lower = parent->virtual_bounds._lower + offset * resolution;
                node->virtual_bounds = Bounds2I(lower, lower + resolution);

                for (uint32_t ix = 0; ix < node->children.size(); ++ix)
                {
                    build_tree_impl_ref(node->children[ix], ix, node.get(), build_tree_impl_ref);
                }
            };
            return build_tree_impl(node, child_index, parent, build_tree_impl);
        };

        _root = std::make_unique<Node>();
        _root->morton_code = 0;
        _root->mip_level = mip_levels - 1;
        _root->virtual_bounds = Bounds2I(uint2(0u), uint2(mip0_resolution_in_page));
        for (uint32_t ix = 0; ix < _root->children.size(); ++ix)
        {
            func_recursive_build_tree(_root->children[ix], _root.get(), ix);
        }

        return true;
    }


    uint32_t Mipmap::QuadTree::query_page_morton_code(uint2 page_id, uint32_t mip_level)
    {
        uint32_t ret = INVALID_SIZE_32;
        Node* current_node = _root.get();
        while (current_node != nullptr)
        {
            if (mip_level != current_node->mip_level)
            {
                for (uint32_t ix = 0; ix < current_node->children.size(); ++ix)
                {
                    // 不包含右边界和下边界
                    if (inside_exclusive(page_id, current_node->children[ix]->virtual_bounds))
                    {
                        current_node = current_node->children[ix].get();
                        break;
                    }
                }
            }
            else
            {
                ret = current_node->morton_code;
                break;
            }
        }
        return ret;
    }

    bool SceneSystem::publish(World* world, const event::OnComponentAssigned<Mipmap>& event)
    {
		Mipmap* mipmap = event.component;
		Material* material = event.entity->get_component<Material>();

        uint32_t mip_levels = std::log2( material->image_resolution / page_size) + 1;
		return mipmap->initialize(mip_levels, material->image_resolution);
    }

}