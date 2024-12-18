#include "dx12_forward.h"
#include "../../core/tools/log.h"

namespace fantasy
{
    void wait_for_fence(ID3D12Fence* fence, uint64_t fence_value, HANDLE event)
    {
        if (fence->GetCompletedValue() < fence_value)
        {
            ResetEvent(event);
            fence->SetEventOnCompletion(fence_value, event);
            WaitForSingleObject(event, INFINITE);
        }
    }

    DX12Heap::DX12Heap(const DX12Context* context, const HeapDesc& desc) :
        _context(context), _desc(desc)
    {
    }

    bool DX12Heap::initialize()
    {
        D3D12_HEAP_DESC desc{};
        desc.Alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
        desc.SizeInBytes = _desc.capacity;
        desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        desc.Properties.CreationNodeMask = 1; // No multi GPU support so far.  
        desc.Properties.VisibleNodeMask = 1;
        desc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;

        
        switch (_desc.type)
        {
        case HeapType::Default:
            desc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
            break;
        case HeapType::Upload:
            desc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
            break;
        case HeapType::Readback:
            desc.Properties.Type = D3D12_HEAP_TYPE_READBACK;
            break;
        default:
            assert(!"invalid enum. ");
        }

        if (FAILED(_context->device->CreateHeap(&desc, IID_PPV_ARGS(_d3d12_heap.GetAddressOf()))))
        {
            LOG_ERROR("Create D3D12 heap failed.");
            return false;
        }

        return true;
    }


}
