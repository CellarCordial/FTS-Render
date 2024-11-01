#include "Utils.h"

namespace FTS
{
    FBitSetAllocator::FBitSetAllocator(UINT64 stSize, BOOL bMultiThreads) : m_bMultiThreaded(bMultiThreads)
    {
        m_bAllocateds.resize(stSize);
    }

    UINT32 FBitSetAllocator::Allocate()
    {
        if (m_bMultiThreaded) m_Mutex.lock();

        UINT32 dwRet = 0;
        
        const UINT32 dwCapacity = static_cast<UINT32>(m_bAllocateds.size());
        for (UINT32 ix = 0; ix < dwCapacity; ++ix)
        {
            // 寻找未被分配的位
            const UINT32 dwPos = (m_dwNextAvailable + ix) % dwCapacity;

            if (!m_bAllocateds[dwPos])
            {
                dwRet = dwPos;
                m_dwNextAvailable = (dwRet + 1) % dwCapacity;
                m_bAllocateds[dwPos] = true;
                break;
            }
        }

        if (m_bMultiThreaded) m_Mutex.unlock();

        return dwRet;        
    }

    void FBitSetAllocator::Release(UINT32 dwIndex)
    {
        if (dwIndex < 0 || dwIndex > static_cast<UINT32>(m_bAllocateds.size()))
        {
            if (m_bMultiThreaded) m_Mutex.lock();

            m_bAllocateds[dwIndex] = false;
            m_dwNextAvailable = std::min(m_dwNextAvailable, dwIndex);            
            
            if (m_bMultiThreaded) m_Mutex.unlock();
        }
    }

    UINT64 FBitSetAllocator::GetCapacity() const
    {
        return m_bAllocateds.size();
    }

}
