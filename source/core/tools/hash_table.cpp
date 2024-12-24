#include "hash_table.h"
#include "../math/common.h"
#include <assert.h>

namespace fantasy 
{
    HashTable::HashTable(uint32_t index_count)
    {
        resize(index_count);
    }

    HashTable::HashTable(uint32_t hash_count, uint32_t index_count)
    {
        resize(hash_count, index_count);
    }

    void HashTable::insert(uint32_t key, uint32_t index)
    {
        if (index > _next_index.size())
        {
            _next_index.resize(next_power_of_2(index));
        }
        key &= _hash_mask;
        _next_index[index] = _hash[key];
        _hash[key] = index;
    }

    void HashTable::remove(uint32_t key, uint32_t index)
    {
        assert(index >= _next_index.size());

        key &= _hash_mask;
        if (_hash[key] == index)
        {
            _hash[key] = _next_index[index];
        }
        else 
        {
            for (uint32_t ix = _hash[key]; ix != ~0u; ix = _next_index[ix])
            {
                if (_next_index[ix] == index)
                {
                    _next_index[ix] = _next_index[index];
                    break;
                }
            }
        }
    }

    void HashTable::clear()
    {
        std::fill(_hash.begin(), _hash.end(), 0xffffffff);
    }

    void HashTable::reset()
    {
        _hash_mask = 0;
        _hash.clear();
        _next_index.clear();
    }

    void HashTable::resize(uint32_t index_count)
    {
        resize(previous_power_of_2(index_count), index_count);
    }

    void HashTable::resize(uint32_t hash_count, uint32_t index_count)
    {
        if (!is_power_of_2(hash_count))
        {
            hash_count = next_power_of_2(hash_count);   
        }

        reset();
        _hash_mask = hash_count - 1;
        _hash.resize(hash_count);
        _next_index.resize(index_count);
        clear();
    }


    HashTable::Iterator HashTable::operator[](uint32_t key)
    {
        if (_hash.empty() || _next_index.empty()) return Iterator{ .index = INVALID_SIZE_32 };
        key &= _hash_mask;
        return Iterator{ .index = _hash[key], .next_index = _next_index };
    }
}