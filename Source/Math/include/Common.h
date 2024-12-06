#ifndef MATH_COMMON_H
#define MATH_COMMON_H

#include "../../Core/include/SysCall.h"
#include <limits>

namespace FTS 
{
#ifdef _MSC_VER
#ifndef MachineEpsilon
#define MachineEpsilon (std::numeric_limits<FTS::FLOAT>::epsilon() * 0.5f)
#endif
#ifndef Infinity
#define Infinity std::numeric_limits<FTS::FLOAT>::infinity()
#endif
#else
    static constexpr FTS::FLOAT MachineEpsilon =
        std::numeric_limits<FTS::FLOAT>::epsilon() * 0.5f;
    static constexpr FLOAT Infinity =
        std::numeric_limits<FTS::FLOAT>::infinity();
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

#ifndef INVALID_SIZE_32
#define INVALID_SIZE_32 static_cast<FTS::UINT32>(-1)
#endif

#ifndef INVALID_SIZE_64
#define INVALID_SIZE_64 static_cast<FTS::UINT64>(-1)
#endif

#define FTS_ENUM_CLASS_FLAG_OPERATORS(T)                          \
inline T operator|(T a, T b) { return T(UINT32(a) | UINT32(b)); } \
inline T operator&(T a, T b) { return T(UINT32(a) & UINT32(b)); } \
inline void operator|=(T& a, T b) { a = a | b; }                  \
inline void operator&=(T& a, T b) { a = a & b; }                  \
inline T operator~(T a) { return T(~UINT32(a)); }                 \
inline bool operator!(T a) { return UINT32(a) == 0; }             \
inline bool operator==(T a, UINT32 b) { return UINT32(a) == b; }  \
inline bool operator!=(T a, UINT32 b) { return UINT32(a) != b; }


    static constexpr FLOAT PI = 3.14159265358979323846f;
    static constexpr FLOAT InvPI = 0.31830988618379067154f;
    static constexpr FLOAT InvPI2 = 0.15915494309189533577f;
    static constexpr FLOAT InvPI4 = 0.07957747154594766788f;


    inline FLOAT Radians(FLOAT fDegree) { return (PI / 180) * fDegree; }

    inline FLOAT Degrees(FLOAT fRadian) { return (180 / PI) * fRadian; }

    template <typename T, typename L, typename H>
    T Clamp(T _t, L Low, H High)
    {
        T tLow = static_cast<T>(Low);
        T tHigh = static_cast<T>(High);
        if (_t < tLow) return tLow;
        if (_t > tHigh) return tHigh;
        return _t;
    }

    inline FLOAT Lerp(FLOAT fValue1, FLOAT fValue2, FLOAT f)
    {
        return (1.0f - f) * fValue1 + f * fValue2;
    }

    inline BOOL CheckIfPowerOf2(UINT32 v)
    {
        return v & (v - 1);
    }

    inline UINT32 NextPowerOf2(UINT32 v)
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

    inline UINT32 PreviousPowerOf2(UINT32 v)
    {
        while (v & (v - 1))
        {
            // 清除最低位的 1.
            v ^= (v & -v);
        }
        return v;   
    }

    inline constexpr FLOAT Gamma(INT32 dwValue)
    {
        return (static_cast<FLOAT>(dwValue) * MachineEpsilon) / (1 - static_cast<FLOAT>(dwValue) * MachineEpsilon);
    }

    
    template<typename T> 
    T Align(T size, T alignment)
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }

    inline UINT32 TriangleIndexCycle3(UINT32 dw)
    {
        UINT32 dwMod3 = dw %3;
        return dw - dwMod3 + ((1 << dwMod3) & 3);
    }

    inline UINT32 TriangleIndexCycle3(UINT32 dw, UINT32 dwOfs)
    {
        return dw - dw % 3 + (dw + dwOfs) % 3;
    }

}



#endif