#ifndef TOOLS_HASH_TABLE_H
#define TOOLS_HASH_TABLE_H


#include "../../Core/include/SysCall.h"
#include <span>
#include <vector>

namespace FTS
{
    class FHashTable
    {
    public:
        FHashTable() = default;
        explicit FHashTable(UINT32 dwIndexSize);
        FHashTable(UINT32 dwHashSize, UINT32 dwIndexSize);

        void Insert(UINT32 dwKey, UINT32 dwIndex);
        void Remove(UINT32 dwKey, UINT32 dwIndex);

        void Reset();
        void Clear();
        void Resize(UINT32 dwIndexSize);
        void Resize(UINT32 dwHashSize, UINT32 dwIndexSize);

        struct Iterator
        {
            UINT32 dwIndex;
            std::span<UINT32> NextIndex;

            void operator++() { dwIndex = NextIndex[dwIndex]; }
            BOOL operator!=(const Iterator& crIter) const { return dwIndex != crIter.dwIndex; }
            UINT32 operator*() { return dwIndex; }

            Iterator begin() const { return *this; }
            Iterator end() const { return Iterator{ .dwIndex = ~0u }; }
        };

        Iterator operator[](UINT32 dwIndex);

    private:

        UINT32 m_dwHashMask;
        std::vector<UINT32> m_Hash;
        std::vector<UINT32> m_NextIndex;
    };

    
    inline UINT32 MurmurAdd(UINT32 Hash, UINT32 Elememt)
    {
        Elememt *= 0xcc9e2d51;
        Elememt = (Elememt << 15) | (Elememt >> (32 - 15));
        Elememt *= 0x1b873593;

        Hash^=Elememt;
        Hash = (Hash << 13) | (Hash >> (32 - 13));
        Hash = Hash * 5 + 0xe6546b64;
        return Hash;
    }

    inline UINT32 MurmurMix(UINT32 Hash)
    {
        Hash ^= Hash >> 16;
        Hash *= 0x85ebca6b;
        Hash ^= Hash >> 13;
        Hash *= 0xc2b2ae35;
        Hash ^= Hash >> 16;
        return Hash;
    }
}





#endif