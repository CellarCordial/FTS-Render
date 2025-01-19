#ifndef CORE_TOOLS_QUAD_TREE_H
#define CORE_TOOLS_QUAD_TREE_H


#include <cstdint>
#include <memory>
#include <vector>
#include <cassert>
#include "../math/bounds.h"
#include "../math/vector.h"


namespace fantasy 
{
    template<typename T>
    class Quadtree
    {
    public:
        Quadtree(const Bounds2F& rect) :
            _bottom_rect(rect), _root(std::make_unique<Node>())
        {
        }

        Quadtree(const Bounds2F& bottom_rect, const Bounds2F& top_rect) :
            _bottom_rect(bottom_rect), _top_rect(top_rect)
        {
            
        }

        void add(const T& value)
        {
            add(_root.get(), 0, _bottom_rect, value);
        }

        void remove(const T& value)
        {
            remove(_root.get(), _bottom_rect, value);
        }

        std::vector<T> query(const Bounds2F& rect) const
        {
            auto values = std::vector<T>();
            query(_root.get(), _bottom_rect, rect, values);
            return values;
        }

        std::vector<std::pair<T, T>> findAllIntersections() const
        {
            auto intersections = std::vector<std::pair<T, T>>();
            findAllIntersections(_root.get(), intersections);
            return intersections;
        }

    private:
        static constexpr auto Threshold = std::size_t(16);
        static constexpr auto MaxDepth = std::size_t(8);

        struct Node
        {
            std::array<std::unique_ptr<Node>, 4> children;
            std::vector<T> value;
        };

        Bounds2F _top_rect;
        Bounds2F _bottom_rect;
        std::unique_ptr<Node> _root;

        bool is_leaf(const Node* node) const
        {
            return node->children[0] == nullptr;
        }

        Bounds2F compute_child_bounds(const Bounds2F& rect, uint32_t ix) const
        {
            float2 origin = rect._lower;
            float2 child_size = float2(rect.width(), rect.height()) / 2.0f;
            switch (ix)
            {
            case 0: // Top Left
                return Bounds2F(origin, child_size);
            
            case 1: // Top Right
                return Bounds2F(float2(origin.x + child_size.x, origin.y), child_size);
            
            case 2: // Bottom Left
                return Bounds2F(float2(origin.x, origin.y + child_size.y), child_size);
            
            case 3: // Bottom Right
                return Bounds2F(origin + child_size, child_size);
            default:
                assert(false && "Invalid child index");
                return Bounds2F();
            }
        }

        int getQuadrant(const Bounds2F& bounds, const Bounds2F& valueBox) const
        {
            auto center = bounds.center();
            // West
            if (valueBox._upper.x < center.x)
            {
                // North West
                if (valueBox._upper.y < center.y)
                    return 0;
                // South West
                else if (valueBox._lower.y >= center.y)
                    return 2;
                // Not contained in any quadrant
                else
                    return -1;
            }
            // East
            else if (valueBox._lower.x >= center.x)
            {
                // North East
                if (valueBox._upper.y < center.y)
                    return 1;
                // South East
                else if (valueBox._lower.y >= center.y)
                    return 3;
                // Not contained in any quadrant
                else
                    return -1;
            }
            // Not contained in any quadrant
            else
                return -1;
        }

        void add(Node* node, std::size_t depth, const Bounds2F& rect, const T& value)
        {
            assert(node != nullptr);
            assert(rect.contain(mGetBox(value)));
            if (is_leaf(node))
            {
                // Insert the value in this node if possible
                if (depth >= MaxDepth || node->value.size() < Threshold)
                    node->value.push_back(value);
                // Otherwise, we split and we try again
                else
                {
                    split(node, rect);
                    add(node, depth, rect, value);
                }
            }
            else
            {
                auto i = getQuadrant(rect, mGetBox(value));
                // Add the value in a child if the value is entirely contained in it
                if (i != -1)
                    add(node->children[static_cast<std::size_t>(i)].get(), depth + 1, compute_child_bounds(rect, i), value);
                // Otherwise, we add the value in the current node
                else
                    node->value.push_back(value);
            }
        }

        void split(Node* node, const Bounds2F& rect)
        {
            assert(node != nullptr);
            assert(isLeaf(node) && "Only leaves can be split");
            // Create children
            for (auto& child : node->children)
                child = std::make_unique<Node>();
            // Assign values to children
            auto newValues = std::vector<T>(); // New values for this node
            for (const auto& value : node->value)
            {
                auto i = getQuadrant(rect,  (value));
                if (i != -1)
                    node->children[static_cast<std::size_t>(i)]->values.push_back(value);
                else
                    newValues.push_back(value);
            }
            node->value = std::move(newValues);
        }

        bool remove(Node* node, const Bounds2F& rect, const T& value)
        {
            assert(node != nullptr);
            assert(rect.contains(mGetBox(value)));
            if (is_leaf(node))
            {
                // Remove the value from node
                removeValue(node, value);
                return true;
            }
            else
            {
                // Remove the value in a child if the value is entirely contained in it
                auto i = getQuadrant(rect, mGetBox(value));
                if (i != -1)
                {
                    if (remove(node->children[static_cast<std::size_t>(i)].get(), compute_child_bounds(rect, i), value))
                        return tryMerge(node);
                }
                // Otherwise, we remove the value from the current node
                else
                    removeValue(node, value);
                return false;
            }
        }

        void removeValue(Node* node, const T& value)
        {
            // Find the value in node->values
            auto it = std::find_if(std::begin(node->value), std::end(node->value),
                [this, &value](const auto& rhs){ return mEqual(value, rhs); });
            assert(it != std::end(node->values) && "Trying to remove a value that is not present in the node");
            // Swap with the last element and pop back
            *it = std::move(node->value.back());
            node->value.pop_back();
        }

        bool tryMerge(Node* node)
        {
            assert(node != nullptr);
            assert(!isLeaf(node) && "Only interior nodes can be merged");
            auto nbValues = node->value.size();
            for (const auto& child : node->children)
            {
                if (!is_leaf(child.get()))
                    return false;
                nbValues += child->values.size();
            }
            if (nbValues <= Threshold)
            {
                node->value.reserve(nbValues);
                // Merge the values of all the children
                for (const auto& child : node->children)
                {
                    for (const auto& value : child->values)
                        node->value.push_back(value);
                }
                // Remove the children
                for (auto& child : node->children)
                    child.reset();
                return true;
            }
            else
                return false;
        }

        void query(Node* node, const Bounds2F& rect, const Bounds2F& queryBox, std::vector<T>& values) const
        {
            assert(node != nullptr);
            assert(queryBox.intersects(rect));
            for (const auto& value : node->value)
            {
                if (intersect(queryBox, mGetBox(value)))
                    values.push_back(value);
            }
            if (!is_leaf(node))
            {
                for (auto i = std::size_t(0); i < node->children.size(); ++i)
                {
                    auto childBox = compute_child_bounds(rect, static_cast<int>(i));
                    if (intersect(queryBox, childBox))
                        query(node->children[i].get(), childBox, queryBox, values);
                }
            }
        }

        void findAllIntersections(Node* node, std::vector<std::pair<T, T>>& intersections) const
        {
            // Find intersections between values stored in this node
            // Make sure to not report the same intersection twice
            for (auto i = std::size_t(0); i < node->value.size(); ++i)
            {
                for (auto j = std::size_t(0); j < i; ++j)
                {
                    if (mGetBox(node->value[i]).intersects(mGetBox(node->value[j])))
                        intersections.emplace_back(node->value[i], node->value[j]);
                }
            }
            if (!is_leaf(node))
            {
                // Values in this node can intersect values in descendants
                for (const auto& child : node->children)
                {
                    for (const auto& value : node->value)
                        findIntersectionsInDescendants(child.get(), value, intersections);
                }
                // Find intersections in children
                for (const auto& child : node->children)
                    findAllIntersections(child.get(), intersections);
            }
        }

        void findIntersectionsInDescendants(Node* node, const T& value, std::vector<std::pair<T, T>>& intersections) const
        {
            // Test against the values stored in this node
            for (const auto& other : node->value)
            {
                if (mGetBox(value).intersects(mGetBox(other)))
                    intersections.emplace_back(value, other);
            }
            // Test against values stored into descendants of this node
            if (!is_leaf(node))
            {
                for (const auto& child : node->children)
                    findIntersectionsInDescendants(child.get(), value, intersections);
            }
        }
    };

}














#endif