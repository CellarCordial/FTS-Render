#include "DX12Resource.h"
#include "DX12Converts.h"
#include "DX12Forward.h"
#include <combaseapi.h>
#include <d3d12.h>
#include <dxgiformat.h>
#include <winerror.h>

namespace FTS 
{
    FDX12Texture::FDX12Texture(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps, const FTextureDesc& crDesc) :
        m_cpContext(cpContext), m_pDescriptorHeaps(pDescriptorHeaps), m_Desc(crDesc)
    {
    }

    FDX12Texture::~FDX12Texture() noexcept
    {
        for (const auto& Pair : m_RTVIndexMap) m_pDescriptorHeaps->RenderTargetHeap.ReleaseDescriptor(Pair.second);
        for (const auto& Pair : m_DSVIndexMap) m_pDescriptorHeaps->DepthStencilHeap.ReleaseDescriptor(Pair.second);
        for (const auto& Pair : m_SRVIndexMap) m_pDescriptorHeaps->ShaderResourceHeap.ReleaseDescriptor(Pair.second);
        for (const auto& Pair : m_UAVIndexMap) m_pDescriptorHeaps->ShaderResourceHeap.ReleaseDescriptor(Pair.second);

        for (auto Index : m_ClearMipLevelUAVIndices) m_pDescriptorHeaps->ShaderResourceHeap.ReleaseDescriptor(Index);
    }

    BOOL FDX12Texture::Initialize()
    {
        if (!m_Desc.bIsRenderTarget && !m_Desc.bIsDepthStencil && m_Desc.bUseClearValue)
        {
            LOG_ERROR("ClearValue must not be used when texture is neither render target nor depth stencil.");
            return false;
        }

        m_D3D12ResourceDesc = ConvertTextureDesc(m_Desc);

        // If the resource is created in bindTextureMemory. 
        if (m_Desc.bIsVirtual) return true;

        D3D12_HEAP_PROPERTIES D3D12HeapProperties{};
        D3D12HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_CLEAR_VALUE D3D12ClearValue = ConvertClearValue(m_Desc);

        if (FAILED(m_cpContext->pDevice->CreateCommittedResource(
            &D3D12HeapProperties, 
            D3D12_HEAP_FLAG_NONE, 
            &m_D3D12ResourceDesc, 
            ConvertResourceStates(m_Desc.InitialState), 
            m_Desc.bUseClearValue ? &D3D12ClearValue : nullptr, 
            IID_PPV_ARGS(m_pD3D12Resource.GetAddressOf())
        )))
        {
            LOG_ERROR("Call to ID3D12Device::CreateCommittedResource failed.");
            return false;
        }

		m_pD3D12Resource->SetName(StringToWString(m_Desc.strName).c_str());

        if (m_Desc.bIsUAV)
        {
            m_ClearMipLevelUAVIndices.resize(m_Desc.dwMipLevels);
            std::fill(m_ClearMipLevelUAVIndices.begin(), m_ClearMipLevelUAVIndices.end(), gdwInvalidViewIndex);
        }

        m_btPlaneCount = m_pDescriptorHeaps->GetFormatPlaneNum(m_D3D12ResourceDesc.Format);

        return true;
    }


    SIZE_T FDX12Texture::GetNativeView(
        EViewType ViewType,
        EFormat Format,
        FTextureSubresourceSet Subresource,
        ETextureDimension Dimension,
        BOOL bIsReadOnlyDSV
    )
    {
        switch (ViewType)
        {
        case EViewType::DX12_GPU_ShaderResourceView:
            {
                FDX12TextureBindingKey BindingKey(Subresource, Format);
                UINT32 dwDescriptorIndex;
                auto Iter = m_SRVIndexMap.find(BindingKey);
                if (Iter == m_SRVIndexMap.end())    // If not found, then create one. 
                {
                    dwDescriptorIndex = m_pDescriptorHeaps->ShaderResourceHeap.AllocateDescriptor();
                    m_SRVIndexMap[BindingKey] = dwDescriptorIndex;
                    
                    const D3D12_CPU_DESCRIPTOR_HANDLE cCpuDescriptorHandle = 
                        m_pDescriptorHeaps->ShaderResourceHeap.GetCpuHandle(dwDescriptorIndex);

                    CreateSRV(cCpuDescriptorHandle.ptr, Format, Dimension, Subresource);
                    m_pDescriptorHeaps->ShaderResourceHeap.CopyToShaderVisibleHeap(dwDescriptorIndex);
                }
                else    // If found, then return it directly. 
                {
                    dwDescriptorIndex = Iter->second;
                }

                return m_pDescriptorHeaps->RenderTargetHeap.GetCpuHandle(dwDescriptorIndex).ptr;
            }
        case EViewType::DX12_GPU_UnorderedAccessView:
            {
                FDX12TextureBindingKey BindingKey(Subresource, Format);
                UINT32 dwDescriptorIndex;
                auto Iter = m_UAVIndexMap.find(BindingKey);
                if (Iter == m_UAVIndexMap.end())    // If not found, then create one. 
                {
                    dwDescriptorIndex = m_pDescriptorHeaps->ShaderResourceHeap.AllocateDescriptor();
                    m_UAVIndexMap[BindingKey] = dwDescriptorIndex;
                    
                    const D3D12_CPU_DESCRIPTOR_HANDLE cCpuDescriptorHandle = 
                        m_pDescriptorHeaps->ShaderResourceHeap.GetCpuHandle(dwDescriptorIndex);

                    CreateUAV(cCpuDescriptorHandle.ptr, Format, Dimension, Subresource);
                    m_pDescriptorHeaps->ShaderResourceHeap.CopyToShaderVisibleHeap(dwDescriptorIndex);
                }
                else    // If found, then return it directly. 
                {
                    dwDescriptorIndex = Iter->second;
                }

				return m_pDescriptorHeaps->ShaderResourceHeap.GetCpuHandle(dwDescriptorIndex).ptr;
            }
        case EViewType::DX12_RenderTargetView:
            {
                FDX12TextureBindingKey BindingKey(Subresource, Format);
                UINT32 dwDescriptorIndex;
                auto Iter = m_RTVIndexMap.find(BindingKey);
                if (Iter == m_RTVIndexMap.end())    // If not found, then create one. 
                {
                    dwDescriptorIndex = m_pDescriptorHeaps->RenderTargetHeap.AllocateDescriptor();
                    m_RTVIndexMap[BindingKey] = dwDescriptorIndex;
                    
                    const D3D12_CPU_DESCRIPTOR_HANDLE cCpuDescriptorHandle = 
                        m_pDescriptorHeaps->RenderTargetHeap.GetCpuHandle(dwDescriptorIndex);

                    CreateRTV(cCpuDescriptorHandle.ptr, Format, Subresource);
                }
                else    // If found, then return it directly. 
                {
                    dwDescriptorIndex = Iter->second;
                }

				return m_pDescriptorHeaps->RenderTargetHeap.GetCpuHandle(dwDescriptorIndex).ptr;
            }
        case EViewType::DX12_DepthStencilView:
            {
                FDX12TextureBindingKey BindingKey(Subresource, Format, bIsReadOnlyDSV);
                UINT32 dwDescriptorIndex;
                auto Iter = m_DSVIndexMap.find(BindingKey);
                if (Iter == m_DSVIndexMap.end())    // If not found, then create one. 
                {
                    dwDescriptorIndex = m_pDescriptorHeaps->DepthStencilHeap.AllocateDescriptor();
                    m_DSVIndexMap[BindingKey] = dwDescriptorIndex;
                    
                    const D3D12_CPU_DESCRIPTOR_HANDLE cCpuDescriptorHandle = 
                        m_pDescriptorHeaps->DepthStencilHeap.GetCpuHandle(dwDescriptorIndex);

                    CreateDSV(cCpuDescriptorHandle.ptr, Subresource, bIsReadOnlyDSV);
                }
                else    // If found, then return it directly. 
                {
                    dwDescriptorIndex = Iter->second;
                }

				return m_pDescriptorHeaps->DepthStencilHeap.GetCpuHandle(dwDescriptorIndex).ptr;
            }
        }
        return gdwInvalidViewIndex;
    }
    
    void FDX12Texture::CreateSRV(UINT64 stDescriptorAddress, EFormat Format, ETextureDimension Dimension, const FTextureSubresourceSet& crSubresourceSet)
    {
        FTextureSubresourceSet SubresourceSet = crSubresourceSet.Resolve(m_Desc, false);

        if (Dimension == ETextureDimension::Unknown) Dimension = m_Desc.Dimension;

        D3D12_SHADER_RESOURCE_VIEW_DESC ViewDesc{};
        ViewDesc.Format = GetDxgiFormatMapping(Format == EFormat::UNKNOWN ? m_Desc.Format : Format).SRVFormat;
        ViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        UINT32 dwPlaneSlice = 0;

        switch (Dimension)
        {
        case ETextureDimension::Texture1D: 
            ViewDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE1D;
            ViewDesc.Texture1D.MipLevels       = SubresourceSet.dwMipLevelsNum;
            ViewDesc.Texture1D.MostDetailedMip = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture1DArray: 
            ViewDesc.ViewDimension                  = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
            ViewDesc.Texture1DArray.ArraySize       = SubresourceSet.dwArraySlicesNum;
            ViewDesc.Texture1DArray.FirstArraySlice = SubresourceSet.dwBaseArraySliceIndex;
            ViewDesc.Texture1DArray.MipLevels       = SubresourceSet.dwMipLevelsNum;
            ViewDesc.Texture1DArray.MostDetailedMip = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture2D: 
            ViewDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
            ViewDesc.Texture2D.MipLevels       = SubresourceSet.dwMipLevelsNum;
            ViewDesc.Texture2D.MostDetailedMip = SubresourceSet.dwBaseMipLevelIndex;
            ViewDesc.Texture2D.PlaneSlice      = dwPlaneSlice;
            break;
        case ETextureDimension::Texture2DArray: 
            ViewDesc.ViewDimension                  = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            ViewDesc.Texture2DArray.ArraySize       = SubresourceSet.dwArraySlicesNum;
            ViewDesc.Texture2DArray.FirstArraySlice = SubresourceSet.dwBaseArraySliceIndex;
            ViewDesc.Texture2DArray.MipLevels       = SubresourceSet.dwMipLevelsNum;
            ViewDesc.Texture2DArray.MostDetailedMip = SubresourceSet.dwBaseMipLevelIndex;
            ViewDesc.Texture2DArray.PlaneSlice      = dwPlaneSlice;
            break;
        case ETextureDimension::TextureCube: 
            ViewDesc.ViewDimension               = D3D12_SRV_DIMENSION_TEXTURECUBE;
            ViewDesc.TextureCube.MipLevels       = SubresourceSet.dwMipLevelsNum;
            ViewDesc.TextureCube.MostDetailedMip = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::TextureCubeArray: 
            ViewDesc.ViewDimension                     = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
            ViewDesc.TextureCubeArray.MipLevels        = SubresourceSet.dwMipLevelsNum;
            ViewDesc.TextureCubeArray.MostDetailedMip  = SubresourceSet.dwBaseMipLevelIndex;
            ViewDesc.TextureCubeArray.First2DArrayFace = SubresourceSet.dwBaseArraySliceIndex;
            ViewDesc.TextureCubeArray.NumCubes         = SubresourceSet.dwArraySlicesNum / 6;
            break;
        case ETextureDimension::Texture2DMS: 
            ViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
            break;
        case ETextureDimension::Texture2DMSArray: 
            ViewDesc.ViewDimension                    = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
            ViewDesc.Texture2DMSArray.ArraySize       = SubresourceSet.dwArraySlicesNum;
            ViewDesc.Texture2DMSArray.FirstArraySlice = SubresourceSet.dwBaseArraySliceIndex;
            break;
        case ETextureDimension::Texture3D: 
            ViewDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE3D;
            ViewDesc.Texture3D.MipLevels       = SubresourceSet.dwMipLevelsNum;
            ViewDesc.Texture3D.MostDetailedMip = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Unknown: 
            assert(!"Invalid Enumeration Value");
            return;
        }

        m_cpContext->pDevice->CreateShaderResourceView(
            m_pD3D12Resource.Get(), 
            &ViewDesc, 
            D3D12_CPU_DESCRIPTOR_HANDLE{ stDescriptorAddress }
        );
    }

    void FDX12Texture::CreateUAV(UINT64 stDescriptorAddress, EFormat Format, ETextureDimension Dimension, const FTextureSubresourceSet& crSubresourceSet)
    {
        FTextureSubresourceSet SubresourceSet = crSubresourceSet.Resolve(m_Desc, true);     // uav 应该为单个 miplevel

        if (Dimension == ETextureDimension::Unknown) Dimension = m_Desc.Dimension;

        D3D12_UNORDERED_ACCESS_VIEW_DESC ViewDesc{};
        ViewDesc.Format = GetDxgiFormatMapping(Format == EFormat::UNKNOWN ? m_Desc.Format : Format).SRVFormat;

        switch (Dimension)
        {
        case ETextureDimension::Texture1D:
            ViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            ViewDesc.Texture1D.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture1DArray:
            ViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            ViewDesc.Texture1DArray.ArraySize = SubresourceSet.dwArraySlicesNum;
            ViewDesc.Texture1DArray.FirstArraySlice = SubresourceSet.dwBaseArraySliceIndex;
            ViewDesc.Texture1DArray.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture2D:
            ViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            ViewDesc.Texture2D.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture2DArray:
        case ETextureDimension::TextureCube:
        case ETextureDimension::TextureCubeArray:
            ViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            ViewDesc.Texture1DArray.ArraySize = SubresourceSet.dwArraySlicesNum;
            ViewDesc.Texture1DArray.FirstArraySlice = SubresourceSet.dwBaseArraySliceIndex;
            ViewDesc.Texture1DArray.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture3D:
            ViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            ViewDesc.Texture3D.FirstWSlice = 0;
            ViewDesc.Texture3D.WSize = m_Desc.dwDepth;
            ViewDesc.Texture3D.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture2DMS:
        case ETextureDimension::Texture2DMSArray:
        case ETextureDimension::Unknown:
            assert(!"Invalid Enumeration Value");
            return;
        }

        m_cpContext->pDevice->CreateUnorderedAccessView(
            m_pD3D12Resource.Get(), 
            nullptr,
            &ViewDesc, 
            D3D12_CPU_DESCRIPTOR_HANDLE{ stDescriptorAddress }
        );
    }

    void FDX12Texture::CreateRTV(UINT64 stDescriptorAddress, EFormat Format, const FTextureSubresourceSet& crSubresourceSet)
    {
        FTextureSubresourceSet SubresourceSet = crSubresourceSet.Resolve(m_Desc, true);

        D3D12_RENDER_TARGET_VIEW_DESC ViewDesc{};
        ViewDesc.Format = GetDxgiFormatMapping(Format == EFormat::UNKNOWN ? m_Desc.Format : Format).RTVFormat;

        switch (m_Desc.Dimension)
        {
        case ETextureDimension::Texture1D:
            ViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
            ViewDesc.Texture1D.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture1DArray:
            ViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
            ViewDesc.Texture1DArray.ArraySize = SubresourceSet.dwArraySlicesNum;
            ViewDesc.Texture1DArray.FirstArraySlice = SubresourceSet.dwBaseArraySliceIndex;
            ViewDesc.Texture1DArray.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture2D:
            ViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            ViewDesc.Texture1D.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture2DArray:
        case ETextureDimension::TextureCube:
        case ETextureDimension::TextureCubeArray:
            ViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            ViewDesc.Texture1DArray.ArraySize = SubresourceSet.dwArraySlicesNum;
            ViewDesc.Texture1DArray.FirstArraySlice = SubresourceSet.dwBaseArraySliceIndex;
            ViewDesc.Texture1DArray.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture2DMS:
            ViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            break;
        case ETextureDimension::Texture2DMSArray:
            ViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
            ViewDesc.Texture2DMSArray.ArraySize = SubresourceSet.dwArraySlicesNum;
            ViewDesc.Texture2DMSArray.FirstArraySlice = SubresourceSet.dwBaseArraySliceIndex;
            break;
        case ETextureDimension::Texture3D:
            ViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
            ViewDesc.Texture3D.FirstWSlice = SubresourceSet.dwBaseArraySliceIndex;
            ViewDesc.Texture3D.WSize = SubresourceSet.dwArraySlicesNum;
            ViewDesc.Texture3D.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Unknown:
            assert(!"Invalid Enumeration Value");
            return;
        }

        m_cpContext->pDevice->CreateRenderTargetView(
            m_pD3D12Resource.Get(), 
            &ViewDesc, 
            D3D12_CPU_DESCRIPTOR_HANDLE{ stDescriptorAddress }
        );
    }

    void FDX12Texture::CreateDSV(UINT64 stDescriptorAddress, const FTextureSubresourceSet& crSubresourceSet, BOOL bIsReadOnly)
    {
        FTextureSubresourceSet SubresourceSet = crSubresourceSet.Resolve(m_Desc, true);

        D3D12_DEPTH_STENCIL_VIEW_DESC ViewDesc{};
        ViewDesc.Format = GetDxgiFormatMapping(m_Desc.Format).RTVFormat;

        if (bIsReadOnly)
        {
            ViewDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
            if (ViewDesc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT || ViewDesc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT)
            {
                ViewDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
            }
        }

        switch (m_Desc.Dimension)
        {
        case ETextureDimension::Texture1D:
            ViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
            ViewDesc.Texture1D.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture1DArray:
            ViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
            ViewDesc.Texture1DArray.ArraySize = SubresourceSet.dwArraySlicesNum;
            ViewDesc.Texture1DArray.FirstArraySlice = SubresourceSet.dwBaseArraySliceIndex;
            ViewDesc.Texture1DArray.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture2D:
            ViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            ViewDesc.Texture1D.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture2DArray:
        case ETextureDimension::TextureCube:
        case ETextureDimension::TextureCubeArray:
            ViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            ViewDesc.Texture1DArray.ArraySize = SubresourceSet.dwArraySlicesNum;
            ViewDesc.Texture1DArray.FirstArraySlice = SubresourceSet.dwBaseArraySliceIndex;
            ViewDesc.Texture1DArray.MipSlice = SubresourceSet.dwBaseMipLevelIndex;
            break;
        case ETextureDimension::Texture2DMS:
            ViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
            break;
        case ETextureDimension::Texture2DMSArray:
            ViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
            ViewDesc.Texture2DMSArray.ArraySize = SubresourceSet.dwArraySlicesNum;
            ViewDesc.Texture2DMSArray.FirstArraySlice = SubresourceSet.dwBaseArraySliceIndex;
            break;
        case ETextureDimension::Texture3D:
        case ETextureDimension::Unknown:
            assert(!"Invalid Enumeration Value");
            return;
        }

        m_cpContext->pDevice->CreateDepthStencilView(
            m_pD3D12Resource.Get(), 
            &ViewDesc, 
            D3D12_CPU_DESCRIPTOR_HANDLE{ stDescriptorAddress }
        );
    }

    UINT32 FDX12Texture::GetClearMipLevelUAVIndex(UINT32 dwMipLevel)
    {
        assert(m_Desc.bIsUAV);

        UINT32 dwDescriptorIndex = m_ClearMipLevelUAVIndices[dwMipLevel];
        if (dwDescriptorIndex != gdwInvalidViewIndex) return dwDescriptorIndex;

        // If not found, then create one. 
        dwDescriptorIndex = m_pDescriptorHeaps->ShaderResourceHeap.AllocateDescriptor();

        assert(dwDescriptorIndex != gdwInvalidViewIndex);
        
        FTextureSubresourceSet SubresourceSet{ dwMipLevel, 1, 0, static_cast<UINT32>(-1) };
        CreateUAV(
            m_pDescriptorHeaps->ShaderResourceHeap.GetCpuHandle(dwDescriptorIndex).ptr,
            EFormat::UNKNOWN,
            ETextureDimension::Unknown,
            SubresourceSet
        );
        m_pDescriptorHeaps->ShaderResourceHeap.CopyToShaderVisibleHeap(dwDescriptorIndex);
        m_ClearMipLevelUAVIndices[dwMipLevel] = dwDescriptorIndex;

        return dwDescriptorIndex;
    }


    FMemoryRequirements FDX12Texture::GetMemoryRequirements()
    {
        D3D12_RESOURCE_ALLOCATION_INFO AllocationInfo = m_cpContext->pDevice->GetResourceAllocationInfo(1, 1, &m_D3D12ResourceDesc);

        return FMemoryRequirements{ AllocationInfo.Alignment, AllocationInfo.SizeInBytes };
    }


    BOOL FDX12Texture::BindMemory(IHeap* pHeap, UINT64 stOffset)
    {
        if (pHeap == nullptr || !m_Desc.bIsVirtual || m_pD3D12Resource == nullptr) return false;

        FDX12Heap* pDX12Heap = CheckedCast<FDX12Heap*>(pHeap);
        ReturnIfFalse(pDX12Heap->m_pD3D12Heap.Get() != nullptr);

        D3D12_CLEAR_VALUE D3D12ClearValue = ConvertClearValue(m_Desc);
        if (FAILED(m_cpContext->pDevice->CreatePlacedResource(
            pDX12Heap->m_pD3D12Heap.Get(), 
            stOffset, 
            &m_D3D12ResourceDesc, 
            ConvertResourceStates(m_Desc.InitialState), 
            m_Desc.bUseClearValue ? &D3D12ClearValue : nullptr,  
            IID_PPV_ARGS(m_pD3D12Resource.GetAddressOf())
        )))
        {
            LOG_ERROR("Bind texture memory failed.");
            return false;
        }

        m_pHeap = pHeap;
                
        if (m_Desc.bIsUAV)
        {
            m_ClearMipLevelUAVIndices.resize(m_Desc.dwMipLevels);
            std::fill(m_ClearMipLevelUAVIndices.begin(), m_ClearMipLevelUAVIndices.end(), gdwInvalidViewIndex);
        }

        m_btPlaneCount = m_pDescriptorHeaps->GetFormatPlaneNum(m_D3D12ResourceDesc.Format);

        return true;
    }


    FDX12Buffer::FDX12Buffer(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps, const FBufferDesc& crDesc) :
        m_cpContext(cpContext), m_pDescriptorHeaps(pDescriptorHeaps), m_Desc(crDesc)
    {
    }

    FDX12Buffer::~FDX12Buffer() noexcept
    {
        if (m_dwClearUAVIndex != gdwInvalidViewIndex)
        {
            m_pDescriptorHeaps->ShaderResourceHeap.ReleaseDescriptor(m_dwClearUAVIndex);
            m_dwClearUAVIndex = gdwInvalidViewIndex;
        }
    }

    BOOL FDX12Buffer::Initialize()
    {
        if (m_Desc.bIsConstantBuffer)
        {
            m_Desc.stByteSize = Align(m_Desc.stByteSize, static_cast<UINT64>(gdwConstantBufferOffsetSizeAlignment));
        }
        
        // Do not create any resources for volatile buffers.
        if (m_Desc.bIsVolatile) return true;

        m_D3D12ResourceDesc.SampleDesc = { 1, 0 };
        m_D3D12ResourceDesc.Alignment = 0;
        m_D3D12ResourceDesc.DepthOrArraySize = 1;
        m_D3D12ResourceDesc.Height = 1;
        m_D3D12ResourceDesc.Width = m_Desc.stByteSize;
        m_D3D12ResourceDesc.MipLevels = 1;
        m_D3D12ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        m_D3D12ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        m_D3D12ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (m_Desc.bCanHaveUAVs) m_D3D12ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        if (m_Desc.bIsVirtual) return true;


        D3D12_HEAP_PROPERTIES D3D12HeapProperties{};
        D3D12_RESOURCE_STATES D3D12InitialState = D3D12_RESOURCE_STATE_COMMON;

        switch (m_Desc.CpuAccess)
        {
        case ECpuAccessMode::None:
            D3D12HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
            D3D12InitialState = D3D12_RESOURCE_STATE_COMMON;
            break;

        case ECpuAccessMode::Read:
            D3D12HeapProperties.Type = D3D12_HEAP_TYPE_READBACK;
            D3D12InitialState = D3D12_RESOURCE_STATE_COPY_DEST;
            break;

        case ECpuAccessMode::Write:
            D3D12HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12InitialState = D3D12_RESOURCE_STATE_GENERIC_READ;
            break;
        }

        if (FAILED(m_cpContext->pDevice->CreateCommittedResource(
            &D3D12HeapProperties, 
            D3D12_HEAP_FLAG_NONE, 
            &m_D3D12ResourceDesc, 
            D3D12InitialState, 
            nullptr, 
            IID_PPV_ARGS(m_pD3D12Resource.GetAddressOf())
        )))
        {
            LOG_ERROR("Call to ID3D12Device::CreateCommittedResource failed.");
            return false;
        }

        m_pD3D12Resource->SetName(StringToWString(m_Desc.strName).c_str());
        m_GpuAddress = m_pD3D12Resource->GetGPUVirtualAddress();
        return true;
    }
    
    
    void* FDX12Buffer::Map(ECpuAccessMode CpuAccess, HANDLE FenceEvent)
    {
        if (m_pLastUsedFence != nullptr)
        {
            WaitForFence(m_pLastUsedFence.Get(), m_stLastUsedFenceValue, FenceEvent);
            m_pLastUsedFence = nullptr;
        }

        D3D12_RANGE D3D12Range;
        if (CpuAccess == ECpuAccessMode::Read)
        {
            D3D12Range = { 0, m_Desc.stByteSize };
        }
        else    // CpuAccess == ECpuAccessMode::Write
        {
            D3D12Range = { 0, 0 };
        }

        void* pMappedAddress = nullptr;
        if (FAILED(m_pD3D12Resource->Map(0, &D3D12Range, &pMappedAddress)))
        {
            LOG_ERROR("Map buffer failed. ");
            return nullptr;
        }

        return pMappedAddress;
    }

    void FDX12Buffer::Unmap()
    {
        m_pD3D12Resource->Unmap(0, nullptr);
    }
    
    FMemoryRequirements FDX12Buffer::GetMemoryRequirements()
    {
        D3D12_RESOURCE_ALLOCATION_INFO D3D12ResourceAllocInfo = m_cpContext->pDevice->GetResourceAllocationInfo(1, 1, &m_D3D12ResourceDesc);

        return FMemoryRequirements{ 
            D3D12ResourceAllocInfo.Alignment, 
            D3D12ResourceAllocInfo.SizeInBytes 
        };
    }

    BOOL FDX12Buffer::BindMemory(IHeap* pHeap, UINT64 stOffset)
    {
        if (pHeap == nullptr || m_pD3D12Resource == nullptr || m_Desc.bIsVirtual) return false;
        
        FDX12Heap* pDX12Heap = CheckedCast<FDX12Heap*>(pHeap);
        ReturnIfFalse(pDX12Heap->m_pD3D12Heap.Get() != nullptr);

        if (FAILED(m_cpContext->pDevice->CreatePlacedResource(
            pDX12Heap->m_pD3D12Heap.Get(), 
            stOffset, 
            &m_D3D12ResourceDesc, 
            ConvertResourceStates(m_Desc.InitialState), 
            nullptr, 
            IID_PPV_ARGS(m_pD3D12Resource.GetAddressOf())
        )))
        {
            LOG_ERROR("Bind memory failed.");
            return false;
        }

        m_GpuAddress = m_pD3D12Resource->GetGPUVirtualAddress();

        return true;
    }

    
    void FDX12Buffer::CreateCBV(UINT64 stDescriptorAddress, const FBufferRange& crRange)
    {
        assert(m_Desc.bIsConstantBuffer);

        FBufferRange Range = crRange.Resolve(m_Desc);
        assert(Range.stByteSize <= UINT_MAX);

        D3D12_CONSTANT_BUFFER_VIEW_DESC ViewDesc;
        ViewDesc.BufferLocation = m_pD3D12Resource->GetGPUVirtualAddress() + Range.stByteOffset;
        ViewDesc.SizeInBytes = Align(static_cast<UINT32>(Range.stByteSize), gdwConstantBufferOffsetSizeAlignment);

        m_cpContext->pDevice->CreateConstantBufferView(&ViewDesc, D3D12_CPU_DESCRIPTOR_HANDLE{ stDescriptorAddress });
    }

    void FDX12Buffer::CreateSRV(UINT64 stDescriptorAddress, EFormat Format, const FBufferRange& crRange, EResourceType ResourceType)
    {
        FBufferRange Range = crRange.Resolve(m_Desc);

        if (Format == EFormat::UNKNOWN) Format = m_Desc.Format;

        D3D12_SHADER_RESOURCE_VIEW_DESC ViewDesc{};
        ViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        ViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

        switch (ResourceType)
        {
        case EResourceType::StructuredBuffer_SRV:
            if (m_Desc.dwStructStride == 0)
            {
                LOG_ERROR("StructuredBuffer's stride can't be 0.");
                return;
            }
            ViewDesc.Format = DXGI_FORMAT_UNKNOWN;
            ViewDesc.Buffer.StructureByteStride = m_Desc.dwStructStride;
            ViewDesc.Buffer.FirstElement = Range.stByteOffset / m_Desc.dwStructStride;
            ViewDesc.Buffer.NumElements = static_cast<UINT32>(Range.stByteSize / m_Desc.dwStructStride);
            break;
        case EResourceType::TypedBuffer_SRV:
            {
                assert(Format != EFormat::UNKNOWN);
                ViewDesc.Format = GetDxgiFormatMapping(Format).SRVFormat;

                UINT8 btBytesPerElement = GetFormatInfo(Format).btBytesPerBlock;
                ViewDesc.Buffer.FirstElement = Range.stByteOffset / btBytesPerElement;
                ViewDesc.Buffer.NumElements = static_cast<UINT32>(Range.stByteSize / btBytesPerElement);
                break;
            }
        case EResourceType::RawBuffer_SRV:
            ViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            ViewDesc.Buffer.FirstElement = Range.stByteOffset / 4;
            ViewDesc.Buffer.NumElements = static_cast<UINT32>(Range.stByteSize / 4);
            ViewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            break;
        default:
            assert(!"Invalid Enumeration Value");
            return;
        }

        m_cpContext->pDevice->CreateShaderResourceView(m_pD3D12Resource.Get(), &ViewDesc, D3D12_CPU_DESCRIPTOR_HANDLE{ stDescriptorAddress });
    }
    
    void FDX12Buffer::CreateUAV(UINT64 stDescriptorAddress, EFormat Format, const FBufferRange& crRange, EResourceType ResourceType)
    {
        FBufferRange Range = crRange.Resolve(m_Desc);

        if (Format == EFormat::UNKNOWN) Format = m_Desc.Format;

        D3D12_UNORDERED_ACCESS_VIEW_DESC ViewDesc{};
        ViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        
        switch (ResourceType) 
        {
        case EResourceType::StructuredBuffer_UAV:
            assert(m_Desc.dwStructStride != 0);
            ViewDesc.Format = DXGI_FORMAT_UNKNOWN;
            ViewDesc.Buffer.StructureByteStride = m_Desc.dwStructStride;
            ViewDesc.Buffer.FirstElement = Range.stByteOffset / m_Desc.dwStructStride;
            ViewDesc.Buffer.NumElements = static_cast<UINT32>(Range.stByteSize / m_Desc.dwStructStride);
            break;
        case EResourceType::TypedBuffer_UAV:
            {
                assert(Format != EFormat::UNKNOWN);
                ViewDesc.Format = GetDxgiFormatMapping(Format).SRVFormat;

                UINT8 btBytesPerElement = GetFormatInfo(Format).btBytesPerBlock;
                ViewDesc.Buffer.FirstElement = Range.stByteOffset / btBytesPerElement;
                ViewDesc.Buffer.NumElements = static_cast<UINT32>(Range.stByteSize / btBytesPerElement);
                break;
            }
        case EResourceType::RawBuffer_UAV:
            ViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            ViewDesc.Buffer.FirstElement = Range.stByteOffset / 4;
            ViewDesc.Buffer.NumElements = static_cast<UINT32>(Range.stByteSize / 4);
            ViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            break;
        default:
			assert(!"Invalid Enumeration Value");
			return;
        }

        m_cpContext->pDevice->CreateUnorderedAccessView(m_pD3D12Resource.Get(), nullptr, &ViewDesc, D3D12_CPU_DESCRIPTOR_HANDLE{ stDescriptorAddress });
    }
    
    UINT32 FDX12Buffer::GetClearUAVIndex()
    {
        assert(m_Desc.bCanHaveUAVs);

        if (m_dwClearUAVIndex != gdwInvalidViewIndex) return m_dwClearUAVIndex;

        // If not found, then create one. 
        CreateUAV(
            m_pDescriptorHeaps->ShaderResourceHeap.GetCpuHandle(m_dwClearUAVIndex).ptr, 
            EFormat::R32_UINT,  // Raw buffer format. 
            gEntireBufferRange, 
            EResourceType::RawBuffer_UAV
        );

        m_pDescriptorHeaps->ShaderResourceHeap.CopyToShaderVisibleHeap(m_dwClearUAVIndex);

        return m_dwClearUAVIndex;
    }

    FDX12StagingTexture::FDX12StagingTexture(const FDX12Context* cpContext, const FTextureDesc& crDesc, ECpuAccessMode CpuAccessMode) :
        m_cpContext(cpContext), 
        m_Desc(crDesc), 
        m_MappedCpuAccessMode(CpuAccessMode), 
        m_D3D12ResourceDesc(ConvertTextureDesc(crDesc))
    {
    }

    BOOL FDX12StagingTexture::Initialize(FDX12DescriptorHeaps* pDescriptorHeaps)
    {
        ComputeSubresourceOffsets();

        FBufferDesc BufferDesc{};
        BufferDesc.strName = m_Desc.strName;
        BufferDesc.stByteSize = GetRequiredSize();
        BufferDesc.dwStructStride = 0;
        BufferDesc.CpuAccess = m_MappedCpuAccessMode;
        BufferDesc.InitialState = m_Desc.InitialState;

        FDX12Buffer* pBuffer = new FDX12Buffer(m_cpContext, pDescriptorHeaps, BufferDesc);
        if (!pBuffer->Initialize() || !pBuffer->QueryInterface(IID_IBuffer, PPV_ARG(m_pBuffer.GetAddressOf())))
        {
            LOG_ERROR("Create staging texture failed.");
            return false;
        }
        return true;
    }


    FDX12SliceRegion FDX12StagingTexture::GetSliceRegion(const FTextureSlice& crTextureSlice) const
    {
        FDX12SliceRegion Ret;
        const UINT32 cdwSubresourceIndex = CalcTextureSubresource(
            crTextureSlice.dwMipLevel, 
            crTextureSlice.dwArraySlice, 
            0, 
            m_Desc.dwMipLevels,
            m_Desc.dwArraySize
        );

        assert(cdwSubresourceIndex < m_SubresourceOffsets.size());

        UINT64 stSize = 0;
        m_cpContext->pDevice->GetCopyableFootprints(
            &m_D3D12ResourceDesc, 
            cdwSubresourceIndex, 
            1, 
            m_SubresourceOffsets[cdwSubresourceIndex], 
            &Ret.Footprint, 
            nullptr, 
            nullptr, 
            &stSize
        );

        Ret.stOffset = Ret.Footprint.Offset;
        Ret.stSize = stSize;

        return Ret;
    }


    void* FDX12StagingTexture::Map(const FTextureSlice& crTextureSlice, ECpuAccessMode CpuAccessMode, HANDLE FenceEvent, UINT64* pstRowPitch)
    {
        if (pstRowPitch == nullptr ||
            crTextureSlice.x != 0 || 
            crTextureSlice.y != 0 || 
            CpuAccessMode == ECpuAccessMode::None || 
            m_MappedRegion.stSize != 0 || 
            m_MappedAccessMode != ECpuAccessMode::None
        )
        {
            LOG_ERROR("This staging texture is not allowed to call map().");
            return nullptr;
        } 

        FTextureSlice SolvedSlice = crTextureSlice.Resolve(m_Desc);

        FDX12SliceRegion SliceRegion = GetSliceRegion(SolvedSlice);

        if (m_pLastUsedFence != nullptr)
        {
            WaitForFence(m_pLastUsedFence.Get(), m_stLastUsedFenceValue, FenceEvent);
            m_pLastUsedFence = nullptr;
        }

        D3D12_RANGE D3D12Range;
        if (CpuAccessMode == ECpuAccessMode::Read)
        {
            D3D12Range = { SliceRegion.stOffset, SliceRegion.stOffset + SliceRegion.stSize };
        }
        else    // ECpuAccessMode::Write
        {
            D3D12Range = { 0, 0 };
        }

        FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(m_pBuffer.Get());

        UINT8* pMappedAddress = nullptr;
        if (FAILED(pDX12Buffer->m_pD3D12Resource->Map(0, &D3D12Range, reinterpret_cast<void**>(&pMappedAddress))))
        {
            LOG_ERROR("Staging texture map failed.");
            return nullptr;
        }

        m_MappedRegion = SliceRegion;
        m_MappedAccessMode = CpuAccessMode;

        *pstRowPitch = SliceRegion.Footprint.Footprint.RowPitch;
        return pMappedAddress + m_MappedRegion.stOffset;
    }

    void FDX12StagingTexture::Unmap()
    {
        assert(m_MappedRegion.stSize != 0 && m_MappedAccessMode != ECpuAccessMode::None);

        D3D12_RANGE D3D12Range;
        if (m_MappedCpuAccessMode == ECpuAccessMode::Write)
        {
            D3D12Range = { m_MappedRegion.stOffset, m_MappedRegion.stOffset + m_MappedRegion.stSize };
        }
        else    // ECpuAccessMode::Read
        {
            D3D12Range = { 0, 0 };
        }

        FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(m_pBuffer.Get());
        pDX12Buffer->m_pD3D12Resource->Unmap(0, &D3D12Range);

        m_MappedRegion.stSize = 0;
        m_MappedCpuAccessMode = ECpuAccessMode::None;
    }

    UINT64 FDX12StagingTexture::GetRequiredSize() const
    {
        const UINT32 dwLastSubresourceIndex = CalcTextureSubresource(
            m_Desc.dwMipLevels - 1, 
            m_Desc.dwArraySize - 1, 
            0, 
            m_Desc.dwMipLevels, 
            m_Desc.dwArraySize
        );
        assert(dwLastSubresourceIndex < m_SubresourceOffsets.size());

        UINT64 stLastSubresourceSize;
        m_cpContext->pDevice->GetCopyableFootprints(
            &m_D3D12ResourceDesc, 
            dwLastSubresourceIndex, 
            1, 
            0, 
            nullptr, 
            nullptr, 
            nullptr, 
            &stLastSubresourceSize
        );

        return m_SubresourceOffsets[dwLastSubresourceIndex] + stLastSubresourceSize;
    }

    void FDX12StagingTexture::ComputeSubresourceOffsets()
    {
        const UINT32 dwLastSubresourceIndex = CalcTextureSubresource(
            m_Desc.dwMipLevels - 1, 
            m_Desc.dwArraySize - 1, 
            0, 
            m_Desc.dwMipLevels, 
            m_Desc.dwArraySize
        );

        const UINT32 dwSubresourcesNum = dwLastSubresourceIndex + 1;
        m_SubresourceOffsets.resize(dwSubresourcesNum);

        UINT64 stBaseOffset = 0;
        for (UINT32 ix = 0; ix < dwSubresourcesNum; ++ix)
        {
            UINT64 stSubresourceSize;
            m_cpContext->pDevice->GetCopyableFootprints(&m_D3D12ResourceDesc, ix, 1, 0, nullptr, nullptr, nullptr, &stSubresourceSize);

            m_SubresourceOffsets[ix] = stBaseOffset;
            stBaseOffset += stSubresourceSize;
            stBaseOffset = Align(stBaseOffset, static_cast<UINT64>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT));
        }
    }


    FDX12Sampler::FDX12Sampler(const FDX12Context* cpContext, const FSamplerDesc& crDesc) :
        m_cpContext(cpContext), m_Desc(crDesc)
    {
    }

    BOOL FDX12Sampler::Initialize()
    {
        UINT32 dwReductionType = ConvertSamplerReductionType(m_Desc.ReductionType);
        if (m_Desc.fMaxAnisotropy > 1.0f)
        {
            m_D3D12SamplerDesc.Filter = D3D12_ENCODE_ANISOTROPIC_FILTER(dwReductionType);
        }
        else 
        {
            m_D3D12SamplerDesc.Filter = D3D12_ENCODE_BASIC_FILTER(
                m_Desc.bMinFilter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
                m_Desc.bMaxFilter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
                m_Desc.bMipFilter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
                dwReductionType
            );
        }

        m_D3D12SamplerDesc.AddressU = ConvertSamplerAddressMode(m_Desc.AddressU);
        m_D3D12SamplerDesc.AddressV = ConvertSamplerAddressMode(m_Desc.AddressV);
        m_D3D12SamplerDesc.AddressW = ConvertSamplerAddressMode(m_Desc.AddressW);

        m_D3D12SamplerDesc.MipLODBias = m_Desc.fMipBias;
        m_D3D12SamplerDesc.MaxAnisotropy = std::max(static_cast<UINT32>(m_Desc.fMaxAnisotropy), 1u);
        m_D3D12SamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
        m_D3D12SamplerDesc.BorderColor[0] = m_Desc.BorderColor.r;
        m_D3D12SamplerDesc.BorderColor[1] = m_Desc.BorderColor.g;
        m_D3D12SamplerDesc.BorderColor[2] = m_Desc.BorderColor.b;
        m_D3D12SamplerDesc.BorderColor[3] = m_Desc.BorderColor.a;
        m_D3D12SamplerDesc.MinLOD = 0;
        m_D3D12SamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        return true;
    }

    
    void FDX12Sampler::CreateDescriptor(UINT64 stDescriptorAddress) const
    {
        m_cpContext->pDevice->CreateSampler(&m_D3D12SamplerDesc, D3D12_CPU_DESCRIPTOR_HANDLE{ stDescriptorAddress });
    }
}