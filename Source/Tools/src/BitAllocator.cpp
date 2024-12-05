#include "../include/BitAllocator.h"
#include "../../Math/include/Common.h"
#include "../../Core/include/ComIntf.h"

namespace FTS
{
    FBitSetAllocator::FBitSetAllocator(UINT64 stSize, BOOL bMultiThreads) : m_bMultiThreaded(bMultiThreads)
    {
        m_bAllocateds.resize(Align(stSize, 32ull) / 32ull, 0);
    }

    UINT32 FBitSetAllocator::Allocate()
    {
        if (m_bMultiThreaded) m_Mutex.lock();

        UINT32 dwRet = 0;
        
        UINT32 dwID = m_dwNextAvailable >> 5;
        UINT32 dwBitIndex =  m_dwNextAvailable & 31;
        for (UINT32 ix = 0; ix < m_bAllocateds.size(); ++ix)
        {
            // 寻找未被分配的位
            const UINT32 dwPos = (dwID + ix) % m_bAllocateds.size();

            for (UINT32 jx = dwBitIndex; jx < 8; ++jx)
            {
                if (!(m_bAllocateds[dwPos] & (1 << jx)))
                {
                    dwRet = dwPos;
                    m_dwNextAvailable = (dwID << 5) + jx + 1;
                    m_bAllocateds[dwPos] |= (1 << jx);
                    break;
                }
            }
            dwBitIndex = 0;
        }

        if (m_bMultiThreaded) m_Mutex.unlock();

        return dwRet;        
    }

    BOOL FBitSetAllocator::Release(UINT32 dwIndex)
    {
        UINT32 dwID = dwIndex >> 5;
        UINT32 dwBitIndex =  dwIndex & 31;
        if (dwID < static_cast<UINT32>(m_bAllocateds.size()))
        {
            if (m_bMultiThreaded) m_Mutex.lock();

            m_bAllocateds[dwID] &= ~(1 << dwBitIndex);
            m_dwNextAvailable = std::min(m_dwNextAvailable, dwIndex);            
            
            if (m_bMultiThreaded) m_Mutex.unlock();
        }
        else
        {
            LOG_ERROR("Invalid bit set Index.");
            return false;
        }
        return true;
    }

    void FBitSetAllocator::Resize(UINT64 stSize)
    {
        m_bAllocateds.clear();
        m_bAllocateds.resize(Align(stSize, 32ull) / 32ull, 0);
    }


    UINT64 FBitSetAllocator::GetCapacity() const
    {
        return m_bAllocateds.size() * 32ull;
    }

    
    void FBitSetAllocator::SetTrue(UINT32 dwIndex)
    {
        UINT32 dwID = dwIndex >> 5;
        UINT32 dwBitIndex =  dwIndex & 31;
        m_bAllocateds[dwID] |= (1 << dwBitIndex);
    }

    void FBitSetAllocator::SetFalse(UINT32 dwIndex)
    {
        UINT32 dwID = dwIndex >> 5;
        UINT32 dwBitIndex =  dwIndex & 31;
        m_bAllocateds[dwID] &= ~(1 << dwBitIndex);
    }

    BOOL FBitSetAllocator::operator[](UINT32 dwIndex)
    {
        UINT32 dwID = dwIndex >> 5;
        UINT32 dwBitIndex =  dwIndex & 31;
        return static_cast<BOOL>((m_bAllocateds[dwID] >> dwBitIndex) & 1);
    }

}