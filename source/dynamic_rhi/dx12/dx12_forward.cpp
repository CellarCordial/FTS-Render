#include "dx12_forward.h"
#include "../../core/tools/log.h"

namespace fantasy
{
    DX12Heap::DX12Heap(const DX12Context* context, const HeapDesc& desc) :
        _context(context), _desc(desc)
    {
    }

    bool DX12Heap::initialize()
    {
        D3D12_HEAP_TYPE d3d12_heap_type;
        switch (_desc.type)
        {
        case HeapType::Default: d3d12_heap_type = D3D12_HEAP_TYPE_DEFAULT; break;
        case HeapType::Upload: d3d12_heap_type = D3D12_HEAP_TYPE_UPLOAD; break;
        case HeapType::Readback: d3d12_heap_type = D3D12_HEAP_TYPE_READBACK; break;
        default:
            assert(!"Invalid enum.");
        }
        
        // heap type 是 D3D12_HEAP_TYPE_CUSTOM 时 CPUPageProperty 和 MemoryPoolPreference 不得为 UNKNOWN.
        // 不是 D3D12_HEAP_TYPE_CUSTOM 时, CPUPageProperty 和 MemoryPoolPreference 必须为 UNKNOWN.
        // D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES = 0, 即 D3D12_HEAP_FLAG_NONE
        D3D12_HEAP_DESC desc{};
        desc.SizeInBytes = _desc.capacity;
        desc.Properties.Type = d3d12_heap_type;
        desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        desc.Properties.CreationNodeMask = 0;
        desc.Properties.VisibleNodeMask = 0;
        desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        desc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;

        return SUCCEEDED(_context->device->CreateHeap(&desc, IID_PPV_ARGS(d3d12_heap.GetAddressOf())));
    }

    bool wait_for_fence(ID3D12Fence* d3d12_fence, uint64_t fence_value)
    {
        if (d3d12_fence->GetCompletedValue() < fence_value)
        {
            HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            ReturnIfFalse(SUCCEEDED(d3d12_fence->SetEventOnCompletion(fence_value, event)));
            WaitForSingleObject(event, INFINITE);
            return CloseHandle(event);
        }
        return true;
    }
}
