#ifndef CORE_CONTAINER_H
#define CORE_CONTAINER_H


#include "../../Core/include/SysCall.h"
#include <array>
#include <cassert>


namespace FTS 
{
    template <class T, UINT32 dwMaxSize>
    class TStackArray : public std::array<T, dwMaxSize>
    {
        typedef std::array<T, dwMaxSize> _Base;
        
        using typename _Base::iterator;
        using typename _Base::const_iterator;

    public:
        using _Base::begin;
        using _Base::cbegin;
        using _Base::end;
        using _Base::cend;
        using _Base::data;

        
        TStackArray() : _Base(), m_dwCurrSize(0)
        {
        }
        
        TStackArray(UINT32 dwInitSize) : _Base(), m_dwCurrSize(dwInitSize)
        {
            assert(dwInitSize <= dwMaxSize);
        }

        _Base::reference operator[](UINT32 dwPos)
        {
            assert(dwPos < m_dwCurrSize);
            return _Base::operator[](dwPos);
        }

        _Base::const_reference operator[](UINT32 dwPos) const
        {
            assert(dwPos < m_dwCurrSize);
            return _Base::operator[](dwPos);
        }

        _Base::reference Back()
        {
            auto Ret = end();
            return *(--Ret);
        }

        _Base::const_reference Back() const
        {
            auto Ret = end();
            return *(--Ret);
        }

        BOOL Empty() const
        {
            return m_dwCurrSize == 0;
        }

        UINT32 Size() const
        {
            return m_dwCurrSize;
        }

        constexpr UINT32 MaxSize() const
        {
            return dwMaxSize;
        }

        void PushBack(const T& crValue)
        {
            assert(m_dwCurrSize < dwMaxSize);
            *(data() + m_dwCurrSize) = crValue;
            m_dwCurrSize++;
        }

        void PushBack(T&& rrValue)
        {
            assert(m_dwCurrSize < dwMaxSize);
            *(data() + m_dwCurrSize) = std::move(rrValue);
            m_dwCurrSize++;
        }

        void PopBack()
        {
            assert(m_dwCurrSize > 0);
            m_dwCurrSize--;
        }

        void Resize(UINT32 dwNewSize)
        {
            assert(dwNewSize <= dwMaxSize);

            if (m_dwCurrSize > dwNewSize)
            {
                for (UINT32 ix = dwNewSize; ix < m_dwCurrSize; ++ix)
                {
                    (data() + ix)->~T();
                }
            }
            else 
            {
                for (UINT32 ix = m_dwCurrSize; ix < dwNewSize; ++ix)
                {
                    new (data() + ix) T();
                }
            }
            m_dwCurrSize = dwNewSize;
        }


        // 这些 end() 是为了能在 for 循环中直接使用 ":" 来获取引用.

        _Base::iterator end()
        {
            return iterator(begin()) + m_dwCurrSize;
        }
        
        _Base::const_iterator end() const
        {
            return cend();
        }

        _Base::const_iterator cend() const
        {
            return const_iterator(cbegin()) + m_dwCurrSize;
        }

    private:
        UINT32 m_dwCurrSize = 0;
    };

}














#endif