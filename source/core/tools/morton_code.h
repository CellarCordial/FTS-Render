#ifndef CORE_TOOLS_MORTON_CODE_H
#define CORE_TOOLS_MORTON_CODE_H
#include <cstdint>

#include "../math/vector.h"

namespace fantasy 
{
    static uint32_t MortonCode2(uint32_t x)
    {
        x &= 0x0000ffff;
        x = (x ^ (x << 8)) & 0x00ff00ff;
        x = (x ^ (x << 4)) & 0x0f0f0f0f;
        x = (x ^ (x << 2)) & 0x33333333;
        x = (x ^ (x << 1)) & 0x55555555;
        return x;
    }

    static uint32_t MortonEncode(uint32_t x,uint32_t y)
    {
        uint32_t Morton = MortonCode2(x) | (MortonCode2(y) << 1);
        return Morton;
    }

    static uint32_t MortonEncode(uint2 value)
    {
        return MortonEncode(value.x, value.y);
    }

    static uint32_t ReverseMortonCode2(uint32_t x)
    {
        x &= 0x55555555;
        x = (x ^ (x >> 1)) & 0x33333333;
        x = (x ^ (x >> 2)) & 0x0f0f0f0f;
        x = (x ^ (x >> 4)) & 0x00ff00ff;
        x = (x ^ (x >> 8)) & 0x0000ffff;
        return x;
    }

    static uint2 MortonDecode(uint32_t Morton)
    {
        uint2 ret;
        ret.x = ReverseMortonCode2(Morton);
        ret.y = ReverseMortonCode2(Morton >> 1);
        return ret;
    }

}





#endif