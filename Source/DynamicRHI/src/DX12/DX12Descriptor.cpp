#include "DX12Descriptor.h"
#include <d3d12.h>

namespace FTS
{
    FDX12StaticDescriptorHeap::FDX12StaticDescriptorHeap(const FDX12Context& crContext) : m_crContext(crContext)
    {
    }

    
    BOOL FDX12StaticDescriptorHeap::Initialize(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, UINT32 dwDescriptorsNum, BOOL bShaderVisible)
    {
        m_HeapType = HeapType;
        m_dwDescriptorsNum = dwDescriptorsNum;
        
        m_pDescriptorHeap = nullptr;
        m_pShaderVisibleDescriptorHeap = nullptr;
        m_dwStride = m_crContext.pDevice->GetDescriptorHandleIncrementSize(m_HeapType);
        m_bAllocatedDescriptors.resize(m_dwDescriptorsNum);

        D3D12_DESCRIPTOR_HEAP_DESC Desc{};
        Desc.Type = m_HeapType;
        Desc.NumDescriptors = m_dwDescriptorsNum;
        Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(m_crContext.pDevice->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(m_pDescriptorHeap.GetAddressOf())))) return false;

        // Srv heap 需要预留第一个位置用于 imgui.

        m_StartCpuHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        if (m_HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) m_StartCpuHandle.ptr += m_dwStride;

        if (bShaderVisible)
        {
            Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            if (FAILED(m_crContext.pDevice->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(m_pShaderVisibleDescriptorHeap.GetAddressOf()))))
                return false;

            m_StartCpuHandleShaderVisible = m_pShaderVisibleDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            m_StartGpuHandleShaderVisible = m_pShaderVisibleDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
            if (m_HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
            {
                m_StartCpuHandleShaderVisible.ptr += m_dwStride;
                m_StartGpuHandleShaderVisible.ptr += m_dwStride;
            }
        }

        return true;
    }

    void FDX12StaticDescriptorHeap::CopyToShaderVisibleHeap(UINT32 dwDescriptorIndex, UINT32 dwNum)
    {
        m_crContext.pDevice->CopyDescriptorsSimple(
            dwNum, 
            GetCpuHandleShaderVisible(dwDescriptorIndex), 
            GetCpuHandle(dwDescriptorIndex), 
            m_HeapType
        );
    }

    UINT32 FDX12StaticDescriptorHeap::AllocateDescriptor()
    {
        return AllocateDescriptors(1);
    }
    
    UINT32 FDX12StaticDescriptorHeap::AllocateDescriptors(UINT32 dwNum)
    {
        std::lock_guard LockGuard(m_Mutex);
        
        UINT32 dwFoundIndex = 0;
        UINT32 dwFreeCount = 0;
        BOOL bFound = false;

        // 查找 dwNum 个连续的未被 allocate 的描述符
        for (UINT32 ix = m_dwDescriptorSearchStartIndex; ix < m_dwDescriptorsNum; ix++)
        {
            if (m_bAllocatedDescriptors[ix]) dwFreeCount = 0;
            else dwFreeCount += 1;

            if (dwFreeCount >= dwNum)
            {
                dwFoundIndex = ix - dwNum + 1;
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            dwFoundIndex = m_dwDescriptorsNum;

            if (!ResizeHeap(m_dwDescriptorsNum + dwNum))
            {
                LOG_ERROR("Failed to resize a descriptor heap.");
                return gdwInvalidViewIndex;
            }
        }

        for (UINT32 ix = dwFoundIndex; ix < dwFoundIndex + dwNum; ix++)
        {
            m_bAllocatedDescriptors[ix] = true;
        }

        m_NumAllocatedDescriptors += dwNum;
        m_dwDescriptorSearchStartIndex = dwFoundIndex + dwNum;

        return dwFoundIndex;
    }
    
    void FDX12StaticDescriptorHeap::ReleaseDescriptor(UINT32 dwIndex)
    {
        return ReleaseDescriptors(dwIndex, 1);
    }

    void FDX12StaticDescriptorHeap::ReleaseDescriptors(UINT32 dwBaseIndex, UINT32 dwNum)
    {
        std::lock_guard LockGuard(m_Mutex);

        if (dwNum == 0) return;

        for (UINT32 ix = dwBaseIndex; ix < dwBaseIndex + dwNum; ix++)
        {
            if (!m_bAllocatedDescriptors[ix])
            {
                LOG_ERROR("Attempted to release an un-allocated descriptor");
                return;
            }

            m_bAllocatedDescriptors[ix] = false;
        }

        m_NumAllocatedDescriptors -= dwNum;

        if (m_dwDescriptorSearchStartIndex > dwBaseIndex)
            m_dwDescriptorSearchStartIndex = dwBaseIndex;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE FDX12StaticDescriptorHeap::GetCpuHandle(UINT32 dwIndex) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE Handle = m_StartCpuHandle;
        Handle.ptr += static_cast<UINT64>(dwIndex) * m_dwStride;
        return Handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE FDX12StaticDescriptorHeap::GetCpuHandleShaderVisible(UINT32 dwIndex) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE Handle = m_StartCpuHandleShaderVisible;
        Handle.ptr += static_cast<UINT64>(dwIndex) * m_dwStride;
        return Handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE FDX12StaticDescriptorHeap::GetGpuHandleShaderVisible(UINT32 dwIndex) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE Handle = m_StartGpuHandleShaderVisible;
        Handle.ptr += static_cast<UINT64>(dwIndex) * m_dwStride;
        return Handle;
    }


    ID3D12DescriptorHeap* FDX12StaticDescriptorHeap::GetShaderVisibleHeap() const
    {
        return m_pShaderVisibleDescriptorHeap.Get();
    }
    
        
    BOOL FDX12StaticDescriptorHeap::ResizeHeap(UINT32 dwSize)
    {
        const UINT32 dwOldSize = m_dwDescriptorsNum;
        const UINT32 dwNewSize = NextPowerOf2(dwSize);

        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pOldHeap = m_pDescriptorHeap;
        
        if (!Initialize(m_HeapType, dwNewSize, m_pShaderVisibleDescriptorHeap != nullptr)) return false;

        m_crContext.pDevice->CopyDescriptorsSimple(
            dwOldSize,
            m_StartCpuHandle,
            pOldHeap->GetCPUDescriptorHandleForHeapStart(),
            m_HeapType
        );

        if (m_pShaderVisibleDescriptorHeap != nullptr)
        {
            m_crContext.pDevice->CopyDescriptorsSimple(
                dwOldSize,
                m_StartCpuHandleShaderVisible,
                pOldHeap->GetCPUDescriptorHandleForHeapStart(),
                m_HeapType
            );
        }

        return true;
    }

    FDX12DescriptorHeaps::FDX12DescriptorHeaps(const FDX12Context& crContext, UINT32 dwMaxTimerQueries) :
        RenderTargetHeap(crContext),
        DepthStencilHeap(crContext),
        ShaderResourceHeap(crContext),
        SamplerHeap(crContext),
        TimeQueries(dwMaxTimerQueries, true),
        m_crContext(crContext)
    {
    }

    UINT8 FDX12DescriptorHeaps::GetFormatPlaneNum(DXGI_FORMAT Format)
    {
        UINT8& rbtPlaneNum = m_DxgiFormatPlaneNumMap[Format];

        if (rbtPlaneNum == 0)
        {
            D3D12_FEATURE_DATA_FORMAT_INFO FormatInfo{ Format, 1 };
            if (FAILED(m_crContext.pDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &FormatInfo, sizeof(FormatInfo))))
            {
                // Format not supported - store a special value in the cache to avoid querying later
                rbtPlaneNum = 255;
            }
            else
            {
                // Format supported - store the plane count in the cache
                rbtPlaneNum = FormatInfo.PlaneCount;
            }
        }

        if (rbtPlaneNum == 255) return 0;
        return rbtPlaneNum;
    }
}
