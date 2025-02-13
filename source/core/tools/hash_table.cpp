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

    void HashTable::insert(uint32_t key, uint32_t hash)
    {
        if (hash >= _next_hash.size())
        {
            _next_hash.resize(next_power_of_2(hash + 1));
        }
        key &= _hash_mask;
        _next_hash[hash] = _hash[key];
        _hash[key] = hash;
    }

    void HashTable::remove(uint32_t key, uint32_t hash)
    {
        assert(hash < _next_hash.size());

        key &= _hash_mask;
        if (_hash[key] == hash)
        {
            _hash[key] = _next_hash[hash];
        }
        else 
        {
            for (uint32_t ix = _hash[key]; ix != INVALID_SIZE_32; ix = _next_hash[ix])
            {
                if (_next_hash[ix] == hash)
                {
                    _next_hash[ix] = _next_hash[hash];
                    break;
                }
            }
        }
    }

    void HashTable::clear()
    {
        std::fill(_hash.begin(), _hash.end(), INVALID_SIZE_32);
    }

    void HashTable::reset()
    {
        _hash_mask = 0;
        _hash.resize(0);
        _next_hash.resize(0);
    }

    void HashTable::resize(uint32_t index_count)
    {
        resize(previous_power_of_2(index_count), index_count);
    }

    void HashTable::resize(uint32_t key_count, uint32_t hash_count)
    {
        if (!is_power_of_2(key_count))
        {
            key_count = next_power_of_2(key_count);   
        }

        reset();
        _hash_mask = key_count - 1;
        _hash.resize(key_count);
        _next_hash.resize(hash_count);
        clear();
    }


    HashTable::Iterator HashTable::operator[](uint32_t key)
    {
        if (_hash.empty() || _next_hash.empty()) return Iterator{ .index = INVALID_SIZE_32 };
        key &= _hash_mask;
        return Iterator{ .index = _hash[key], .next_index = _next_hash };
    }
}