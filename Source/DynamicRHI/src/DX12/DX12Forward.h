#ifndef RHI_D3D12_FORWARD_H
#define RHI_D3D12_FORWARD_H

#include <d3d12.h>
#include "../../../Core/include/ComCli.h"
#include "../../../Core/include/ComRoot.h"
#include "../../include/Resource.h"
#include <wrl.h>

namespace FTS
{
    // Forward declaration
    class FDX12Heap;

    static constexpr UINT32 gdwInvalidViewIndex = static_cast<UINT32>(-1);
    static constexpr D3D12_RESOURCE_STATES gResourceStateUnknown = D3D12_RESOURCE_STATES(static_cast<UINT16>(~0u));


    void WaitForFence(ID3D12Fence* pFence, UINT64 stFenceValue, HANDLE Event);
    
    /**
     * @brief       https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/subresources?redirectedfrom=MSDN
     * 
     * @param       dwMipLevel 
     * @param       dwArraySlice 
     * @param       dwPlaneSlice 
     * @param       dwMipLevels 
     * @param       dwArraySize 
     * @return      UINT32 
     */
    inline UINT32 CalcTextureSubresource(
        UINT32 dwMipLevel,
        UINT32 dwArraySlice,
        UINT32 dwPlaneSlice,
        UINT32 dwMipLevels,
        UINT32 dwArraySize
    )
    {
        return dwMipLevel + (dwArraySlice * dwMipLevels) + (dwPlaneSlice * dwMipLevels * dwArraySize);
    }

    inline UINT32 CalcTextureSubresource(UINT32 dwMipLevel, UINT32 dwArraySlice, const FTextureDesc& crDesc)
    {
        return dwMipLevel + dwArraySlice * crDesc.dwMipLevels;
    }

    struct FDX12Context
    {
        Microsoft::WRL::ComPtr<ID3D12Device> pDevice;
    
        Microsoft::WRL::ComPtr<ID3D12QueryHeap> pTimerQueryHeap;
        TComPtr<IBuffer> pTimerQueryResolveBuffer;  // FD3D12Buffer;
    };


    class FDX12Heap :
        public TComObjectRoot<FComMultiThreadModel>,
        public IHeap
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12Heap)
            INTERFACE_ENTRY(IID_IHeap, IHeap)
        END_INTERFACE_MAP

        FDX12Heap(const FDX12Context* cpContext, const FHeapDesc& crHeapDesc);

        BOOL Initialize();

        // IHeap
        FHeapDesc GetDesc() const override { return m_Desc; }

    public:
        Microsoft::WRL::ComPtr<ID3D12Heap> m_pD3D12Heap;

    private:
        const FDX12Context* m_cpContext = nullptr;
        FHeapDesc m_Desc{};
    };
    
}

















#endif