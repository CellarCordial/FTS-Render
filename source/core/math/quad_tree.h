#ifndef CORE_MATH_QUAD_TREE_H
#define CORE_MATH_QUAD_TREE_H

#include "rectangle.h"
#include "vector.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace fantasy 
{
    struct QuadTree
    {
        uint32_t level;
        float3 mapping;
        Rectangle rectangle;
        std::vector<std::unique_ptr<QuadTree>> children;

    };

}


#endif