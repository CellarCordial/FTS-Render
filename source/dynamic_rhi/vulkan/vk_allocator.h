#ifndef DYNAMIC_RHI_VULKAN_ALLOCATOR_H
#define DYNAMIC_RHI_VULKAN_ALLOCATOR_H

#include "vk_forward.h"

namespace fantasy 
{
    class BufferInterface;
    class TextureInterface;
    
    class VulkanAllocator
    {
    public:
        explicit VulkanAllocator(const VKContext* context)
            : _context(context)
        {
        }

        vk::Result allocateBufferMemory(BufferInterface* buffer, bool enable_buffer_address = false) const;
        void freeBufferMemory(BufferInterface* buffer) const;

        vk::Result allocateTextureMemory(TextureInterface* texture) const;
        void freeTextureMemory(TextureInterface* texture) const;

        vk::Result allocateMemory(vk::DeviceMemory* res,
            vk::MemoryRequirements memRequirements,
            vk::MemoryPropertyFlags memPropertyFlags,
            bool enableDeviceAddress = false,
            bool enableExportMemory = false,
            VkImage dedicatedImage = nullptr,
            VkBuffer dedicatedBuffer = nullptr) const;
        void freeMemory(vk::DeviceMemory* res) const;

    private:
        const VKContext* _context;
    };

}




#endif