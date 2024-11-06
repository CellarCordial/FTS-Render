#ifndef RHI_D3D12_DESCRIPTOR_HEAP_H
#define RHI_D3D12_DESCRIPTOR_HEAP_H


#include <mutex>
#include <unordered_map>
#include <vector>
#include "../../include/DynamicRHI.h"

#include "DX12Forward.h"
#include "../Utils.h"

namespace FTS
{
    class FDX12StaticDescriptorHeap
    {
    public:
        explicit FDX12StaticDescriptorHeap(const FDX12Context& crContext);

        BOOL Initialize(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, UINT32 dwDescriptorsNum, BOOL bShaderVisible);

        void CopyToShaderVisibleHeap(UINT32 dwDescriptorIndex, UINT32 dwNum = 1u);

        UINT32 AllocateDescriptor();
        UINT32 AllocateDescriptors(UINT32 dwNum);
        
        void ReleaseDescriptor(UINT32 dwIndex);
        void ReleaseDescriptors(UINT32 dwBaseIndex, UINT32 dwNum);
        
        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(UINT32 dwIndex) const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandleShaderVisible(UINT32 dwIndex) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandleShaderVisible(UINT32 dwIndex) const;
        
        ID3D12DescriptorHeap* GetShaderVisibleHeap() const;
        
    private:
        
        // 重新分配 DescriptorHeap 的大小
        BOOL ResizeHeap(UINT32 dwSize);
        
    private:
        const FDX12Context& m_crContext;
        
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pShaderVisibleDescriptorHeap;
        D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        
        D3D12_CPU_DESCRIPTOR_HANDLE m_StartCpuHandle = { 0 };
        D3D12_CPU_DESCRIPTOR_HANDLE m_StartCpuHandleShaderVisible = { 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE m_StartGpuHandleShaderVisible = { 0 };
        
        UINT32 m_dwStride = 0;
        UINT32 m_dwDescriptorsNum = 0;
        std::vector<BOOL> m_bAllocatedDescriptors;
        UINT32 m_dwDescriptorSearchStartIndex = 0;
        UINT32 m_NumAllocatedDescriptors = 0;
        std::mutex m_Mutex;
    
    };


    class FDX12DescriptorHeaps
    {
    public:

        FDX12StaticDescriptorHeap RenderTargetHeap;
        FDX12StaticDescriptorHeap DepthStencilHeap;
        FDX12StaticDescriptorHeap ShaderResourceHeap;
        FDX12StaticDescriptorHeap SamplerHeap;
        FBitSetAllocator TimeQueries;

        std::unordered_map<UINT64, IDX12RootSignature*> RootSignatureMap;


        explicit FDX12DescriptorHeaps(const FDX12Context& crContext, UINT32 dwMaxTimerQueries);

        
        UINT8 GetFormatPlaneNum(DXGI_FORMAT Format);
        
    private:

        const FDX12Context& m_crContext;
        std::unordered_map<DXGI_FORMAT, UINT8> m_DxgiFormatPlaneNumMap;
    };
}




#endif