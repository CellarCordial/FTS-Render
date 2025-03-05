#ifndef CORE_TOOLS_LRU_CACHE_H
#define CORE_TOOLS_LRU_CACHE_H

#include <cstdint>
#include <cassert>
#include <unordered_map>
#include <utility>

namespace fantasy 
{
    template <typename T>
    class LruCache 
    {
    public:
        LruCache(uint32_t capacity = 0) : _capacity(capacity)
        {
        }

        bool check_cache(uint64_t key, T& out_element)
        {
            auto iter = _map.find(key);
            if (iter != _map.end())
            {
                out_element = iter->second->second;
                _list.splice(_list.begin(), _list, iter->second);
                return true;
            }
            return false;
        }

        T evict()
        {
            if (_list.empty()) assert(!"Lru is empty.");

            auto [key, value] = _list.back();
            _list.pop_back();
            _map.erase(key);

            return value;
        }

        void insert(uint64_t key, const T& value) 
        {
            auto map_iter = _map.find(key);
            if(map_iter == _map.end())
            {
                _list.push_front(std::make_pair(key, value));
                _map.insert(std::make_pair(key, _list.begin()));
                if(_list.size() > _capacity)
                {
                    _map.erase(_list.back().first);
                    _list.pop_back();
                }
            }
            else if (_list.front().second != value)
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
        std::list<std::pair<uint64_t, T>> _list;
        std::unordered_map<uint64_t,  const typename std::list<std::pair<uint64_t, T>>::iterator> _map;
    };


}

#endif