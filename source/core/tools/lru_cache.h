#ifndef CORE_TOOLS_LRU_CACHE_H
#define CORE_TOOLS_LRU_CACHE_H

#include <cstdint>
#include <list>
#include <unordered_map>
#include <functional>

namespace fantasy 
{
    template <typename T, typename OnEvict = std::function<void(T&)>>
    class LruCache 
    {
    public:
        LruCache(uint32_t capacity = 0, OnEvict on_evict = [](T&) {}) : 
            _capacity(capacity), _on_evict(on_evict) 
        {
        }

        void initialize(uint32_t capacity, OnEvict on_evict)
        {
            _capacity = capacity;
            _on_evict = on_evict;
        }
        
        T* get(uint64_t key) 
        {
            auto iter = _map.find(key);
            if(iter == _map.end())
            {
                return nullptr;
            }
            else
            {
                _list.splice(_list.begin(), _list, iter->second);
                auto data = _map[key];
                return &data->second;
            }
        }
        
        void insert(uint64_t key, const T& value) 
        {
            auto iter = _map.find(key);
            if(iter == _map.end())
            {
                _list.push_front(std::make_pair(key, value));
                _map.insert(std::make_pair(key, _list.begin()));
                if(_list.size() > _capacity)
                {
                    auto data = &_list.back();
                    _map.erase(data->first);
                    _on_evict(data->second);
                    _list.pop_back();
                }
            }
            else
            {
                iter->second->second = value;
                _list.splice(_list.begin(), _list, iter->second);
            }
        }

    private:
        OnEvict _on_evict;
        uint32_t _capacity;
        std::unordered_map<uint64_t,  typename std::list<std::pair<uint64_t, T>>::iterator> _map;
        std::list<std::pair<uint64_t, T>> _list;
    };
}








#endif