#ifndef TOOLS_STACK_ARRAY_H
#define TOOLS_STACK_ARRAY_H


#include <array>
#include <cassert>

namespace fantasy 
{
    template <class T, uint32_t dwMaxSize>
    class StackArray : public std::array<T, dwMaxSize>
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

        
        StackArray() : _Base(), _current_size(0)
        {
        }
        
        StackArray(uint32_t init_size) : _Base(), _current_size(init_size)
        {
            assert(init_size <= dwMaxSize);
        }

        _Base::reference operator[](uint32_t pos)
        {
            assert(pos < _current_size);
            return _Base::operator[](pos);
        }

        _Base::const_reference operator[](uint32_t pos) const
        {
            assert(pos < _current_size);
            return _Base::operator[](pos);
        }

        _Base::reference back()
        {
            auto ret = end();
            return *(--ret);
        }

        _Base::const_reference back() const
        {
            auto ret = end();
            return *(--ret);
        }

        bool empty() const
        {
            return _current_size == 0;
        }

        uint32_t size() const
        {
            return _current_size;
        }

        constexpr uint32_t max_size() const
        {
            return dwMaxSize;
        }

        void push_back(const T& value)
        {
            assert(_current_size < dwMaxSize);
            *(data() + _current_size) = value;
            _current_size++;
        }

        void push_back(T&& rrValue)
        {
            assert(_current_size < dwMaxSize);
            *(data() + _current_size) = std::move(rrValue);
            _current_size++;
        }

        void pop_back()
        {
            assert(_current_size > 0);
            _current_size--;
        }

        void resize(uint32_t new_size)
        {
            assert(new_size <= dwMaxSize);

            if (_current_size > new_size)
            {
                for (uint32_t ix = new_size; ix < _current_size; ++ix)
                {
                    (data() + ix)->~T();
                }
            }
            else 
            {
                for (uint32_t ix = _current_size; ix < new_size; ++ix)
                {
                    new (data() + ix) T();
                }
            }
            _current_size = new_size;
        }


        // 这些 end() 是为了能在 for 循环中直接使用 ":" 来获取引用.

        _Base::iterator end()
        {
            return iterator(begin()) + _current_size;
        }
        
        _Base::const_iterator end() const
        {
            return cend();
        }

        _Base::const_iterator cend() const
        {
            return const_iterator(cbegin()) + _current_size;
        }

    private:
        uint32_t _current_size = 0;
    };

}














#endif