#ifndef RHI_D3D12_DESCRIPTOR_HEAP_H
#define RHI_D3D12_DESCRIPTOR_HEAP_H


#include <cstdint>
#include <mutex>

#include "dx12_forward.h"
#include "../../core/tools/bit_allocator.h"

namespace fantasy
{
    class DX12DescriptorHeap
    {
    public:
        explicit DX12DescriptorHeap(const DX12Context* context, D3D12_DESCRIPTOR_HEAP_TYPE d3d12_heap_type, uint32_t descriptor_count);

        bool initialize();

        uint32_t allocate_descriptor();
        uint32_t allocate_descriptors(uint32_t count);
        
        void release_descriptor(uint32_t index);
        void release_descriptors(uint32_t base_index, uint32_t count);
        
        void copy_to_shader_visible_heap(uint32_t descriptor_index, uint32_t count = 1u);
        
        D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(uint32_t index) const;
        D3D12_CPU_DESCRIPTOR_HANDLE get_shader_visible_cpu_handle(uint32_t index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(uint32_t index) const;
        
        ID3D12DescriptorHeap* get_shader_visible_heap() const;
        
    private:
        bool resize_heap(uint32_t size);
        
    private:
        const DX12Context* _context;
        
        D3D12_DESCRIPTOR_HEAP_TYPE _d3d12_heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _d3d12_descriptor_heap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _d3d12_shader_visible_descriptor_heap;
        
        D3D12_CPU_DESCRIPTOR_HANDLE _d3d12_start_cpu_handle = { 0 };

        D3D12_GPU_DESCRIPTOR_HANDLE _d3d12_start_gpu_handle = { 0 };
        D3D12_CPU_DESCRIPTOR_HANDLE _d3d12_start_shader_visible_cpu_handle = { 0 };
        
        std::mutex _mutex;

        uint32_t _descriptor_stride = 0;
        uint32_t _descriptor_count = 0;
       
        uint32_t _descriptor_search_start_index = 0;
       
        BitSetAllocator _allocated_descriptors;
    };

    class DX12DescriptorManager
    {
    public:
        explicit DX12DescriptorManager(const DX12Context* context);

    public:
        DX12DescriptorHeap render_target_heap;
        DX12DescriptorHeap depth_stencil_heap;
        DX12DescriptorHeap shader_resource_heap;
        DX12DescriptorHeap sampler_heap;

        static const uint32_t rtv_heap_size = 1024;
        static const uint32_t dsv_heap_size = 1024;
        static const uint32_t srv_heap_size = 16384;
        static const uint32_t sampler_heap_size = 1024;

    private:
        const DX12Context* _context;
    };
}




#endif