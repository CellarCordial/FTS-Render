#ifndef DYNAMIC_RHI_VULKAN_ALLOCATOR_H
#define DYNAMIC_RHI_VULKAN_ALLOCATOR_H

#include "vk_forward.h"
#include "../resource.h"

namespace fantasy 
{
    struct VKMemoryResource
    {
        bool managed = true;
        vk::DeviceMemory memory;
    };

    
    class VKMemoryAllocator
    {
    public:
        explicit VKMemoryAllocator(const VKContext* context)
            : _context(context)
        {
        }

        bool allocate_buffer_memory(BufferInterface* buffer, bool enable_buffer_address = false) const;
        void free_buffer_memory(BufferInterface* buffer) const;

        bool allocate_texture_memory(TextureInterface* texture) const;
        void free_texture_memory(TextureInterface* texture) const;

        bool allocate_memory(
            vk::DeviceMemory* resource,
            vk::MemoryRequirements memory_requirements,
            vk::MemoryPropertyFlags memory_property_flags,
            bool enable_divice_address = false,
            bool enable_export_memory = false,
            VkImage dedicated_image = nullptr,
            VkBuffer dedicated_buffer = nullptr
        ) const;
        void free_memory(vk::DeviceMemory* resource) const;

    private:
        const VKContext* _context;
    };

}




#endif