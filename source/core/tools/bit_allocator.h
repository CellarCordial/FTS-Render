#ifndef TOOLS_BIT_ALLOCATOR_H
#define TOOLS_BIT_ALLOCATOR_H


#include <mutex>
#include <vector>
#include <cassert>
#include <mutex>

namespace fantasy
{
    class BitSetAllocator
    {
    public:
        BitSetAllocator(uint64_t size, bool multi_thread = true);

        uint32_t allocate();
        bool release(uint32_t index);
        
        void resize(uint64_t size);
        uint64_t get_capacity() const;

        void set_true(uint32_t index);
        void set_false(uint32_t index);

        bool operator[](uint32_t index);

    private:
        std::mutex _mutex;
        bool _multi_threaded;
        
        uint32_t _next_avaiable = 0;   // 记录下一个可能未被分配的位
        std::vector<uint32_t> _allocateds;
    };
}

#endif
