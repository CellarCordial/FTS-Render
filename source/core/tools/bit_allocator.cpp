#include "bit_allocator.h"
#include "../math/common.h"
#include "../tools/log.h"

namespace fantasy
{
    BitSetAllocator::BitSetAllocator(uint64_t size, bool multi_thread) : _multi_threaded(multi_thread)
    {
        _allocateds.resize(Align(size, 32ull) / 32ull, 0);
    }

    uint32_t BitSetAllocator::allocate()
    {
        if (_multi_threaded) _mutex.lock();

        uint32_t ret = 0;
        
        uint32_t id = _next_avaiable >> 5;
        uint32_t bit_index =  _next_avaiable & 31;
        for (uint32_t ix = 0; ix < _allocateds.size(); ++ix)
        {
            // 寻找未被分配的位
            const uint32_t pos = (id + ix) % _allocateds.size();

            for (uint32_t jx = bit_index; jx < 8; ++jx)
            {
                if (!(_allocateds[pos] & (1 << jx)))
                {
                    ret = pos;
                    _next_avaiable = (id << 5) + jx + 1;
                    _allocateds[pos] |= (1 << jx);
                    break;
                }
            }
            bit_index = 0;
        }

        if (_multi_threaded) _mutex.unlock();

        return ret;        
    }

    bool BitSetAllocator::release(uint32_t index)
    {
        uint32_t id = index >> 5;
        uint32_t bit_index =  index & 31;
        if (id < static_cast<uint32_t>(_allocateds.size()))
        {
            if (_multi_threaded) _mutex.lock();

            _allocateds[id] &= ~(1 << bit_index);
            _next_avaiable = std::min(_next_avaiable, index);            
            
            if (_multi_threaded) _mutex.unlock();
        }
        else
        {
            LOG_ERROR("invalid bit set Index.");
            return false;
        }
        return true;
    }

    void BitSetAllocator::resize(uint64_t size)
    {
        _allocateds.clear();
        _allocateds.resize(Align(size, 32ull) / 32ull, 0);
    }


    uint64_t BitSetAllocator::get_capacity() const
    {
        return _allocateds.size() * 32ull;
    }

    
    void BitSetAllocator::set_true(uint32_t index)
    {
        uint32_t id = index >> 5;
        uint32_t bit_index =  index & 31;
        _allocateds[id] |= (1 << bit_index);
    }

    void BitSetAllocator::set_false(uint32_t index)
    {
        uint32_t id = index >> 5;
        uint32_t bit_index =  index & 31;
        _allocateds[id] &= ~(1 << bit_index);
    }

    bool BitSetAllocator::operator[](uint32_t index)
    {
        uint32_t id = index >> 5;
        uint32_t bit_index =  index & 31;
        return static_cast<bool>((_allocateds[id] >> bit_index) & 1);
    }

}