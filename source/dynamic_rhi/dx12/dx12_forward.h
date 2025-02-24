#ifndef RHI_D3D12_FORWARD_H
#define RHI_D3D12_FORWARD_H

#include <d3d12.h>
#include "../../core/tools/stack_array.h"
#include "../forward.h"
#include <wrl.h>

namespace fantasy
{
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
        Microsoft::WRL::ComPtr<ID3D12Device8> device8;

        Microsoft::WRL::ComPtr<ID3D12CommandSignature> draw_indirect_signature;
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> draw_indexed_indirect_signature;
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> dispatch_indirect_signature;
    };
    

    class DX12Heap : public HeapInterface
    {
    public:
        DX12Heap(const DX12Context* context, const HeapDesc& desc);

        bool initialize();

        const HeapDesc& get_desc() const override { return _desc; }

    public:
        Microsoft::WRL::ComPtr<ID3D12Heap> d3d12_heap;

    private:
        const DX12Context* _context = nullptr;
        HeapDesc _desc{};
    };

    bool wait_for_fence(ID3D12Fence* d3d12_fence, uint64_t fence_value);
}

















#endif