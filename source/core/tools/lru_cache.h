#ifndef CORE_TOOLS_LRU_CACHE_H
#define CORE_TOOLS_LRU_CACHE_H

#include <cstdint>
#include <cassert>
#include <queue>
#include <unordered_map>
#include "log.h"
#include "morton_code.h"

namespace fantasy 
{
    template <typename T>
    class LruCache 
    {
    public:
        LruCache(uint32_t capacity = 0) : _capacity(capacity)
        {
        }

        bool check_cache(uint64_t key, T& out_element) const
        {
            auto iter = _map.find(key);
            if (iter != _map.end())
            {
                out_element = *iter->second;
                return true;
            }
            return false;
        }

        T evict()
        {
            if (_list.empty()) assert(!"Lru is empty.");

            T evict = _list.back();
            _list.pop_back();

            return evict;
        }

        void insert(uint64_t key, const T& value) 
        {
            auto map_iter = _map.find(key);
            if(map_iter == _map.end())
            {
                _list.push_front(value);
                _map.insert(std::make_pair(key, _list.begin()));
                if(_list.size() > _capacity)
                {
                    _map.erase(map_iter);
                    _list.pop_back();
                }
            }
            else if (_list.front() != value)
            {
                _list.splice(_list.begin(), _list, map_iter->second);
            }
        }

        void reset()
        {
            _map.clear();
            _list.clear();
        }

    private:
        uint32_t _capacity;
        std::list<T> _list;
        std::unordered_map<uint64_t,  const typename std::list<T>::iterator> _map;
    };

    template <typename T>
    class MortonLruCache 
    {
    public:
        MortonLruCache(uint32_t levels) : 
            _max_level(levels - 1), _level_lists(levels)
        {
            uint32_t resoluton = 2 << (_max_level);
            _level_lists[_max_level].push_back(Block{
                .morton_start = 0,
                .morton_end = morton_encode(resoluton - 1, resoluton - 1)
            });
        }

        bool check_cache(uint64_t key, T& out_element) const
        {
            auto iter = _map.find(key);
            if (iter != _map.end())
            {
                out_element = iter->second->value;
                return true;
            }
            return false;
        }

        T evict(uint32_t level)
        {
            if (_level_lists[level].empty()) assert(generate_available_block(level));

            T ret = _level_lists[level].back().value;
            _level_lists[level].pop_back();

            return ret;
        }

        bool insert(uint64_t key, const T& value, uint32_t level) 
        {
            auto map_iter = _map.find(key);
            if(map_iter == _map.end())
            {
                if (_level_lists[level].empty()) ReturnIfFalse(generate_available_block(level));

                auto iter = _level_lists[level].begin();
                iter->key = key;
                iter->value = value;

                _map.insert(std::make_pair(key, iter));

                _earliest_used_levels.push(level);
            }
            else if (_level_lists[level].front().value != value)
            {
                _level_lists[level].splice(_level_lists[level].begin(), _level_lists[level], map_iter->second);
            }
            return true;
        }

        void reset()
        {
            _level_lists.clear();
            _level_lists.resize(_max_level + 1);

            uint32_t resoluton = 2 << (_max_level);
            _level_lists[_max_level].push_back(Block{
                .morton_start = 0,
                .morton_end = morton_encode(resoluton - 1, resoluton - 1)
            });
        }

    private:
        bool generate_available_block(uint32_t level)
        {
            uint32_t earliest_level = _earliest_used_levels.front();
            bool search_high_level = level < earliest_level;
            _earliest_used_levels.pop();
            
            if (search_high_level)
            {
                if (!separate_block(level + 1))
                {
                    ReturnIfFalse(combine_block(level - 1));
                }
            }
            else
            {
                if (!combine_block(level - 1))
                {
                    ReturnIfFalse(separate_block(level + 1));
                }
            }
            return !_level_lists[level].empty();
        }

        bool combine_block(uint32_t level)
        {
            if (level < _max_level && level >= 0) return false;

            if (_level_lists[level].size() < 4) 
            {
                if (combine_block(level - 1)) return false;
            }

            uint32_t block_start = _level_lists[level].back().morton_start;

            _level_lists[level + 1].push_front(Block{
                .morton_start = block_start,
                .morton_end = block_start + (4 << (level + 1)) - 1
            });

            for (uint32_t ix = 0; ix < 4; ++ix)
            {
                _map.erase(_level_lists[level].back().key);
                _level_lists[level].pop_back();
            }

            return true;
        }

        bool separate_block(uint32_t level)
        {
            if (level > 0 && level <= _max_level) return false;
            
            if (_level_lists[level].empty())
            {
                if (combine_block(level + 1)) return false;
            }

            uint32_t stride = (4 << (level - 1)) - 1;

            uint32_t block_start = _level_lists[level].back().morton_start;
            for (uint32_t ix = 0; ix < 4; ++ix)
            {
                _level_lists[level - 1].push_front(Block{
                    .morton_start = block_start,
                    .morton_end = block_start + stride
                });

                block_start += stride;
            }

            _map.erase(_level_lists[level].back().key);
            _level_lists[level].pop_back();

            return true;
        }

    private:
        struct Block
        {
            uint32_t morton_start = 0;
            uint32_t morton_end = 0;

            uint64_t key = INVALID_SIZE_64;
            T value;
        };

        uint32_t _max_level;
        std::queue<uint32_t> _earliest_used_levels;
        std::vector<std::list<Block>> _level_lists;
        std::unordered_map<uint64_t,  const typename std::list<Block>::iterator> _map;
    };
}

#endif