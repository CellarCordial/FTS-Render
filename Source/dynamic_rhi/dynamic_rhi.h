#ifndef RHI_DYNAMIC_H
#define RHI_DYNAMIC_H

#include "device.h"
#include <d3d12.h>


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





}























#endif