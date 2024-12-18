#ifndef RHI_D3D12_DESCRIPTOR_HEAP_H
#define RHI_D3D12_DESCRIPTOR_HEAP_H


#include <mutex>
#include <unordered_map>
#include <vector>

#include "dx12_forward.h"
#include "../pipeline.h"
#include "../../core/tools/bit_allocator.h"

namespace fantasy
{
    struct DX12DescriptorHeaps;

    class DX12DescriptorHeap
    {
    public:
        explicit DX12DescriptorHeap(const DX12Context& context);

        bool initialize(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, uint32_t descriptor_count, bool bShaderVisible);

        void copy_to_shader_visible_heap(uint32_t descriptor_index, uint32_t count = 1u);

        uint32_t allocate_descriptor();
        uint32_t allocate_descriptors(uint32_t count);
        
        void release_descriptor(uint32_t index);
        void release_descriptors(uint32_t base_index, uint32_t count);
        
        D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(uint32_t index) const;
        D3D12_CPU_DESCRIPTOR_HANDLE get_shader_visible_cpu_handle(uint32_t index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(uint32_t index) const;
        
        ID3D12DescriptorHeap* get_shader_visible_heap() const;
        
    private:
        
        // 重新分配 DescriptorHeap 的大小
        bool resize_heap(uint32_t size);
        
    private:
        const DX12Context& _context;
        
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _d3d12_descriptor_heap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _d3d12_shader_visible_descriptor_heap;
        D3D12_DESCRIPTOR_HEAP_TYPE _heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        
        D3D12_CPU_DESCRIPTOR_HANDLE _start_cpu_handle = { 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE _start_gpu_handle = { 0 };
        D3D12_CPU_DESCRIPTOR_HANDLE _start_shader_visible_cpu_handle = { 0 };
        
        uint32_t _descriptor_stride = 0;
        uint32_t _descriptor_count = 0;
        std::vector<bool> _allocated_descriptors;
        uint32_t _descriptor_search_start_ndex = 0;
        uint32_t _allocated_descriptor_count = 0;
        std::mutex _mutex;
    
    };


    class DX12RootSignature
    {
    public:
        DX12RootSignature(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps);

        ~DX12RootSignature() noexcept;

        bool initialize(
            const BindingLayoutInterfaceArray& binding_layouts,
            bool allow_input_layout,
            const D3D12_ROOT_PARAMETER1* custom_parameters = nullptr,
            uint32_t custom_parameter_count = 0
        );

    public:
        Microsoft::WRL::ComPtr<ID3D12RootSignature> d3d12_root_signature;

        // uint32_t: RootParameter index. 
        std::vector<std::pair<BindingLayoutInterface*, uint32_t>> binding_layout_map;
        
        uint32_t push_constant_size = 0;
        uint32_t root_param_push_constant_index = ~0u;
        
        // The index in _descriptor_heaps->dx12_root_signatures. 
        uint64_t hash_index = 0;

    private:
        const DX12Context* _context;
        DX12DescriptorHeaps* _descriptor_heaps;
    };



    class DX12DescriptorHeaps
    {
    public:
        DX12DescriptorHeap render_target_heap;
        DX12DescriptorHeap depth_stencil_heap;
        DX12DescriptorHeap shader_resource_heap;
        DX12DescriptorHeap sampler_heap;
        BitSetAllocator time_queries;

        std::unordered_map<uint64_t, DX12RootSignature*> dx12_root_signatures;

        explicit DX12DescriptorHeaps(const DX12Context& context, uint32_t max_timer_queries);

        
        uint8_t get_format_plane_num(DXGI_FORMAT Format);
        
    private:
        const DX12Context& _context;
        std::unordered_map<DXGI_FORMAT, uint8_t> _dxgi_format_plane_num;
    };
}




#endif