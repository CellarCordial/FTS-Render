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

            Iterator begine() const { return *this; }
            Iterator end() const { return Iterator{ .dwIndex = ~0u }; }
        };

        Iterator operator[](UINT32 dwIndex);

    private:

        UINT32 m_dwHashMask;
        std::vector<UINT32> m_Hash;
        std::vector<UINT32> m_NextIndex;
    };
}





#endif