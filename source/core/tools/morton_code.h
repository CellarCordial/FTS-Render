#ifndef CORE_TOOLS_MORTON_CODE_H
#define CORE_TOOLS_MORTON_CODE_H
#include <cstdint>

namespace fantasy 
{
    static int32_t MortonCode2(int32_t x)
    {
        x &= 0x0000ffff;
        x = (x ^ (x << 8)) & 0x00ff00ff;
        x = (x ^ (x << 4)) & 0x0f0f0f0f;
        x = (x ^ (x << 2)) & 0x33333333;
        x = (x ^ (x << 1)) & 0x55555555;
        return x;
    }

    static int32_t MortonEncode(int32_t x,int32_t y)
    {
        int32_t Morton = MortonCode2(x) | (MortonCode2(y) << 1);
        return Morton;
    }
    static int32_t ReverseMortonCode2(int32_t x)
    {
        x &= 0x55555555;
        x = (x ^ (x >> 1)) & 0x33333333;
        x = (x ^ (x >> 2)) & 0x0f0f0f0f;
        x = (x ^ (x >> 4)) & 0x00ff00ff;
        x = (x ^ (x >> 8)) & 0x0000ffff;
        return x;
    }

    static void MortonDecode(int32_t Morton, int32_t& x, int32_t& y)
    {
        x = ReverseMortonCode2(Morton);
        y = ReverseMortonCode2(Morton >> 1);
    }

}





#endif