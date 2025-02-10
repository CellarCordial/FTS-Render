#include "dx12_descriptor.h"
#include "../../core/tools/log.h"
#include "../../core/math/common.h"
#include <d3d12.h>
#include <intsafe.h>
#include <winerror.h>

namespace fantasy
{
    DX12DescriptorHeap::DX12DescriptorHeap(
        const DX12Context* context, 
        D3D12_DESCRIPTOR_HEAP_TYPE d3d12_heap_type, 
        uint32_t descriptor_count
    ) : 
        _context(context),
        _d3d12_heap_type(d3d12_heap_type),
        _descriptor_count(descriptor_count),
        _allocated_descriptors(descriptor_count)
    {
    }

    bool DX12DescriptorHeap::initialize()
    {
        bool is_shader_visible = false;
        if (_d3d12_heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || _d3d12_heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
        {
            is_shader_visible = true;
        }

        _descriptor_stride = _context->device->GetDescriptorHandleIncrementSize(_d3d12_heap_type);

        D3D12_DESCRIPTOR_HEAP_DESC d3d12_descriptor_heap_desc{};
        d3d12_descriptor_heap_desc.Type = _d3d12_heap_type;
        d3d12_descriptor_heap_desc.NumDescriptors = _descriptor_count;
        d3d12_descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        d3d12_descriptor_heap_desc.NodeMask = 0;

        ReturnIfFalse(SUCCEEDED(_context->device->CreateDescriptorHeap(
            &d3d12_descriptor_heap_desc, 
            IID_PPV_ARGS(_d3d12_descriptor_heap.GetAddressOf())
        )));

        _d3d12_start_cpu_handle = _d3d12_descriptor_heap->GetCPUDescriptorHandleForHeapStart();

        if (is_shader_visible)
        {
            d3d12_descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            ReturnIfFalse(SUCCEEDED(_context->device->CreateDescriptorHeap(
                &d3d12_descriptor_heap_desc, 
                IID_PPV_ARGS(_d3d12_shader_visible_descriptor_heap.GetAddressOf())
            )));

            _d3d12_start_gpu_handle = _d3d12_shader_visible_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
            _d3d12_start_shader_visible_cpu_handle = _d3d12_shader_visible_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
        }
        return true;
    }

    uint32_t DX12DescriptorHeap::allocate_descriptor()
    {
        return allocate_descriptors(1);
    }
    
    uint32_t DX12DescriptorHeap::allocate_descriptors(uint32_t count)
    {
        std::lock_guard lock(_mutex);
        
        uint32_t found_index = 0;
        uint32_t available_count = 0;
        bool found = false;

        for (uint32_t ix = _descriptor_search_start_index; ix < _descriptor_count; ix++)
        {
            if (_allocated_descriptors[ix]) available_count = 0;
            else available_count += 1;

            if (available_count == count)
            {
                found_index = ix - count + 1;
                found = true;
                break;
            }
        }

        if (!found)
        {
            found_index = _descriptor_count;
            ReturnIfFalse(resize_heap(_descriptor_count + count));
        }

        for (uint32_t ix = found_index; ix < found_index + count; ix++)
        {
            _allocated_descriptors.set_true(ix);
        }

        _descriptor_search_start_index = (found_index + count) % _descriptor_count;

        return found_index;
    }
    
    void DX12DescriptorHeap::release_descriptor(uint32_t index)
    {
        return release_descriptors(index, 1);
    }

    void DX12DescriptorHeap::release_descriptors(uint32_t base_index, uint32_t count)
    {
        if (count == 0) return;

        std::lock_guard lock(_mutex);

        for (uint32_t ix = base_index; ix < base_index + count; ix++)
        {
            if (!_allocated_descriptors[ix])
            {
                LOG_WARN("Attempted to release an un-allocated descriptor");
                return;
            }

            _allocated_descriptors.set_false(ix);
        }

        if (_descriptor_search_start_index > base_index)
        {
            _descriptor_search_start_index = base_index;
        }
    }

    void DX12DescriptorHeap::copy_to_shader_visible_heap(uint32_t descriptor_index, uint32_t count)
    {
        _context->device->CopyDescriptorsSimple(
            count, 
            get_shader_visible_cpu_handle(descriptor_index), 
            get_cpu_handle(descriptor_index), 
            _d3d12_heap_type
        );
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::get_cpu_handle(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE ret = _d3d12_start_cpu_handle;
        ret.ptr += index * _descriptor_stride;
        return ret;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::get_shader_visible_cpu_handle(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE ret = _d3d12_start_shader_visible_cpu_handle;
        ret.ptr += index * _descriptor_stride;
        return ret;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::get_gpu_handle(uint32_t index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE ret = _d3d12_start_gpu_handle;
        ret.ptr += index * _descriptor_stride;
        return ret;
    }

    ID3D12DescriptorHeap* DX12DescriptorHeap::get_shader_visible_heap() const
    {
        return _d3d12_shader_visible_descriptor_heap.Get();
    }
        
    bool DX12DescriptorHeap::resize_heap(uint32_t size)
    {
        const uint32_t old_size = _descriptor_count;
        const uint32_t new_size = next_power_of_2(size);

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> old_d3d12_descriptor_heap = _d3d12_descriptor_heap;
        
        _descriptor_count = new_size;
        _allocated_descriptors.resize(new_size);
        _d3d12_descriptor_heap.Reset();
        _d3d12_shader_visible_descriptor_heap.Reset();
        ReturnIfFalse(initialize());

        _context->device->CopyDescriptorsSimple(
            old_size,
            _d3d12_start_cpu_handle,
            old_d3d12_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
            _d3d12_heap_type
        );

        if (_d3d12_shader_visible_descriptor_heap != nullptr)
        {
            // shader visible heap 中的 descriptor 本身也都是从 non shader visible heap 中复制过去的.
            _context->device->CopyDescriptorsSimple(
                old_size,
                _d3d12_start_shader_visible_cpu_handle,
                old_d3d12_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
                _d3d12_heap_type
            );
        }

        return true;
    }


    DX12DescriptorManager::DX12DescriptorManager(const DX12Context* context) :
        render_target_heap(context, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, rtv_heap_size),
        depth_stencil_heap(context, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, dsv_heap_size),
        shader_resource_heap(context, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srv_heap_size),
        sampler_heap(context, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, sampler_heap_size),      
        _context(context)
    {
    }
}
