#ifndef RHI_DYNAMIC_H
#define RHI_DYNAMIC_H

#include "binding.h"
#include "device.h"
#include "resource.h"
#include <d3d12.h>
#include <vulkan/vulkan.hpp>
#include <wrl/client.h>


namespace fantasy
{
    //////////////////////////////////////////////////////////////////////////////////////////////
    // D3D12
    /////////////////////////////////////////////////////////////////////////////////////////////
    
    struct DX12DeviceDesc
    {
        Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_graphics_cmd_queue;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_compute_cmd_queue;
    };

    DeviceInterface* CreateDevice(const DX12DeviceDesc& desc);

    D3D12_GPU_DESCRIPTOR_HANDLE get_texture_gpu_handle(
        DeviceInterface* device, 
        TextureInterface* texture, 
        const TextureSubresourceSet &subresource, 
        ResourceViewType view_type
    );
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    // Vulkan
    /////////////////////////////////////////////////////////////////////////////////////////////


    struct VKDeviceDesc
    {
        vk::Instance vk_instance;
        vk::PhysicalDevice vk_physical_device;
        vk::Device vk_device;

        VkQueue vk_graphics_queue;
        uint32_t graphics_queue_index = -1;
        VkQueue vk_compute_queue;
        uint32_t compute_queue_index = -1;
        
        vk::AllocationCallbacks* vk_allocation_callbacks = nullptr;
    };

    DeviceInterface* CreateDevice(const VKDeviceDesc& desc);
}























#endif