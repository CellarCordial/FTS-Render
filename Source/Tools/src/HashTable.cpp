#include "../include/HashTable.h"
#include "../../Math/include/Common.h"

namespace FTS 
{
    FHashTable::FHashTable(UINT32 dwIndexSize)
    {
        Resize(dwIndexSize);
    }

    FHashTable::FHashTable(UINT32 dwHashSize, UINT32 dwIndexSize)
    {
        Resize(dwHashSize, dwIndexSize);
    }

    void FHashTable::Insert(UINT32 dwKey, UINT32 dwIndex)
    {
        if (dwIndex > m_NextIndex.size())
        {
            m_NextIndex.resize(NextPowerOf2(dwIndex));
        }
        dwKey &= m_dwHashMask;
        m_NextIndex[dwIndex] = m_Hash[dwKey];
        m_Hash[dwKey] = dwIndex;
    }

    void FHashTable::Remove(UINT32 dwKey, UINT32 dwIndex)
    {
        assert(dwIndex >= m_NextIndex.size());

        dwKey &= m_dwHashMask;
        if (m_Hash[dwKey] == dwIndex)
        {
            m_Hash[dwKey] = m_NextIndex[dwIndex];
        }
        else 
        {
            for (UINT32 ix = m_Hash[dwKey]; ix != ~0u; ix = m_NextIndex[ix])
            {
                if (m_NextIndex[ix] == dwIndex)
                {
                    m_NextIndex[ix] = m_NextIndex[dwIndex];
                    break;
                }
            }
        }
    }

    void FHashTable::Clear()
    {
        std::fill(m_Hash.begin(), m_Hash.end(), 0xffffffff);
    }

    void FHashTable::Reset()
    {
        m_dwHashMask = 0;
        m_Hash.clear();
        m_NextIndex.clear();
    }

    void FHashTable::Resize(UINT32 dwIndexSize)
    {
        Resize(PreviousPowerOf2(dwIndexSize), dwIndexSize);
    }

    void FHashTable::Resize(UINT32 dwHashSize, UINT32 dwIndexSize)
    {
        if (!CheckIfPowerOf2(dwHashSize))
        {
            dwHashSize = NextPowerOf2(dwHashSize);   
        }

        Reset();
        m_dwHashMask = dwHashSize - 1;
        m_Hash.resize(dwHashSize);
        m_NextIndex.resize(dwIndexSize);
        Clear();
    }


    FHashTable::Iterator FHashTable::operator[](UINT32 dwKey)
    {
        if (m_Hash.empty() || m_NextIndex.empty()) return Iterator{ .dwIndex = ~0u };
        dwKey &= m_dwHashMask;
        return Iterator{ .dwIndex = m_Hash[dwKey], .NextIndex = m_NextIndex };
    }
}