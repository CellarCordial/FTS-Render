#ifndef RHI_DYNAMIC_H
#define RHI_DYNAMIC_H

#include "device.h"
#include <d3d12.h>
#include <vulkan/vulkan.h>


namespace fantasy
{
    //////////////////////////////////////////////////////////////////////////////////////////////
    // D3D12
    /////////////////////////////////////////////////////////////////////////////////////////////
    
    struct DX12DeviceDesc
    {
        ID3D12Device* d3d12_device = nullptr;
        ID3D12CommandQueue* d3d12_graphics_cmd_queue = nullptr;
        ID3D12CommandQueue* d3d12_compute_cmd_queue = nullptr;
        ID3D12CommandQueue* d3d12_copy_cmd_queue = nullptr;

        uint32_t rtv_heap_size = 1024;
        uint32_t dsv_heap_size = 1024;
        uint32_t srv_heap_size = 16384;
        uint32_t sampler_heap_size = 1024;
        uint32_t max_timer_queries = 256;
    };

    DeviceInterface* CreateDevice(const DX12DeviceDesc& desc);
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    // Vulkan
    /////////////////////////////////////////////////////////////////////////////////////////////


    struct VKDeviceDesc
    {
        VkInstance instance;
        VkPhysicalDevice physical_device;
        VkDevice device;

        VkQueue graphics_queue;
        uint32_t graphics_queue_index = -1;
        VkQueue transfer_queue;
        uint32_t transfer_queue_index = -1;
        VkQueue compute_queue;
        uint32_t compute_queue_index = -1;

        const char** instance_extensions = nullptr;
        size_t num_instance_extensions = 0;
        
        const char** device_extensions = nullptr;
        size_t num_device_extensions = 0;

        uint32_t max_timer_queries = 256;

        bool buffer_device_address_supported = false;
    };

    DeviceInterface* VKDeviceDesc(const DX12DeviceDesc& desc);
}























#endif