#ifndef TOOLS_BIT_ALLOCATOR_H
#define TOOLS_BIT_ALLOCATOR_H


#include "../../Core/include/SysCall.h"
#include <mutex>
#include <vector>
#include <cassert>
#include <mutex>

namespace FTS
{
    class FBitSetAllocator
    {
    public:
        FBitSetAllocator(UINT64 stSize, BOOL bMultiThreads);

        UINT32 Allocate();
        BOOL Release(UINT32 dwIndex);
        
        void Resize(UINT64 stSize);
        UINT64 GetCapacity() const;

        void SetTrue(UINT32 dwIndex);
        void SetFalse(UINT32 dwIndex);

        BOOL operator[](UINT32 dwIndex);

    private:
        std::mutex m_Mutex;
        BOOL m_bMultiThreaded;
        
        UINT32 m_dwNextAvailable = 0;   // 记录下一个可能未被分配的位
        std::vector<UINT32> m_bAllocateds;
    };
}

#endif
