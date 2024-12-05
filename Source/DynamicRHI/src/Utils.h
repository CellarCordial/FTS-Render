#ifndef RHI_UTILS_H
#define RHI_UTILS_H

#include "../../Core/include/SysCall.h"
#include "../include/Descriptor.h"
#include <cassert>

namespace FTS
{

    template <class T>
    inline void HashCombine(UINT64& rstSeed, const T& crValue)
    {
        std::hash<T> Hasher;
        rstSeed = Hasher(crValue) + 0x9e3779b9 + (rstSeed << 6) + (rstSeed >> 2);
    }

    inline EResourceType GetNormalizedResourceType(EResourceType type)
    {
        switch (type)  // NOLINT(clang-diagnostic-switch-enum)
        {
        case EResourceType::StructuredBuffer_UAV:
        case EResourceType::RawBuffer_UAV:
            return EResourceType::TypedBuffer_UAV;
        case EResourceType::StructuredBuffer_SRV:
        case EResourceType::RawBuffer_SRV:
            return EResourceType::TypedBuffer_SRV;
        default:
            return type;
        }
    }

    inline BOOL AreResourceTypesCompatible(EResourceType Type1, EResourceType Type2)
    {
        if (Type1 == Type2) return true;

        Type1 = GetNormalizedResourceType(Type1);
        Type2 = GetNormalizedResourceType(Type2);

        if (Type1 == EResourceType::TypedBuffer_SRV && Type2 == EResourceType::Texture_SRV ||
            Type2 == EResourceType::TypedBuffer_SRV && Type1 == EResourceType::Texture_SRV)
            return true;

        if (Type1 == EResourceType::TypedBuffer_UAV && Type2 == EResourceType::Texture_UAV ||
            Type2 == EResourceType::TypedBuffer_UAV && Type1 == EResourceType::Texture_UAV)
            return true;

        return false;
    }

    
    /**
     * @brief       Get the different bit mask between crArray1 and crArray2. 
     * 
     * @tparam T 
     * @tparam U 
     * @param       crArray1 
     * @param       dwSize1 
     * @param       crArray2 
     * @param       dwSize2 
     * @return      UINT32 
     */
    template<typename T, typename U> 
    inline UINT32 FindArrayDifferenctBits(const T& crArray1, UINT32 dwSize1, const U& crArray2, UINT32 dwSize2)
    {
        assert(dwSize1 <= 32);
        assert(dwSize2 <= 32);

        if (dwSize1 != dwSize2) return ~0u;

        UINT32 dwMask = 0;
        for (UINT32 i = 0; i < dwSize1; i++)
        {
            if (crArray1[i] != crArray2[i]) dwMask |= (1 << i);
        }

        return dwMask;
    }


    template <class T>
    concept StackArrayType =requires(T t) { t.Size(); t[0]; };

    template <class StackArrayType>
    inline BOOL IsSameArrays(const StackArrayType& crArray1, const StackArrayType& crArray2)
    {
        if (crArray1.Size() != crArray2.Size()) return false;
        
        for (UINT32 ix = 0; ix < crArray1.Size(); ++ix)
        {
            if (crArray1[ix] != (crArray2[ix])) return false;
        }
        
        return true;
    }

}
































#endif