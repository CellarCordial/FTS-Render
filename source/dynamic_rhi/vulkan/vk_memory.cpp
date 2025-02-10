#include "vk_memory.h"
#include "vk_resource.h"
#include "../../core/tools/check_cast.h"
#include "../../core/tools/log.h"

namespace fantasy 
{
    VKMemoryAllocator::VKMemoryAllocator(const VKContext* context)
        : _context(context)
    {
        _context->vk_physical_device.getMemoryProperties(&_vk_physical_device_memory_properties);
    }

    bool VKMemoryAllocator::allocate_buffer_memory(BufferInterface* buffer_) const
    {
        VKBuffer* buffer = check_cast<VKBuffer*>(buffer_);

        vk::MemoryRequirements vk_memory_requirements;
        _context->device.getBufferMemoryRequirements(buffer->vk_buffer, &vk_memory_requirements);

        vk::MemoryPropertyFlags vk_memory_property_flags{};

        switch(buffer->desc.cpu_access)
        {
        case CpuAccessMode::None:
            vk_memory_property_flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
            break;
        case CpuAccessMode::Read:
            vk_memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCached;
            break;
        case CpuAccessMode::Write:
            vk_memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible;
            break;
        }

        ReturnIfFalse(allocate_memory(
            &buffer->vk_device_memory, 
            vk_memory_requirements, 
            vk_memory_property_flags, 
            nullptr, 
            buffer->vk_buffer
        ));

        _context->device.bindBufferMemory(buffer->vk_buffer, buffer->vk_device_memory, 0);

        return true;
    }

    void VKMemoryAllocator::free_buffer_memory(BufferInterface* buffer) const
    {
        free_memory(check_cast<VKBuffer*>(buffer)->vk_device_memory);
    }

    bool VKMemoryAllocator::allocate_texture_memory(TextureInterface* texture_) const
    {
        VKTexture* texture = check_cast<VKTexture*>(texture_);

        vk::MemoryRequirements vk_memory_requirements;
        _context->device.getImageMemoryRequirements(texture->vk_image, &vk_memory_requirements);

        const vk::MemoryPropertyFlags vk_memory_property_flags = vk::MemoryPropertyFlagBits::eDeviceLocal;

        ReturnIfFalse(allocate_memory(
            &texture->vk_device_memory, 
            vk_memory_requirements, 
            vk_memory_property_flags, 
            texture->vk_image, 
            nullptr
        ));

        _context->device.bindImageMemory(texture->vk_image, texture->vk_device_memory, 0);

        return true;
    }

    void VKMemoryAllocator::free_texture_memory(TextureInterface* texture) const
    {
        free_memory(check_cast<VKTexture*>(texture)->vk_device_memory);
    }

    bool VKMemoryAllocator::allocate_memory(
        vk::DeviceMemory* vk_device_memory,
        vk::MemoryRequirements vk_memory_requirements,
        vk::MemoryPropertyFlags vk_memory_property_flags,
        VkImage vk_dedicated_image,
        VkBuffer vk_dedicated_buffer
    ) const
    {
        uint32_t memory_type_index = 0;
        for(; memory_type_index < _vk_physical_device_memory_properties.memoryTypeCount; memory_type_index++)
        {
            if (
                (vk_memory_requirements.memoryTypeBits & (1 << memory_type_index)) &&
                ((_vk_physical_device_memory_properties.memoryTypes[memory_type_index].propertyFlags & vk_memory_property_flags) == vk_memory_property_flags))
            {
                break;
            }
        }
        ReturnIfFalse(memory_type_index != _vk_physical_device_memory_properties.memoryTypeCount);

        vk::MemoryDedicatedAllocateInfo vk_dedicated_allocate_info{};
        vk_dedicated_allocate_info.image = vk_dedicated_image;
        vk_dedicated_allocate_info.buffer = vk_dedicated_buffer;
        vk_dedicated_allocate_info.pNext = nullptr;

        vk::MemoryAllocateInfo vk_memroy_allocate_info{};
        vk_memroy_allocate_info.pNext = vk_dedicated_image || vk_dedicated_buffer ? &vk_dedicated_allocate_info : nullptr;
        vk_memroy_allocate_info.allocationSize = vk_memory_requirements.size;
        vk_memroy_allocate_info.memoryTypeIndex = memory_type_index;

        return _context->device.allocateMemory(&vk_memroy_allocate_info, _context->allocation_callbacks, vk_device_memory) == vk::Result::eSuccess;
    }

    void VKMemoryAllocator::free_memory(vk::DeviceMemory resource) const
    {
        _context->device.freeMemory(resource, _context->allocation_callbacks);
    }

}