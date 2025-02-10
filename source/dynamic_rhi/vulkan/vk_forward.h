#ifndef RHI_VK_FORWARD_H
#define RHI_VK_FORWARD_H

#include <vulkan/vulkan.hpp>
#include "../forward.h"

namespace fantasy
{
    class VKMemoryAllocator;

    static const uint32_t volatile_constant_buffer_max_version = 3;
    static const uint32_t texture_tile_byte_size = 65536;

    struct VKContext
    {
        vk::Instance vk_instance;
        vk::PhysicalDevice vk_physical_device;
        vk::PipelineCache vk_pipeline_cache;
        vk::PhysicalDeviceProperties vk_physical_device_properties;
        
        vk::Device device;
        vk::AllocationCallbacks* allocation_callbacks = nullptr;

        void name_object(
            const void* object, 
            const vk::ObjectType object_type,
            const char* name
        ) const
        {
            if (!(name && *name && object)) return;

            vk::DebugUtilsObjectNameInfoEXT info{};
            info.objectType = object_type;
            info.objectHandle = reinterpret_cast<uint64_t>(object);
            info.pObjectName = name;
            device.setDebugUtilsObjectNameEXT(info);
        }
    };

    class VKHeap: public HeapInterface
    {
    public:
        explicit VKHeap(const VKContext* context, const VKMemoryAllocator* allocator, const HeapDesc& desc_)
            : _context(context), _allocator(allocator), desc(desc_)
        {
        }

        ~VKHeap() override;

        bool initialize() { return true; }

        const HeapDesc& get_desc() const override { return desc; }

    public:
        HeapDesc desc;

        vk::DeviceMemory vk_device_memory;

    private:
        const VKContext* _context;
        const VKMemoryAllocator* _allocator;
    };


    bool wait_for_semaphore(const VKContext* context, vk::Semaphore vk_semaphore, uint64_t semaphore_value);

    static vk::Viewport vk_viewport_with_dx_coords(const Viewport& v)
    {
        return vk::Viewport(v.min_x, v.max_y, v.max_x - v.min_x, -(v.max_y - v.min_y), v.min_z, v.max_z);
    }
}




#endif