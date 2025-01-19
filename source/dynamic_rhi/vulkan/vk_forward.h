#ifndef RHI_VK_FORWARD_H
#define RHI_VK_FORWARD_H

#include <vulkan/vulkan.hpp>
#include "../forward.h"

namespace fantasy
{
    class VulkanAllocator;

    struct VKContext
    {
        vk::Instance instance;
        vk::PhysicalDevice physicalDevice;
        vk::Device device;
        vk::PipelineCache pipelineCache;

        struct {
            bool KHR_synchronization2 = false;
            bool KHR_maintenance1 = false;
            bool EXT_debug_report = false;
            bool EXT_debug_marker = false;
            bool KHR_acceleration_structure = false;
            bool buffer_device_address = false;
            bool KHR_ray_query = false;
            bool KHR_ray_tracing_pipeline = false;
        } extensions;

        vk::PhysicalDeviceProperties physical_device_properties;
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_pipeline_properties;
        vk::PhysicalDeviceAccelerationStructurePropertiesKHR accel_struct_properties;
    };

    class VKHeap: public HeapInterface
    {
    public:
        explicit VKHeap(const VKContext* context, const VulkanAllocator* allocator)
            : _context(context), _allocator(allocator)
        {
        }

        ~VKHeap() override;

        
        const HeapDesc& get_desc() const override { return desc; }

    private:
        const VKContext* _context;
        const VulkanAllocator* _allocator;
        
        HeapDesc desc;
    };
}




#endif