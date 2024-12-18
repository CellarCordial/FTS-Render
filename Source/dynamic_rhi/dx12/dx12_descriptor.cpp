#include "dx12_descriptor.h"
#include "../../core/tools/log.h"
#include <d3d12.h>

namespace fantasy
{
    DX12DescriptorHeap::DX12DescriptorHeap(const DX12Context& context) : _context(context)
    {
    }

    bool DX12DescriptorHeap::initialize(D3D12_DESCRIPTOR_HEAP_TYPE heap_type, uint32_t descriptor_count, bool is_shader_visible)
    {
        _heap_type = heap_type;
        _descriptor_count = descriptor_count;
        
        _d3d12_descriptor_heap = nullptr;
        _d3d12_shader_visible_descriptor_heap = nullptr;
        _descriptor_stride = _context.device->GetDescriptorHandleIncrementSize(_heap_type);
        _allocated_descriptors.resize(_descriptor_count);

        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = _heap_type;
        desc.NumDescriptors = _descriptor_count;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(_context.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_d3d12_descriptor_heap.GetAddressOf())))) return false;

        _start_cpu_handle = _d3d12_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
        if (heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
        {
            _start_cpu_handle.ptr += _descriptor_stride;
            _allocated_descriptors[0] = true;
        }

        if (is_shader_visible)
        {
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            if (FAILED(_context.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_d3d12_shader_visible_descriptor_heap.GetAddressOf()))))
                return false;

            _start_shader_visible_cpu_handle = _d3d12_shader_visible_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
            _start_gpu_handle = _d3d12_shader_visible_descriptor_heap->GetGPUDescriptorHandleForHeapStart();

            if (heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
            {
				_start_shader_visible_cpu_handle.ptr += _descriptor_stride;
                _start_gpu_handle.ptr += _descriptor_stride;
            }

        }

        return true;
    }

    void DX12DescriptorHeap::copy_to_shader_visible_heap(uint32_t descriptor_index, uint32_t count)
    {
        _context.device->CopyDescriptorsSimple(
            count, 
            get_shader_visible_cpu_handle(descriptor_index), 
            get_cpu_handle(descriptor_index), 
            _heap_type
        );
    }

    uint32_t DX12DescriptorHeap::allocate_descriptor()
    {
        return allocate_descriptors(1);
    }
    
    uint32_t DX12DescriptorHeap::allocate_descriptors(uint32_t count)
    {
        std::lock_guard lock_guard(_mutex);
        
        uint32_t found_index = 0;
        uint32_t free_count = 0;
        bool found = false;

        // 查找 count 个连续的未被 allocate 的描述符
        for (uint32_t ix = _descriptor_search_start_ndex; ix < _descriptor_count; ix++)
        {
            if (_allocated_descriptors[ix]) free_count = 0;
            else free_count += 1;

            if (free_count >= count)
            {
                found_index = ix - count + 1;
                found = true;
                break;
            }
        }

        if (!found)
        {
            found_index = _descriptor_count;

            if (!resize_heap(_descriptor_count + count))
            {
                LOG_ERROR("Failed to resize a descriptor heap.");
                return INVALID_SIZE_32;
            }
        }

        for (uint32_t ix = found_index; ix < found_index + count; ix++)
        {
            _allocated_descriptors[ix] = true;
        }

        _allocated_descriptor_count += count;
        _descriptor_search_start_ndex = found_index + count;

        return found_index;
    }
    
    void DX12DescriptorHeap::release_descriptor(uint32_t index)
    {
        return release_descriptors(index, 1);
    }

    void DX12DescriptorHeap::release_descriptors(uint32_t base_index, uint32_t count)
    {
        if (count == 0) return;
        std::lock_guard lock_guard(_mutex);

        for (uint32_t ix = base_index; ix < base_index + count; ix++)
        {
            if (!_allocated_descriptors[ix])
            {
                LOG_ERROR("Attempted to release an un-allocated descriptor");
                return;
            }

            _allocated_descriptors[ix] = false;
        }

        _allocated_descriptor_count -= count;

        if (_descriptor_search_start_ndex > base_index)
            _descriptor_search_start_ndex = base_index;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::get_cpu_handle(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE Handle = _start_cpu_handle;
        Handle.ptr += static_cast<uint64_t>(index) * _descriptor_stride;
        return Handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::get_shader_visible_cpu_handle(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE Handle = _start_shader_visible_cpu_handle;
        Handle.ptr += static_cast<uint64_t>(index) * _descriptor_stride;
        return Handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::get_gpu_handle(uint32_t index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE Handle = _start_gpu_handle;
        Handle.ptr += static_cast<uint64_t>(index) * _descriptor_stride;
        return Handle;
    }


    ID3D12DescriptorHeap* DX12DescriptorHeap::get_shader_visible_heap() const
    {
        return _d3d12_shader_visible_descriptor_heap.Get();
    }
    
        
    bool DX12DescriptorHeap::resize_heap(uint32_t size)
    {
        const uint32_t old_size = _descriptor_count;
        const uint32_t new_size = next_power_of_2(size);

        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pOldHeap = _d3d12_descriptor_heap;
        
        if (!initialize(_heap_type, new_size, _d3d12_shader_visible_descriptor_heap != nullptr)) return false;

        _context.device->CopyDescriptorsSimple(
            old_size,
            _start_cpu_handle,
            pOldHeap->GetCPUDescriptorHandleForHeapStart(),
            _heap_type
        );

        if (_d3d12_shader_visible_descriptor_heap != nullptr)
        {
            _context.device->CopyDescriptorsSimple(
                old_size,
                _start_shader_visible_cpu_handle,
                pOldHeap->GetCPUDescriptorHandleForHeapStart(),
                _heap_type
            );
        }

        return true;
    }

    DX12DescriptorHeaps::DX12DescriptorHeaps(const DX12Context& context, uint32_t max_timer_queries) :
        render_target_heap(context),
        depth_stencil_heap(context),
        shader_resource_heap(context),
        sampler_heap(context),
        time_queries(max_timer_queries, true),
        _context(context)
    {
    }

    uint8_t DX12DescriptorHeaps::get_format_plane_num(DXGI_FORMAT format)
    {
        uint8_t& plane_num = _dxgi_format_plane_num[format];

        if (plane_num == 0)
        {
            D3D12_FEATURE_DATA_FORMAT_INFO format_info{ format, 1 };
            if (FAILED(_context.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &format_info, sizeof(format_info))))
            {
                // Format not supported - store a special value in the cache to avoid querying later
                plane_num = 255;
            }
            else
            {
                // Format supported - store the plane count in the cache
                plane_num = format_info.PlaneCount;
            }
        }

        if (plane_num == 255) return 0;
        return plane_num;
    }
}
