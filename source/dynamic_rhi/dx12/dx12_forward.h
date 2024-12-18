#ifndef RHI_D3D12_FORWARD_H
#define RHI_D3D12_FORWARD_H

#include <d3d12.h>
#include "../resource.h"
#include <memory>
#include "../../core/tools/stack_array.h"
#include <wrl.h>

namespace fantasy
{
    void wait_for_fence(ID3D12Fence* fence, uint64_t fence_value, HANDLE event);

    struct DX12SliceRegion
    {
        uint64_t offset;
        uint64_t size;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    };

    using D3D12ViewportArray = StackArray<D3D12_VIEWPORT, MAX_VIEWPORTS>;
    using D3D12ScissorRectArray = StackArray<D3D12_RECT, MAX_VIEWPORTS>;

    struct DX12ViewportState
    {
        D3D12ViewportArray viewports;
        D3D12ScissorRectArray scissor_rects;
    };

    struct DX12Context
    {
        Microsoft::WRL::ComPtr<ID3D12Device> device;
        Microsoft::WRL::ComPtr<ID3D12Device5> device5;

        Microsoft::WRL::ComPtr<ID3D12QueryHeap> timer_query_heap;
        std::unique_ptr<BufferInterface> timer_query_resolve_buffer;
    };


    class DX12Heap : public HeapInterface
    {
    public:
        DX12Heap(const DX12Context* context, const HeapDesc& desc);

        bool initialize();

        // HeapInterface.
        HeapDesc get_desc() const override { return _desc; }

    public:
        Microsoft::WRL::ComPtr<ID3D12Heap> _d3d12_heap;

    private:
        const DX12Context* _context = nullptr;
        HeapDesc _desc{};
    };

    struct DX12RootSignature;
    
}

















#endif