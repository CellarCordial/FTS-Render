#include "DX12Forward.h"

namespace FTS
{

    void WaitForFence(ID3D12Fence* pFence, UINT64 stFenceValue, HANDLE Event)
    {
        if (pFence->GetCompletedValue() < stFenceValue)
        {
            ResetEvent(Event);
            pFence->SetEventOnCompletion(stFenceValue, Event);
            WaitForSingleObject(Event, INFINITE);
        }
    }

    FDX12Heap::FDX12Heap(const FDX12Context* cpContext, const FHeapDesc& crHeapDesc) :
        m_cpContext(cpContext), m_Desc(crHeapDesc)
    {
    }

    BOOL FDX12Heap::Initialize()
    {
        D3D12_HEAP_DESC HeapDesc{};
        HeapDesc.Alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
        HeapDesc.SizeInBytes = m_Desc.stCapacity;
        HeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        HeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapDesc.Properties.CreationNodeMask = 1; // No multi GPU support so far.  
        HeapDesc.Properties.VisibleNodeMask = 1;
        HeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;

        
        switch (m_Desc.Type)
        {
        case EHeapType::Default:
            HeapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
            break;
        case EHeapType::Upload:
            HeapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
            break;
        case EHeapType::Readback:
            HeapDesc.Properties.Type = D3D12_HEAP_TYPE_READBACK;
            break;
        default:
            assert(!"Invalid enum. ");
        }

        if (FAILED(m_cpContext->pDevice->CreateHeap(&HeapDesc, IID_PPV_ARGS(m_pD3D12Heap.GetAddressOf()))))
        {
            LOG_ERROR("Create D3D12 heap failed.");
            return false;
        }

        return true;
    }


}
