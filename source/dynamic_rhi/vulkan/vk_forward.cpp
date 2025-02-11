#include "vk_forward.h"
#include "vk_memory.h"

namespace fantasy 
{
    VKHeap::VKHeap(const VKContext* context, VKMemoryAllocator* allocator, const HeapDesc& desc_)
        : _context(context), _allocator(allocator), desc(desc_)
    {
    }

    VKHeap::~VKHeap()
    {
        _allocator->free_memory(vk_device_memory);
    }

    bool VKHeap::initialize() 
    { 
        return true; 
    }

    const HeapDesc& VKHeap::get_desc() const 
    { 
        return desc; 
    }

    bool wait_for_semaphore(const VKContext* context, vk::Semaphore vk_semaphore, uint64_t semaphore_value)
    {
        if (context->device.getSemaphoreCounterValue(vk_semaphore) > semaphore_value) return true;

        vk::SemaphoreWaitInfo vk_semaphore_wait_info{};
        vk_semaphore_wait_info.pNext = nullptr;
        vk_semaphore_wait_info.flags = vk::SemaphoreWaitFlags();
        vk_semaphore_wait_info.semaphoreCount = 1;
        vk_semaphore_wait_info.pSemaphores = &vk_semaphore;
        vk_semaphore_wait_info.pValues = &semaphore_value;

        return context->device.waitSemaphores(vk_semaphore_wait_info, UINT64_MAX) == vk::Result::eSuccess;
    }


}