#ifndef TOOLS_HASH_TABLE_H
#define TOOLS_HASH_TABLE_H


#include <span>
#include <vector>
#include "../math/vector.h"

namespace fantasy
{
    template <class T>
    inline void hash_combine(uint64_t& key, const T& value)
    {
        std::hash<T> hasher;
        key = hasher(value) + 0x9e3779b9 + (key << 6) + (key >> 2);
    }

    class HashTable
    {
    public:
        HashTable() = default;
        explicit HashTable(uint32_t index_count);
        HashTable(uint32_t hash_count, uint32_t index_count);

        void insert(uint32_t key, uint32_t index);
        void remove(uint32_t key, uint32_t index);

        void reset();
        void clear();
        void resize(uint32_t index_count);
        void resize(uint32_t hash_count, uint32_t index_count);

        struct Iterator
        {
            uint32_t index;
            std::span<uint32_t> next_index;

            void operator++() { index = next_index[index]; }
            bool operator!=(const Iterator& crIter) const { return index != crIter.index; }
            uint32_t operator*() { return index; }

            Iterator begin() const { return *this; }
            Iterator end() const { return Iterator{ .index = ~0u }; }
        };

        Iterator operator[](uint32_t index);

    private:
        uint32_t _hash_mask;
        std::vector<uint32_t> _hash;
        std::vector<uint32_t> _next_hash;
    };

    
    inline uint32_t murmur_add(uint32_t hash, uint32_t elememt)
    {
        elememt *= 0xcc9e2d51;
        elememt = (elememt << 15) | (elememt >> (32 - 15));
        elememt *= 0x1b873593;

        hash ^= elememt;
        hash = (hash << 13) | (hash >> (32 - 13));
        hash = hash * 5 + 0xe6546b64;
        return hash;
    }

    inline uint32_t murmur_mix(uint32_t hash)
    {
        hash ^= hash >> 16;
        hash *= 0x85ebca6b;
        hash ^= hash >> 13;
        hash *= 0xc2b2ae35;
        hash ^= hash >> 16;
        return hash;
    }

    inline uint32_t hash(const float3& vec)
	{
		union 
		{
			float f;
			uint32_t u;
		} x, y, z;

		x.f = (vec.x == 0.0f ? 0 : vec.x);
		y.f = (vec.y == 0.0f ? 0 : vec.y);
		z.f = (vec.z == 0.0f ? 0 : vec.z);
		return murmur_mix(murmur_add(murmur_add(x.u, y.u), z.u));
	}
}





#endif