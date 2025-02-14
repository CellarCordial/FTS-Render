#ifndef MATH_COMMON_H
#define MATH_COMMON_H

#include <limits>
#include <stdint.h>
#include <cassert>

namespace fantasy 
{
#ifdef _MSC_VER
#ifndef MachineEpsilon
#define MachineEpsilon (std::numeric_limits<float>::epsilon() * 0.5f)
#endif
#else
    static constexpr float MachineEpsilon =
        std::numeric_limits<float>::epsilon() * 0.5f;
#endif

#ifndef NOT_FLOAT_ZERO
#define NOT_FLOAT_ZERO(x) ((x) < -0.0001f || (x) > 0.0001f)
#endif

#ifndef NOT_FLOAT_ONE
#define NOT_FLOAT_ONE(x) ((x) < 0.9999f || (x) > 1.0001f)
#endif

#ifndef EQUAL_FLOAT_ZERO
#define EQUAL_FLOAT_ZERO(x) ((x) > -0.0001f && (x) < 0.0001f)
#endif

#ifndef EQUAL_FLOAT_ONE
#define EQUAL_FLOAT_ONE(x) ((x) > 0.9999f && (x) < 1.0001f)
#endif

#ifndef INVALID_SIZE_16
#define INVALID_SIZE_16 static_cast<uint16_t>(-1)
#endif

#ifndef INVALID_SIZE_32
#define INVALID_SIZE_32 static_cast<uint32_t>(-1)
#endif

#ifndef INVALID_SIZE_64
#define INVALID_SIZE_64 static_cast<uint64_t>(-1)
#endif

#ifndef ENUM_CLASS_FLAG_OPERATORS
#define ENUM_CLASS_FLAG_OPERATORS(T)                                    \
inline T operator|(T a, T b) { return T(uint32_t(a) | uint32_t(b)); }   \
inline T operator&(T a, T b) { return T(uint32_t(a) & uint32_t(b)); }   \
inline void operator|=(T& a, T b) { a = a | b; }                        \
inline void operator&=(T& a, T b) { a = a & b; }                        \
inline T operator~(T a) { return T(~uint32_t(a)); }                     \
inline bool operator!(T a) { return uint32_t(a) == 0; }                 \
inline bool operator==(T a, uint32_t b) { return uint32_t(a) == b; }    \
inline bool operator!=(T a, uint32_t b) { return uint32_t(a) != b; }
#endif

    static constexpr float PI = 3.14159265358979323846f;
    static constexpr float InvPI = 0.31830988618379067154f;
    static constexpr float InvPI2 = 0.15915494309189533577f;
    static constexpr float InvPI4 = 0.07957747154594766788f;


    inline float radians(float fDegree) { return (PI / 180) * fDegree; }

    inline float degrees(float fRadian) { return (180 / PI) * fRadian; }

    template <typename T, typename L, typename H>
    T Clamp(T _t, L Low, H High)
    {
        T tLow = static_cast<T>(Low);
        T tHigh = static_cast<T>(High);
        if (_t < tLow) return tLow;
        if (_t > tHigh) return tHigh;
        return _t;
    }

    inline float lerp(float fValue1, float fValue2, float f)
    {
        return (1.0f - f) * fValue1 + f * fValue2;
    }

    inline bool is_power_of_2(uint32_t v)
    {
        return (v & (v - 1)) == 0;
    }

    inline uint32_t next_power_of_2(uint32_t v)
    {
        if(v & (v - 1))
        {
            while (v & (v - 1)) 
            {
                // 清除最低位的 1.
                v ^= (v & -v);
            }
        }
        return v == 0 ? 1 : (v << 1);
    }

    inline uint32_t previous_power_of_2(uint32_t v)
    {
        while (v & (v - 1))
        {
            // 清除最低位的 1.
            v ^= (v & -v);
        }
        return v;   
    }

    inline constexpr float gamma(int32_t dwValue)
    {
        return (static_cast<float>(dwValue) * MachineEpsilon) / (1 - static_cast<float>(dwValue) * MachineEpsilon);
    }

    
    template<typename T> 
    T align(T size, T alignment)
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }

    inline uint32_t triangle_edge_index_cycle(uint32_t ix)
    {
        uint32_t mod3 = ix % 3;
        return ix - mod3 + ((1 << mod3) & 3);
    }

    inline uint32_t triangle_edge_index_cycle(uint32_t ix, uint32_t ofs)
    {
        return ix - ix % 3 + (ix + ofs) % 3;
    }

    inline uint32_t search_most_significant_bit(uint32_t x)
    {
        // 折半查找最高有效位.
        uint32_t res = 0, t = 16, y = 0;
        y = -((x >> t) != 0 ? 1 : 0); res += y & t; x >>= y & t; t >>= 1;
        y = -((x >> t) != 0 ? 1 : 0); res += y & t; x >>= y & t; t >>= 1;
        y = -((x >> t) != 0 ? 1 : 0); res += y & t; x >>= y & t; t >>= 1;
        y = -((x >> t) != 0 ? 1 : 0); res += y & t; x >>= y & t; t >>= 1;
        y = (x >> t) != 0 ? 1 : 0; res += y;
        return res;
    }

    template<typename T, typename U> 
    inline uint32_t find_array_different_bits(const T& array0, uint32_t size0, const U& array1, uint32_t size1)
    {
        assert(size0 <= 32);
        assert(size1 <= 32);

        if (size0 != size1) return ~0u;

        uint32_t mask = 0;
        for (uint32_t i = 0; i < size0; i++)
        {
            if (array0[i] != array1[i]) mask |= (1 << i);
        }

        return mask;
    }

    template <class T>
    concept StackArrayType =requires(T t) { t.size(); t[0]; };

    template <class StackArrayType>
    inline bool is_same_arrays(const StackArrayType& array0, const StackArrayType& array1)
    {
        if (array0.size() != array1.size()) return false;
        
        for (uint32_t ix = 0; ix < array0.size(); ++ix)
        {
            if (array0[ix] != (array1[ix])) return false;
        }
        
        return true;
    }

}



#endif