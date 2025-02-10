#ifndef DYNAMIC_RHI_VULKAN_ALLOCATOR_H
#define DYNAMIC_RHI_VULKAN_ALLOCATOR_H

#include "vk_forward.h"
#include "../resource.h"

namespace fantasy 
{
    class VKMemoryAllocator
    {
    public:
        explicit VKMemoryAllocator(const VKContext* context);

        bool allocate_buffer_memory(BufferInterface* buffer) const;
        void free_buffer_memory(BufferInterface* buffer) const;

        bool allocate_texture_memory(TextureInterface* texture) const;
        void free_texture_memory(TextureInterface* texture) const;

        bool allocate_memory(
            vk::DeviceMemory* resource,
            vk::MemoryRequirements memory_requirements,
            vk::MemoryPropertyFlags memory_property_flags,
            VkImage dedicated_image = nullptr,
            VkBuffer dedicated_buffer = nullptr
        ) const;
        void free_memory(vk::DeviceMemory vk_device_memory) const;

    private:
        const VKContext* _context;
        vk::PhysicalDeviceMemoryProperties _vk_physical_device_memory_properties;
    };

}




#endif