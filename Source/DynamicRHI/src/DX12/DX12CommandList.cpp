#include "DX12CommandList.h"
#include "../Utils.h"
#include "DX12Converts.h"
#include "DX12Forward.h"
#include <climits>
#include <combaseapi.h>
#include <cstring>
#include <d3d12.h>
#include <memory>
#include <minwindef.h>
#include <pix_win.h>

#include "DX12Device.h"
#include "DX12FrameBuffer.h"
#include "DX12Pipeline.h"
#include "DX12RayTracing.h"
#include "DX12Resource.h"

namespace FTS 
{
    constexpr UINT64 gstVersionSubmittedFlag = 0x8000000000000000;
    constexpr UINT32 gdwVersionQueueShift = 60;
    constexpr UINT64 gstVersionIDMask = 0x0FFFFFFFFFFFFFFF;
    constexpr UINT64 MakeVersion(UINT64 id, ECommandQueueType QueueType, BOOL bSubmitted)
    {
        UINT64 stResult = (id & gstVersionIDMask) | (UINT64(QueueType) << gdwVersionQueueShift);
        if (bSubmitted) stResult |= gstVersionSubmittedFlag;
        return stResult;
    }
    constexpr UINT64 VersionGetInstance(UINT64 stVersion)
    {
        return stVersion & gstVersionIDMask;
    }

    constexpr BOOL VersionGetSubmitted(UINT64 stVersion)
    {
        return (stVersion & gstVersionSubmittedFlag) != 0;
    }
    

    FDX12UploadManager::FDX12UploadManager(
        const FDX12Context* cpContext, 
        FDX12CommandQueue* pCmdQueue, 
        UINT64 stDefaultChunkSize, 
        UINT64 stMemoryLimit,
        BOOL bDxrScratch
    ) :
        m_cpContext(cpContext), 
        m_pCmdQueue(pCmdQueue), 
        m_stDefaultChunkSize(stDefaultChunkSize), 
        m_stMaxMemorySize(stMemoryLimit),
        m_stAllocatedMemorySize(0),
        m_bDxrScratch(bDxrScratch)
    {
        if (!m_pCmdQueue)
        {
            LOG_ERROR("UploadManager initialize failed for nullptr cmdqueue.");
            assert(m_pCmdQueue != nullptr);
        }
    }
    

    BOOL FDX12UploadManager::SuballocateBuffer(
        UINT64 stSize, 
        ID3D12Resource** ppD3D12Buffer, 
        UINT64* pstOffset, 
        UINT8** ppCpuAddress, 
        D3D12_GPU_VIRTUAL_ADDRESS* pGpuAddress, 
        UINT64 stCurrentVersion, 
        UINT32 dwAligment,
        ID3D12GraphicsCommandList* pD3D12CmdList
    )
    {
        // DxrScratch upload manager need d3d12 cmdlist to set uav barrier.
        ReturnIfFalse(!m_bDxrScratch || pD3D12CmdList);

        std::shared_ptr<FDX12BufferChunk> pBufferChunkToRetire;

        // Try to allocate from the current chunk first.
        if (m_pCurrentChunk != nullptr)
        {
            UINT64 stAlignedOffset = Align(m_pCurrentChunk->stWriteEndPosition, static_cast<UINT64>(dwAligment));
            UINT64 stDataEndPos = stAlignedOffset + stSize;

            if (stDataEndPos <= m_pCurrentChunk->stBufferSize)
            {
                // The buffer can fit into the current chunk. 
                m_pCurrentChunk->stWriteEndPosition = stDataEndPos;

                if (ppD3D12Buffer != nullptr) *ppD3D12Buffer = m_pCurrentChunk->pD3D12Buffer.Get();
                if (pstOffset != nullptr) *pstOffset = stAlignedOffset;
                if (ppCpuAddress != nullptr) *ppCpuAddress = static_cast<UINT8*>(m_pCurrentChunk->pvCpuAddress) + stAlignedOffset;
                if (pGpuAddress != nullptr) *pGpuAddress = m_pCurrentChunk->pD3D12Buffer->GetGPUVirtualAddress() + stAlignedOffset;

                return true;
            }

            pBufferChunkToRetire = m_pCurrentChunk;
            m_pCurrentChunk.reset();
        }

        UINT64 stLastCompeletedValue = m_pCmdQueue->stLastCompletedValue;

        auto VersionGetSubmittedFunc = [](UINT64 stVersion) { return (stVersion & gstVersionSubmittedFlag) != 0; };
        auto VersionGetValueFunc     = [](uint64_t stVersion) { return stVersion & gstVersionIDMask; };

        for (auto it = m_pChunkPool.begin(); it != m_pChunkPool.end(); ++it)
        {
            std::shared_ptr<FDX12BufferChunk> pChunk = *it;
            
            if (VersionGetSubmittedFunc(pChunk->stVersion) && VersionGetValueFunc(pChunk->stVersion) <= stLastCompeletedValue)
            {
                pChunk->stVersion = 0;
            }

            // If this chunk has submitted and size-fit. 
            if (pChunk->stVersion == 0 && pChunk->stBufferSize >= stSize)
            {
                m_pCurrentChunk = pChunk;
                m_pChunkPool.erase(it);
                break;
            }
        }

        if (pBufferChunkToRetire)
        {
            m_pChunkPool.push_back(pBufferChunkToRetire);
        }

        if (!m_pCurrentChunk)
        {
            UINT64 stSizeToAllocate = Align(std::max(stSize, m_stDefaultChunkSize), FDX12BufferChunk::cstSizeAlignment);
            if (m_stMaxMemorySize > 0 && m_stAllocatedMemorySize + stSizeToAllocate <= m_stMaxMemorySize)
            {
                if (m_bDxrScratch)
                {
                    std::shared_ptr<FDX12BufferChunk> pBestChunk;
                    for (const auto& crChunk : m_pChunkPool)
                    {
                        if (crChunk->stBufferSize >= stSizeToAllocate)
                        {
                            if (!pBestChunk)
                            {
                                pBestChunk = crChunk;
                                continue;
                            }

                            BOOL bChunkSubmitted = VersionGetSubmitted(crChunk->stVersion);
                            BOOL bBestChunkSubmitted = VersionGetSubmitted(pBestChunk->stVersion);
                            UINT64 stChunkInstance = VersionGetInstance(crChunk->stVersion);
                            UINT64 stBestChunkInstance = VersionGetInstance(pBestChunk->stVersion);

                            if (
                                bChunkSubmitted && !bBestChunkSubmitted ||
                                bChunkSubmitted == bBestChunkSubmitted && 
                                stChunkInstance < stBestChunkInstance ||
                                bChunkSubmitted == bBestChunkSubmitted && 
                                stChunkInstance == stBestChunkInstance && 
                                crChunk->stBufferSize > pBestChunk->stBufferSize
                            )
                            {
                                pBestChunk = crChunk;
                            }
                        }
                    }
                    ReturnIfFalse(pBestChunk != nullptr);

                    m_pChunkPool.erase(std::find(m_pChunkPool.begin(), m_pChunkPool.end(), pBestChunk));
                    m_pCurrentChunk = pBestChunk;

                    D3D12_RESOURCE_BARRIER UAVBarrier = {};
                    UAVBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                    UAVBarrier.UAV.pResource = pBestChunk->pD3D12Buffer.Get();
                    pD3D12CmdList->ResourceBarrier(1, &UAVBarrier);
                }
                else 
                {
                    LOG_ERROR("No memory limit to upload resource.");
                    return false;
                }
            }
            else 
            {
                m_pCurrentChunk = CreateBufferChunk(stSizeToAllocate);
            }
        }

        // 从 ChunkPool 中掏出来的 chunk 相当于重置了, 和新创建的一样 
        m_pCurrentChunk->stVersion = stCurrentVersion;
        m_pCurrentChunk->stWriteEndPosition = stSize;

        if (ppD3D12Buffer != nullptr)   *ppD3D12Buffer = m_pCurrentChunk->pD3D12Buffer.Get();
        if (pstOffset != nullptr)       *pstOffset     = 0;
        if (ppCpuAddress != nullptr)    *ppCpuAddress  = static_cast<UINT8*>(m_pCurrentChunk->pvCpuAddress);
        if (pGpuAddress != nullptr)     *pGpuAddress   = m_pCurrentChunk->pD3D12Buffer->GetGPUVirtualAddress();

        return true;
    }

    void FDX12UploadManager::SubmitChunks(UINT64 stCurrentVersion, UINT64 stSubmittedVersion)
    {
        if (m_pCurrentChunk != nullptr)
        {
            m_pChunkPool.push_back(m_pCurrentChunk);
            m_pCurrentChunk.reset();
        }

        for (const auto& crChunk : m_pChunkPool)
        {
            if (crChunk->stVersion == stCurrentVersion)
            {
                crChunk->stVersion = stSubmittedVersion;
            }
        }
    }

    std::shared_ptr<FDX12BufferChunk> FDX12UploadManager::CreateBufferChunk(UINT64 stSize) const
    {
        std::shared_ptr<FDX12BufferChunk> Ret = std::make_shared<FDX12BufferChunk>();
        stSize = Align(stSize, FDX12BufferChunk::cstSizeAlignment);

        D3D12_HEAP_PROPERTIES HeapProperties{};
        HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC Desc = {};
        Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        Desc.Width = stSize;
        Desc.Height = 1;
        Desc.DepthOrArraySize = 1;
        Desc.MipLevels = 1;
        Desc.SampleDesc.Count = 1;
        Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        BOOL hRes = m_cpContext->pDevice->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &Desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(Ret->pD3D12Buffer.GetAddressOf())
        );
        if (FAILED(hRes)) return nullptr;

        hRes = Ret->pD3D12Buffer->Map(0, nullptr, &Ret->pvCpuAddress);
        if (FAILED(hRes)) return nullptr;

        Ret->stBufferSize = stSize;
        Ret->GpuAddress = Ret->pD3D12Buffer->GetGPUVirtualAddress();
        Ret->dwIndexInPool = static_cast<UINT32>(m_pChunkPool.size());

        return Ret;
    }


    FDX12CommandList::FDX12CommandList(
        const FDX12Context* cpContext,
        FDX12DescriptorHeaps* pDescriptorHeaps,
        IDevice* pDevice,
        FDX12CommandQueue* pDX12CmdQueue,
        const FCommandListDesc& crDesc
    ) :
        m_cpContext(cpContext), 
        m_pDescriptorHeaps(pDescriptorHeaps),
        m_pDevice(pDevice),
        m_pCmdQueue(pDX12CmdQueue),
        m_Desc(crDesc),
        m_UploadManager(m_cpContext, m_pCmdQueue, crDesc.stUploadChunkSize, 0),
        m_DxrScratchManager(m_cpContext, m_pCmdQueue, crDesc.stScratchChunkSize, crDesc.stScratchMaxMemory, true)
    {
    }

    BOOL FDX12CommandList::Initialize()
    {
        return true;
    }

    BOOL FDX12CommandList::Open()
    {
        UINT64 stCompletedValue = m_pCmdQueue->UpdateLastCompletedValue();

        std::shared_ptr<FDX12InternalCommandList> pCmdList;

        if (!m_pCmdListPool.empty())
        {
            pCmdList = m_pCmdListPool.front();

            if (pCmdList->stLastSubmittedValue <= stCompletedValue)
            {
                pCmdList->pCmdAllocator->Reset();
                pCmdList->pD3D12CommandList->Reset(pCmdList->pCmdAllocator.Get(), nullptr);
                m_pCmdListPool.pop_front();
            }
            else 
            {
                pCmdList = nullptr;
            }
        }
        
        if (!pCmdList) pCmdList = CreateInternalCmdList();

        m_pActiveCmdList = pCmdList;

        m_pInstance = std::make_shared<FDX12CommandListInstance>();
        m_pInstance->pD3D12CommandList = m_pActiveCmdList->pD3D12CommandList;
        m_pInstance->pCommandAllocator = m_pActiveCmdList->pCmdAllocator;
        m_pInstance->CommandQueueType = m_Desc.QueueType;

        m_stRecordingVersion = MakeVersion(m_pCmdQueue->stRecordingVersion++, m_Desc.QueueType, false);

        return true;
    }
    
    BOOL FDX12CommandList::Close()
    {
        CommitBarriers();

        m_pActiveCmdList->pD3D12CommandList->Close();

        ClearStateCache();

        m_pCurrentUploadBuffer = nullptr;
        m_VolatileCBAddressMap.clear();
        m_ShaderTableStatesMap.clear();

        return true;
    }

    BOOL FDX12CommandList::ClearState()
    {
        m_pActiveCmdList->pD3D12CommandList->ClearState(nullptr);
        ClearStateCache();
        ReturnIfFalse(CommitDescriptorHeaps());
        return true; 
    }

    BOOL FDX12CommandList::ClearTextureFloat(ITexture* pTexture, const FTextureSubresourceSet& crSubresourceSet, const FColor& crClearColor)
    {
        FDX12Texture* pDX12Texture = CheckedCast<FDX12Texture*>(pTexture);

        const FFormatInfo& crFormatInfo = GetFormatInfo(pDX12Texture->m_Desc.Format);
        if (crFormatInfo.bHasStencil || crFormatInfo.bHasDepth || (!pDX12Texture->m_Desc.bIsRenderTarget && !pDX12Texture->m_Desc.bIsUAV))
        {
            LOG_ERROR("This function can't use on the depth stenicl texture. Require the texture is render target or unordered access. ");
            return false;
        }

        FTextureSubresourceSet Subresource = crSubresourceSet.Resolve(pDX12Texture->m_Desc, false);
        
        m_pInstance->pReferencedResources.emplace_back(pTexture);

        if (pDX12Texture->m_Desc.bIsRenderTarget)
        {
            ReturnIfFalse(RequireTextureState(pTexture, Subresource, EResourceStates::RenderTarget));

            CommitBarriers();

            for (UINT32 ix = Subresource.dwBaseMipLevelIndex; ix < Subresource.dwBaseMipLevelIndex + Subresource.dwMipLevelsNum; ++ix)
            {
                UINT32 dwViewIndex = pDX12Texture->GetViewIndex(
                    EViewType::DX12_RenderTargetView,
                    Subresource,
                    false
                );

				D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle{ m_pDescriptorHeaps->RenderTargetHeap.GetCpuHandle(dwViewIndex) };

                m_pActiveCmdList->pD3D12CommandList->ClearRenderTargetView(RtvHandle, &crClearColor.r, 0, nullptr);
            }
        }
        else 
        {
			ReturnIfFalse(RequireTextureState(pTexture, Subresource, EResourceStates::UnorderedAccess));

            CommitBarriers();
            ReturnIfFalse(CommitDescriptorHeaps());

            for (UINT32 ix = Subresource.dwBaseMipLevelIndex; ix < Subresource.dwBaseMipLevelIndex + Subresource.dwMipLevelsNum; ++ix)
            {
                UINT32 dwDescriptorIndex = pDX12Texture->GetClearMipLevelUAVIndex(ix);
                
                m_pActiveCmdList->pD3D12CommandList->ClearUnorderedAccessViewFloat(
                    m_pDescriptorHeaps->ShaderResourceHeap.GetGpuHandleShaderVisible(dwDescriptorIndex), 
                    m_pDescriptorHeaps->ShaderResourceHeap.GetCpuHandle(dwDescriptorIndex), 
                    pDX12Texture->m_pD3D12Resource.Get(), 
                    &crClearColor.r, 
                    0, 
                    nullptr
                );
            }
        }
        return true; 
    }

    BOOL FDX12CommandList::ClearTextureUInt(ITexture* pTexture, const FTextureSubresourceSet& crSubresourceSet, UINT32 dwClearColor)
    {
        FDX12Texture* pDX12Texture = CheckedCast<FDX12Texture*>(pTexture);

        const FFormatInfo& crFormatInfo = GetFormatInfo(pDX12Texture->m_Desc.Format);
        if (crFormatInfo.bHasStencil || crFormatInfo.bHasDepth || (!pDX12Texture->m_Desc.bIsRenderTarget && !pDX12Texture->m_Desc.bIsUAV))
        {
            LOG_ERROR("This function can't use on the depth stenicl texture. Require the texture is render target or unordered access. ");
            return false;
        }

        FTextureSubresourceSet Subresource = crSubresourceSet.Resolve(pDX12Texture->m_Desc, false);

        m_pInstance->pReferencedResources.emplace_back(pTexture);

        if (pDX12Texture->m_Desc.bIsRenderTarget)
        {
			ReturnIfFalse(RequireTextureState(pTexture, Subresource, EResourceStates::RenderTarget));

            CommitBarriers();

            UINT32 dwMaxMipLevel = Subresource.dwBaseMipLevelIndex + Subresource.dwMipLevelsNum;
            for (UINT32 ix = Subresource.dwBaseMipLevelIndex; ix < dwMaxMipLevel; ++ix)
            {
                UINT32 dwViewIndex = pDX12Texture->GetViewIndex(
                    EViewType::DX12_RenderTargetView,
                    Subresource,
                    false
                );

				D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle{ m_pDescriptorHeaps->RenderTargetHeap.GetCpuHandle(dwViewIndex) };

                FLOAT pfClearValues[4] = { 
                    static_cast<FLOAT>(dwClearColor), 
                    static_cast<FLOAT>(dwClearColor), 
                    static_cast<FLOAT>(dwClearColor), 
                    static_cast<FLOAT>(dwClearColor) 
                };

                m_pActiveCmdList->pD3D12CommandList->ClearRenderTargetView(RtvHandle, pfClearValues, 0, nullptr);
            }
        }
        else 
        {
			ReturnIfFalse(RequireTextureState(pTexture, Subresource, EResourceStates::UnorderedAccess));

            CommitBarriers();
            ReturnIfFalse(CommitDescriptorHeaps());

            UINT32 dwMaxMipLevel = Subresource.dwBaseMipLevelIndex + Subresource.dwMipLevelsNum;
            for (UINT32 ix = Subresource.dwBaseMipLevelIndex; ix < dwMaxMipLevel; ++ix)
            {
                UINT32 dwDescriptorIndex = pDX12Texture->GetClearMipLevelUAVIndex(ix);
                
                UINT32 pdwClearValues[4] = { dwClearColor, dwClearColor, dwClearColor, dwClearColor };

                m_pActiveCmdList->pD3D12CommandList->ClearUnorderedAccessViewUint(
                    m_pDescriptorHeaps->ShaderResourceHeap.GetGpuHandleShaderVisible(dwDescriptorIndex), 
                    m_pDescriptorHeaps->ShaderResourceHeap.GetCpuHandle(dwDescriptorIndex), 
                    pDX12Texture->m_pD3D12Resource.Get(), 
                    pdwClearValues, 
                    0, 
                    nullptr
                );
            }
        }
        return true; 
    }
    BOOL FDX12CommandList::ClearDepthStencilTexture(
        ITexture* pTexture, 
        const FTextureSubresourceSet& crSubresourceSet, 
        BOOL bClearDepth, 
        FLOAT fDepth, 
        BOOL bClearStencil, 
        UINT8 btStencil
    )
    {
        if (!bClearDepth && !bClearStencil)
        {
            LOG_ERROR("Require the texture is depth or stencil.");
            return false;
        }
        
        FDX12Texture* pDX12Texture = CheckedCast<FDX12Texture*>(pTexture);

        const FFormatInfo& crFormatInfo = GetFormatInfo(pDX12Texture->m_Desc.Format);
        if (!(crFormatInfo.bHasStencil || crFormatInfo.bHasDepth) || !pDX12Texture->m_Desc.bIsDepthStencil)
        {
            LOG_ERROR("Require the texture is depth or stencil.");
            return false;
        }

        FTextureSubresourceSet Subresource = crSubresourceSet.Resolve(pDX12Texture->m_Desc, false);

        m_pInstance->pReferencedResources.emplace_back(pTexture);
        
		ReturnIfFalse(RequireTextureState(pTexture, Subresource, EResourceStates::DepthWrite));

        CommitBarriers();

        D3D12_CLEAR_FLAGS ClearFlags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;
        if (!bClearDepth) ClearFlags = D3D12_CLEAR_FLAG_STENCIL;
        else if (!bClearStencil) ClearFlags = D3D12_CLEAR_FLAG_DEPTH;

        for (UINT32 ix = Subresource.dwBaseMipLevelIndex; ix < Subresource.dwBaseMipLevelIndex + Subresource.dwMipLevelsNum; ++ix)
        {
            UINT32 dwViewIndex = pDX12Texture->GetViewIndex(
                EViewType::DX12_DepthStencilView,
                Subresource,
                false
            );
			D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle{ m_pDescriptorHeaps->DepthStencilHeap.GetCpuHandle(dwViewIndex) };

            m_pActiveCmdList->pD3D12CommandList->ClearDepthStencilView(DsvHandle, ClearFlags, fDepth, btStencil, 0, nullptr);
        }
        return true; 
    }
    
    BOOL FDX12CommandList::CopyTexture(ITexture* pDst, const FTextureSlice& crDstSlice, ITexture* pSrc, const FTextureSlice& crSrcSlice)
    {
        FDX12Texture *pDX12TextureDst = CheckedCast<FDX12Texture*>(pDst);
        FDX12Texture *pDX12TextureSrc = CheckedCast<FDX12Texture*>(pSrc);

        FTextureSlice ResolvedDstSlice = crDstSlice.Resolve(pDX12TextureDst->m_Desc);
        FTextureSlice ResolvedSrcSlice = crSrcSlice.Resolve(pDX12TextureSrc->m_Desc);

        UINT32 dwDstSubresourceIndex = CalcTextureSubresource(
            ResolvedDstSlice.dwMipLevel, 
            ResolvedDstSlice.dwArraySlice, 
            0, 
            pDX12TextureDst->m_Desc.dwMipLevels, 
            pDX12TextureDst->m_Desc.dwArraySize
        );

        UINT32 dwSrcSubresourceIndex = CalcTextureSubresource(
            ResolvedSrcSlice.dwMipLevel, 
            ResolvedSrcSlice.dwArraySlice, 
            0, 
            pDX12TextureSrc->m_Desc.dwMipLevels, 
            pDX12TextureSrc->m_Desc.dwArraySize
        );


        D3D12_TEXTURE_COPY_LOCATION DstLocation;
        DstLocation.pResource        = pDX12TextureDst->m_pD3D12Resource.Get();
        DstLocation.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        DstLocation.SubresourceIndex = dwDstSubresourceIndex;

        D3D12_TEXTURE_COPY_LOCATION SrcLocation;
        SrcLocation.pResource        = pDX12TextureSrc->m_pD3D12Resource.Get();
        SrcLocation.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        SrcLocation.SubresourceIndex = dwSrcSubresourceIndex;

        D3D12_BOX SrcBox;
        SrcBox.left   = ResolvedSrcSlice.x;
        SrcBox.top    = ResolvedSrcSlice.y;
        SrcBox.front  = ResolvedSrcSlice.z;
        SrcBox.right  = ResolvedSrcSlice.x + ResolvedSrcSlice.dwWidth;
        SrcBox.bottom = ResolvedSrcSlice.y + ResolvedSrcSlice.dwHeight;
        SrcBox.back   = ResolvedSrcSlice.z + ResolvedSrcSlice.dwDepth;

		ReturnIfFalse(RequireTextureState(pDst, FTextureSubresourceSet{ crDstSlice.dwMipLevel, 1, crDstSlice.dwArraySlice, 1 }, EResourceStates::CopyDest));
		ReturnIfFalse(RequireTextureState(pSrc, FTextureSubresourceSet{ crSrcSlice.dwMipLevel, 1, crSrcSlice.dwArraySlice, 1 }, EResourceStates::CopySource));

        CommitBarriers();

        m_pInstance->pReferencedResources.emplace_back(pDst);
        m_pInstance->pReferencedResources.emplace_back(pSrc);

        m_pActiveCmdList->pD3D12CommandList->CopyTextureRegion(
            &DstLocation,
            ResolvedDstSlice.x,
            ResolvedDstSlice.y,
            ResolvedDstSlice.z,
            &SrcLocation,
            &SrcBox
        );
        
        return true; 
    }
    BOOL FDX12CommandList::CopyTexture(IStagingTexture* pDst, const FTextureSlice& crDstSlice, ITexture* pSrc, const FTextureSlice& crSrcSlice)
    {
        FDX12Texture* pDX12TextureSrc   = CheckedCast<FDX12Texture*>(pSrc);
        FDX12StagingTexture* pDX12StagingTextureDst = CheckedCast<FDX12StagingTexture*>(pDst);
        
        FTextureSlice ResolvedDstSlice = crDstSlice.Resolve(pDX12StagingTextureDst->m_Desc);
        FTextureSlice ResolvedSrcSlice = crSrcSlice.Resolve(pDX12TextureSrc->m_Desc);

        UINT32 dwSrcSubresourceIndex = CalcTextureSubresource(
            ResolvedSrcSlice.dwMipLevel, 
            ResolvedSrcSlice.dwArraySlice, 
            0, 
            pDX12TextureSrc->m_Desc.dwMipLevels, 
            pDX12TextureSrc->m_Desc.dwArraySize
        );

		IBuffer* pTempBufferDst = pDX12StagingTextureDst->m_pBuffer.Get();
        FDX12Buffer* pDX12BufferDst = CheckedCast<FDX12Buffer*>(pTempBufferDst);

        D3D12_TEXTURE_COPY_LOCATION DstLocation;
        DstLocation.pResource        = pDX12BufferDst->m_pD3D12Resource.Get();
        DstLocation.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        DstLocation.PlacedFootprint = pDX12StagingTextureDst->GetSliceRegion(ResolvedDstSlice).Footprint;

        D3D12_TEXTURE_COPY_LOCATION SrcLocation;
        SrcLocation.pResource        = pDX12TextureSrc->m_pD3D12Resource.Get();
        SrcLocation.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        SrcLocation.SubresourceIndex = dwSrcSubresourceIndex;

        D3D12_BOX SrcBox;
        SrcBox.left   = ResolvedSrcSlice.x;
        SrcBox.top    = ResolvedSrcSlice.y;
        SrcBox.front  = ResolvedSrcSlice.z;
        SrcBox.right  = ResolvedSrcSlice.x + ResolvedSrcSlice.dwWidth;
        SrcBox.bottom = ResolvedSrcSlice.y + ResolvedSrcSlice.dwHeight;
        SrcBox.back   = ResolvedSrcSlice.z + ResolvedSrcSlice.dwDepth;

		ReturnIfFalse(RequireStagingTextureState(pDst, EResourceStates::CopyDest));
		ReturnIfFalse(RequireTextureState(pSrc, FTextureSubresourceSet{ crSrcSlice.dwMipLevel, 1, crSrcSlice.dwArraySlice, 1 }, EResourceStates::CopySource));

        CommitBarriers();

        m_pInstance->pReferencedStagingTextures.emplace_back(pDst);
        m_pInstance->pReferencedResources.emplace_back(pSrc);

        m_pActiveCmdList->pD3D12CommandList->CopyTextureRegion(
            &DstLocation,
            ResolvedDstSlice.x,
            ResolvedDstSlice.y,
            ResolvedDstSlice.z,
            &SrcLocation,
            &SrcBox
        );

        return true; 
    }
    BOOL FDX12CommandList::CopyTexture(ITexture* pDst, const FTextureSlice& crDstSlice, IStagingTexture* pSrc, const FTextureSlice& crSrcSlice)
    { 
        FDX12StagingTexture* pDX12StagingTextureSrc = CheckedCast<FDX12StagingTexture*>(pSrc);
        FDX12Texture* pDX12TextureDst = CheckedCast<FDX12Texture*>(pDst);
        
        FTextureSlice ResolvedDstSlice = crDstSlice.Resolve(pDX12TextureDst->m_Desc);
        FTextureSlice ResolvedSrcSlice = crSrcSlice.Resolve(pDX12StagingTextureSrc->m_Desc);

        UINT32 dwDstSubresourceIndex = CalcTextureSubresource(
            ResolvedDstSlice.dwMipLevel, 
            ResolvedDstSlice.dwArraySlice, 
            0, 
            pDX12TextureDst->m_Desc.dwMipLevels, 
            pDX12TextureDst->m_Desc.dwArraySize
        );

        IBuffer* pTempBufferSrc = pDX12StagingTextureSrc->m_pBuffer.Get();
        FDX12Buffer* pDX12BufferSrc = CheckedCast<FDX12Buffer*>(pTempBufferSrc);

        D3D12_TEXTURE_COPY_LOCATION DstLocation;
        DstLocation.pResource        = pDX12TextureDst->m_pD3D12Resource.Get();
        DstLocation.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        DstLocation.SubresourceIndex = dwDstSubresourceIndex;

        D3D12_TEXTURE_COPY_LOCATION SrcLocation;
        SrcLocation.pResource        = pDX12BufferSrc->m_pD3D12Resource.Get();
        SrcLocation.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        SrcLocation.PlacedFootprint = pDX12StagingTextureSrc->GetSliceRegion(ResolvedSrcSlice).Footprint;

        D3D12_BOX SrcBox;
        SrcBox.left   = ResolvedSrcSlice.x;
        SrcBox.top    = ResolvedSrcSlice.y;
        SrcBox.front  = ResolvedSrcSlice.z;
        SrcBox.right  = ResolvedSrcSlice.x + ResolvedSrcSlice.dwWidth;
        SrcBox.bottom = ResolvedSrcSlice.y + ResolvedSrcSlice.dwHeight;
        SrcBox.back   = ResolvedSrcSlice.z + ResolvedSrcSlice.dwDepth;

		ReturnIfFalse(RequireTextureState(pDst, FTextureSubresourceSet{ crDstSlice.dwMipLevel, 1, crDstSlice.dwArraySlice, 1 }, EResourceStates::CopyDest));
		ReturnIfFalse(RequireStagingTextureState(pSrc, EResourceStates::CopySource));

        CommitBarriers();

        m_pInstance->pReferencedResources.emplace_back(pDst);
        m_pInstance->pReferencedStagingTextures.emplace_back(pSrc);

        m_pActiveCmdList->pD3D12CommandList->CopyTextureRegion(
            &DstLocation,
            ResolvedDstSlice.x,
            ResolvedDstSlice.y,
            ResolvedDstSlice.z,
            &SrcLocation,
            &SrcBox
        );

        return true;
    }
    
    BOOL FDX12CommandList::WriteTexture(
        ITexture* pDst, 
        UINT32 dwArraySlice, 
        UINT32 dwMipLevel, 
        const UINT8* cpData, 
        UINT64 stRowPitch, 
        UINT64 stDepthPitch
    )
    { 
        FDX12Texture* pDX12Texture = CheckedCast<FDX12Texture*>(pDst);

		ReturnIfFalse(RequireTextureState(pDst, FTextureSubresourceSet{ dwMipLevel, 1, dwArraySlice, 1 }, EResourceStates::CopyDest));

        CommitBarriers();

        UINT32 dwSubresourceIndex = CalcTextureSubresource(
            dwMipLevel,
            dwArraySlice,
            0,
            pDX12Texture->m_Desc.dwMipLevels,
            pDX12Texture->m_Desc.dwArraySize
        );

        D3D12_RESOURCE_DESC D3D12ResourceDesc = pDX12Texture->m_pD3D12Resource->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
        UINT32 dwRowsNum;
        UINT64 stRowSizeInBytes;
        UINT64 stTotalBytes;

        m_cpContext->pDevice->GetCopyableFootprints(&D3D12ResourceDesc, dwSubresourceIndex, 1, 0, &Footprint, &dwRowsNum, &stRowSizeInBytes, &stTotalBytes);

        UINT8* pvCpuAddress = nullptr;
        ID3D12Resource* pUploadBuffer = nullptr;
        UINT64 stOffsetInUploadBuffer = 0;

        ReturnIfFalse(m_UploadManager.SuballocateBuffer(
            stTotalBytes, 
            &pUploadBuffer, 
            &stOffsetInUploadBuffer, 
            &pvCpuAddress, 
            nullptr, 
            m_stRecordingVersion, 
            D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
        ));
        Footprint.Offset = stOffsetInUploadBuffer;

        ReturnIfFalse(dwRowsNum <= Footprint.Footprint.Height);

        for (UINT32 dwDepthSlice = 0; dwDepthSlice < Footprint.Footprint.Depth; ++dwDepthSlice)
        {
            for (UINT32 dwRow = 0; dwRow < dwRowsNum; ++dwRow)
            {
                void* pvDstAddress = static_cast<UINT8*>(pvCpuAddress) + Footprint.Footprint.RowPitch * (dwRow + dwDepthSlice * dwRowsNum);
                const void* cpvSrcAddress = cpData + stRowPitch * dwRow + stDepthPitch * dwDepthSlice;
                memcpy(pvDstAddress, cpvSrcAddress, std::min(stRowPitch, stRowSizeInBytes));
            }
        }

        D3D12_TEXTURE_COPY_LOCATION DstLocation;
        DstLocation.pResource = pDX12Texture->m_pD3D12Resource.Get();
        DstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        DstLocation.SubresourceIndex = dwSubresourceIndex;

        D3D12_TEXTURE_COPY_LOCATION SrcLocation;
        SrcLocation.pResource = pUploadBuffer;
        SrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        SrcLocation.PlacedFootprint = Footprint;

        m_pInstance->pReferencedResources.emplace_back(pDst);

        if (pUploadBuffer != m_pCurrentUploadBuffer)
        {
            m_pInstance->pReferencedNativeResources.emplace_back(pUploadBuffer);
            m_pCurrentUploadBuffer = pUploadBuffer;
        }

        m_pActiveCmdList->pD3D12CommandList->CopyTextureRegion(&DstLocation, 0, 0, 0, &SrcLocation, nullptr);

        return true; 
    }

    BOOL FDX12CommandList::ResolveTexture(ITexture* pDst, const FTextureSubresourceSet& crDstSubresourceSet, ITexture* pSrc, const FTextureSubresourceSet& crSrcSubresourceSet)
    { 
        FDX12Texture* pDX12TextureDst = CheckedCast<FDX12Texture*>(pDst);
        FDX12Texture* pDX12TextureSrc = CheckedCast<FDX12Texture*>(pSrc);

        FTextureSubresourceSet DstSubresource = crDstSubresourceSet.Resolve(pDX12TextureDst->m_Desc, false);
        FTextureSubresourceSet SrcSubresource = crSrcSubresourceSet.Resolve(pDX12TextureSrc->m_Desc, false);

        if (DstSubresource.dwArraySlicesNum != SrcSubresource.dwArraySlicesNum || 
            DstSubresource.dwMipLevelsNum != SrcSubresource.dwMipLevelsNum)
            return false;

		ReturnIfFalse(RequireTextureState(pDst, DstSubresource, EResourceStates::ResolveDest));
		ReturnIfFalse(RequireTextureState(pSrc, SrcSubresource, EResourceStates::ResolveSource));

        CommitBarriers();
        
        const FDxgiFormatMapping& crDxgiFormatMapping = GetDxgiFormatMapping(pDX12TextureDst->m_Desc.Format);

        for (UINT32 dwPlane = 0; dwPlane < pDX12TextureDst->m_btPlaneCount; ++dwPlane)
        {
            for (UINT32 dwArraySlice = 0; dwArraySlice < DstSubresource.dwArraySlicesNum; ++dwArraySlice)
            {
                for (UINT32 dwMipLevel = 0; dwMipLevel < DstSubresource.dwMipLevelsNum; ++dwMipLevel)
                {
                    UINT32 dwDstSubresourceIndex = CalcTextureSubresource(
                        dwMipLevel + DstSubresource.dwBaseMipLevelIndex, 
                        dwArraySlice + DstSubresource.dwBaseArraySliceIndex, 
                        dwPlane, 
                        pDX12TextureDst->m_Desc.dwMipLevels, 
                        pDX12TextureDst->m_Desc.dwArraySize
                    );

                    UINT32 dwSrcSubresourceIndex = CalcTextureSubresource(
                        dwMipLevel + DstSubresource.dwBaseMipLevelIndex, 
                        dwArraySlice + DstSubresource.dwBaseArraySliceIndex, 
                        dwPlane, 
                        pDX12TextureSrc->m_Desc.dwMipLevels, 
                        pDX12TextureSrc->m_Desc.dwArraySize
                    );

                    m_pActiveCmdList->pD3D12CommandList->ResolveSubresource(
                        pDX12TextureDst->m_pD3D12Resource.Get(), 
                        dwDstSubresourceIndex, 
                        pDX12TextureSrc->m_pD3D12Resource.Get(), 
                        dwSrcSubresourceIndex, 
                        crDxgiFormatMapping.RTVFormat
                    );
                }
            }
        }

        return true;
    }
    
    BOOL FDX12CommandList::WriteBuffer(IBuffer* pBuffer, const void* cpvData, UINT64 stDataSize, UINT64 stDstOffsetBytes)
    {
        UINT8* pCpuAddress;
        D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
        ID3D12Resource* pD3D12UploadBuffer;
        UINT64 stOffsetInUploadBuffer;
        if (!m_UploadManager.SuballocateBuffer(
            stDataSize, 
            &pD3D12UploadBuffer, 
            &stOffsetInUploadBuffer, 
            &pCpuAddress, 
            &GpuAddress, 
            m_stRecordingVersion, 
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
        ))
        {
            LOG_ERROR("Couldn't suballocate a upload buffer. ");
            return false;
        }

        if (pD3D12UploadBuffer != m_pCurrentUploadBuffer)
        {
            m_pInstance->pReferencedNativeResources.emplace_back(pD3D12UploadBuffer);
            m_pCurrentUploadBuffer = pD3D12UploadBuffer;
        }

        memcpy(pCpuAddress, cpvData, stDataSize);

        FBufferDesc BufferDesc = pBuffer->GetDesc();

        if (BufferDesc.bIsVolatile)
        {
            m_VolatileCBAddressMap[pBuffer] = GpuAddress;
            m_bAnyVolatileCBWrites = true;
        }
        else 
        {
			ReturnIfFalse(RequireBufferState(pBuffer, EResourceStates::CopyDest));

            CommitBarriers();

            m_pInstance->pReferencedResources.emplace_back(pBuffer);

            FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(pBuffer);
            m_pActiveCmdList->pD3D12CommandList->CopyBufferRegion(pDX12Buffer->m_pD3D12Resource.Get(), stDstOffsetBytes, pD3D12UploadBuffer, stOffsetInUploadBuffer, stDataSize);
        }

        return true;
    }

    BOOL FDX12CommandList::ClearBufferUInt(IBuffer* pBuffer, UINT32 dwClearValue)
    {
        FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(pBuffer);

        ReturnIfFalse(pDX12Buffer->m_Desc.bCanHaveUAVs);

		ReturnIfFalse(RequireBufferState(pBuffer, EResourceStates::UnorderedAccess));

        CommitBarriers();

        ReturnIfFalse(CommitDescriptorHeaps());

        UINT32 dwClearUavIndex = pDX12Buffer->GetClearUAVIndex();

        m_pInstance->pReferencedResources.emplace_back(pBuffer);

        const UINT32 pdwClearValues[4] = { dwClearValue, dwClearValue, dwClearValue, dwClearValue };
        m_pActiveCmdList->pD3D12CommandList->ClearUnorderedAccessViewUint(
            m_pDescriptorHeaps->ShaderResourceHeap.GetGpuHandleShaderVisible(dwClearUavIndex), 
            m_pDescriptorHeaps->ShaderResourceHeap.GetCpuHandle(dwClearUavIndex), 
            pDX12Buffer->m_pD3D12Resource.Get(), 
            pdwClearValues, 
            0, 
            nullptr
        );

        return true;
    }
    
    BOOL FDX12CommandList::CopyBuffer(IBuffer* pDst, UINT64 stDstOffsetBytes, IBuffer* pSrc, UINT64 stSrcOffsetBytes, UINT64 stDataSizeBytes)
    {
        FDX12Buffer* pDX12DstBuffer = CheckedCast<FDX12Buffer*>(pDst);
        FDX12Buffer* pDX12SrcBuffer = CheckedCast<FDX12Buffer*>(pSrc);

		ReturnIfFalse(RequireBufferState(pDst, EResourceStates::CopyDest));
		ReturnIfFalse(RequireBufferState(pSrc, EResourceStates::CopySource));

        CommitBarriers();

        if (pDX12DstBuffer->m_Desc.CpuAccess == ECpuAccessMode::None)
            m_pInstance->pReferencedResources.emplace_back(pDst);
        else 
            m_pInstance->pReferencedStagingBuffers.emplace_back(pDst);

        if (pDX12SrcBuffer->m_Desc.CpuAccess == ECpuAccessMode::None)
            m_pInstance->pReferencedResources.emplace_back(pSrc);
        else 
            m_pInstance->pReferencedStagingBuffers.emplace_back(pSrc);

        m_pActiveCmdList->pD3D12CommandList->CopyBufferRegion(
            pDX12DstBuffer->m_pD3D12Resource.Get(),
            stDstOffsetBytes,
            pDX12SrcBuffer->m_pD3D12Resource.Get(),
            stSrcOffsetBytes,
            stDataSizeBytes
        );

        return true;
    }
    
    BOOL FDX12CommandList::SetPushConstants(const void* cpvData, UINT64 stByteSize)
    {
        FDX12RootSignature* pDX12RootSignature = nullptr;
        BOOL bIsGraphics = false;

        if (m_bCurrGraphicsStateValid && m_CurrGraphicsState.pPipeline != nullptr)
        {
            FDX12GraphicsPipeline* pDX12GraphicsPipeline = CheckedCast<FDX12GraphicsPipeline*>(m_CurrGraphicsState.pPipeline);
            pDX12RootSignature = CheckedCast<FDX12RootSignature*>(pDX12GraphicsPipeline->m_pDX12RootSignature.Get());
            bIsGraphics = true;
        }
        else if (m_bCurrComputeStateValid && m_CurrComputeState.pPipeline != nullptr)
        {
            FDX12ComputePipeline* pDX12ComputePipeline = CheckedCast<FDX12ComputePipeline*>(m_CurrComputeState.pPipeline);
            pDX12RootSignature = CheckedCast<FDX12RootSignature*>(pDX12ComputePipeline->m_pDX12RootSignature.Get());
            bIsGraphics = false;
        }

		ReturnIfFalse(pDX12RootSignature && pDX12RootSignature->m_dwPushConstantSize == stByteSize);

        if (bIsGraphics)
        {
            m_pActiveCmdList->pD3D12CommandList->SetGraphicsRoot32BitConstants(
                pDX12RootSignature->m_dwRootParameterPushConstantsIndex, 
                static_cast<UINT32>(stByteSize / 4), 
                cpvData, 
                0
            );
        }
        else 
        {
            m_pActiveCmdList->pD3D12CommandList->SetComputeRoot32BitConstants(
                pDX12RootSignature->m_dwRootParameterPushConstantsIndex, 
                static_cast<UINT32>(stByteSize / 4), 
                cpvData, 
                0
            );
        }

        return true;
    }

    BOOL FDX12CommandList::SetGraphicsState(const FGraphicsState& crState)
    { 
        FDX12GraphicsPipeline* pCurrDX12GraphicsPipeline = CheckedCast<FDX12GraphicsPipeline*>(m_CurrGraphicsState.pPipeline);
        FDX12GraphicsPipeline* pDX12GraphicsPipeline = CheckedCast<FDX12GraphicsPipeline*>(crState.pPipeline);

        UINT32 dwBindingUpdateMask = 0;     // 按位判断 bindingset 数组中哪一个 bindingset 需要更新绑定

        BOOL bUpdateRootSignature = !m_bCurrGraphicsStateValid || 
                                    m_CurrGraphicsState.pPipeline == nullptr || 
                                    pCurrDX12GraphicsPipeline->m_pDX12RootSignature != pDX12GraphicsPipeline->m_pDX12RootSignature;
        
        if (!bUpdateRootSignature) dwBindingUpdateMask = ~0u;

        if (CommitDescriptorHeaps()) dwBindingUpdateMask = ~0u;

        if (dwBindingUpdateMask == 0)
        {
            dwBindingUpdateMask = FindArrayDifferenctBits(
                m_CurrGraphicsState.pBindingSets, 
                m_CurrGraphicsState.pBindingSets.Size(), 
                crState.pBindingSets, 
                crState.pBindingSets.Size()
            );
        } 

        BOOL bUpdatePipeline = !m_bCurrGraphicsStateValid || m_CurrGraphicsState.pPipeline != crState.pPipeline;

        if (bUpdatePipeline)
        {
            ReturnIfFalse(BindGraphicsPipeline(crState.pPipeline, bUpdateRootSignature));
            m_pInstance->pReferencedResources.emplace_back(crState.pPipeline);
        }

        
        FGraphicsPipelineDesc PipelineDesc = pDX12GraphicsPipeline->GetDesc();

        UINT8 btEffectiveStencilRefValue = 
            PipelineDesc.RenderState.DepthStencilState.bDynamicStencilRef ? 
            crState.btDynamicStencilRefValue : 
            PipelineDesc.RenderState.DepthStencilState.btStencilRefValue;

        BOOL bUpdateStencilRef = !m_bCurrGraphicsStateValid || m_CurrGraphicsState.btDynamicStencilRefValue != btEffectiveStencilRefValue;

        if (PipelineDesc.RenderState.DepthStencilState.bStencilEnable && (bUpdatePipeline || bUpdateStencilRef))
        {
            m_pActiveCmdList->pD3D12CommandList->OMSetStencilRef(btEffectiveStencilRefValue);
        }

        BOOL bUpdateBlendFactor = !m_bCurrGraphicsStateValid || m_CurrGraphicsState.BlendConstantColor != crState.BlendConstantColor;
        if (pDX12GraphicsPipeline->m_bRequiresBlendFactor && bUpdateBlendFactor)
        {
            m_pActiveCmdList->pD3D12CommandList->OMSetBlendFactor(&crState.BlendConstantColor.r);
        }

		BOOL bUpdateFrameBuffer = !m_bCurrGraphicsStateValid || m_CurrGraphicsState.pFramebuffer != crState.pFramebuffer;
		if (bUpdateFrameBuffer)
		{
			ReturnIfFalse(BindFrameBuffer(crState.pFramebuffer));
			m_pInstance->pReferencedResources.emplace_back(crState.pFramebuffer);
		}

        ReturnIfFalse(SetGraphicsBindings(
            crState.pBindingSets,
            dwBindingUpdateMask, 
            pDX12GraphicsPipeline->m_pDX12RootSignature.Get()
        ));


        BOOL bUpdateIndexBuffer = 
            !m_bCurrGraphicsStateValid || 
            m_CurrGraphicsState.IndexBufferBinding != crState.IndexBufferBinding;
        if (bUpdateIndexBuffer && crState.IndexBufferBinding.IsValid())
        {
            D3D12_INDEX_BUFFER_VIEW IndexBufferView{};
            
            auto& rpIndexBuffer = crState.IndexBufferBinding.pBuffer;
			ReturnIfFalse(RequireBufferState(rpIndexBuffer, EResourceStates::IndexBuffer));

            FDX12Buffer* pDX12IndexBuffer = CheckedCast<FDX12Buffer*>(rpIndexBuffer);

            IndexBufferView.Format = GetDxgiFormatMapping(crState.IndexBufferBinding.Format).SRVFormat;
            IndexBufferView.BufferLocation = pDX12IndexBuffer->m_GpuAddress + crState.IndexBufferBinding.dwOffset;
            IndexBufferView.SizeInBytes = static_cast<UINT32>(pDX12IndexBuffer->m_Desc.stByteSize - crState.IndexBufferBinding.dwOffset);

            m_pInstance->pReferencedResources.emplace_back(rpIndexBuffer);
            
            m_pActiveCmdList->pD3D12CommandList->IASetIndexBuffer(&IndexBufferView);
        }

        BOOL bUpdateVertexBuffer = 
            !m_bCurrGraphicsStateValid || 
            !IsSameArrays(m_CurrGraphicsState.VertexBufferBindings, crState.VertexBufferBindings);

        if (bUpdateVertexBuffer && !crState.VertexBufferBindings.Empty())
        {
            D3D12_VERTEX_BUFFER_VIEW VertexBufferViews[gdwMaxVertexAttributes];
            UINT32 dwMaxVertexBufferBindingSlot = 0;

            FDX12InputLayout* pDX12InputLayout = CheckedCast<FDX12InputLayout*>(PipelineDesc.pInputLayout);

            for (const auto& crBinding : crState.VertexBufferBindings)
            {
				ReturnIfFalse(RequireBufferState(crBinding.pBuffer, EResourceStates::VertexBuffer));
                
                if (crBinding.dwSlot > gdwMaxVertexAttributes) return false;

                auto& rpVertexBuffer = crBinding.pBuffer;
                
                FDX12Buffer* pDX12VertexBuffer = CheckedCast<FDX12Buffer*>(rpVertexBuffer);

                VertexBufferViews[crBinding.dwSlot].StrideInBytes = pDX12InputLayout->m_SlotStrideMap[crBinding.dwSlot];
                VertexBufferViews[crBinding.dwSlot].BufferLocation = pDX12VertexBuffer->m_GpuAddress + crBinding.stOffset;
                VertexBufferViews[crBinding.dwSlot].SizeInBytes = 
                    static_cast<UINT32>(std::min(pDX12VertexBuffer->m_Desc.stByteSize - crBinding.stOffset, static_cast<UINT64>(ULONG_MAX)));

                dwMaxVertexBufferBindingSlot = std::max(dwMaxVertexBufferBindingSlot, crBinding.dwSlot);

                m_pInstance->pReferencedResources.emplace_back(rpVertexBuffer);
            }

            if (m_bCurrGraphicsStateValid)
            {
                for (const auto& crBinding : m_CurrGraphicsState.VertexBufferBindings)
                {
                    if (crBinding.dwSlot < gdwMaxVertexAttributes)
                    {
                        dwMaxVertexBufferBindingSlot = std::max(dwMaxVertexBufferBindingSlot, crBinding.dwSlot);
                    }
                }
            }

            m_pActiveCmdList->pD3D12CommandList->IASetVertexBuffers(0, dwMaxVertexBufferBindingSlot + 1, VertexBufferViews);
        }
        
        CommitBarriers();

        BOOL bUpdateViewports = 
            !m_bCurrGraphicsStateValid ||
            !IsSameArrays(m_CurrGraphicsState.ViewportState.Viewports, crState.ViewportState.Viewports) ||
            !IsSameArrays(m_CurrGraphicsState.ViewportState.Rects, crState.ViewportState.Rects);

        if (bUpdateViewports)
        {
            FFrameBufferInfo FrameBufferInfo = crState.pFramebuffer->GetInfo();

            FDX12ViewportState ViewportState = ConvertViewportState(PipelineDesc.RenderState.RasterizerState, FrameBufferInfo, crState.ViewportState);

            if (ViewportState.Viewports.Size() > 0)
            {
                m_pActiveCmdList->pD3D12CommandList->RSSetViewports(ViewportState.Viewports.Size(), ViewportState.Viewports.data());
            }

            if (ViewportState.ScissorRects.Size() > 0)
            {
                m_pActiveCmdList->pD3D12CommandList->RSSetScissorRects(ViewportState.ScissorRects.Size(), ViewportState.ScissorRects.data());
            }
        }

        m_bCurrGraphicsStateValid = true;
        m_bCurrComputeStateValid = false;
        m_CurrGraphicsState = crState;
        m_CurrGraphicsState.btDynamicStencilRefValue = btEffectiveStencilRefValue;

        return true;
    }

    BOOL FDX12CommandList::SetComputeState(const FComputeState& crState)
    {
        FDX12ComputePipeline* pCurrDX12ComputePipeline = CheckedCast<FDX12ComputePipeline*>(m_CurrComputeState.pPipeline);
        FDX12ComputePipeline* pDX12ComputePipeline = CheckedCast<FDX12ComputePipeline*>(crState.pPipeline);

        UINT32 dwBindingUpdateMask = 0;

        BOOL bUpdateRootSignature = !m_bCurrComputeStateValid || m_CurrComputeState.pPipeline == nullptr;
        if (!bUpdateRootSignature && pCurrDX12ComputePipeline->m_pDX12RootSignature != pDX12ComputePipeline->m_pDX12RootSignature)
            bUpdateRootSignature = true;

        if (!m_bCurrComputeStateValid || bUpdateRootSignature) dwBindingUpdateMask = ~0u; 
        if (CommitDescriptorHeaps()) dwBindingUpdateMask = ~0u;

        if (dwBindingUpdateMask == 0)
        {
            dwBindingUpdateMask = FindArrayDifferenctBits(
                m_CurrComputeState.pBindingSets, 
                m_CurrComputeState.pBindingSets.Size(), 
                crState.pBindingSets, 
                crState.pBindingSets.Size()
            );
        } 

        if (bUpdateRootSignature)
        {
            FDX12RootSignature* pDXRootSignature = CheckedCast<FDX12RootSignature*>(pDX12ComputePipeline->m_pDX12RootSignature.Get());
            m_pActiveCmdList->pD3D12CommandList->SetComputeRootSignature(pDXRootSignature->m_pD3D12RootSignature.Get());
        }

        BOOL bUpdatePipeline = !m_bCurrComputeStateValid || m_CurrComputeState.pPipeline != crState.pPipeline;
        if (bUpdatePipeline)
        {
            m_pActiveCmdList->pD3D12CommandList->SetPipelineState(pDX12ComputePipeline->m_pD3D12PipelineState.Get());
            m_pInstance->pReferencedResources.emplace_back(crState.pPipeline);
        }

        SetComputeBindings(
            crState.pBindingSets, 
            dwBindingUpdateMask, 
            pDX12ComputePipeline->m_pDX12RootSignature.Get()
        );
        
        m_bCurrComputeStateValid = true;
        m_bCurrGraphicsStateValid = false;
        m_CurrComputeState = crState;
        return true;
    }

    BOOL FDX12CommandList::Draw(const FDrawArguments& crArgs)
    {
        UpdateGraphicsVolatileBuffers();

        m_pActiveCmdList->pD3D12CommandList->DrawInstanced(
            crArgs.dwIndexOrVertexCount, 
            crArgs.dwInstanceCount, 
            crArgs.dwStartVertexLocation, 
            crArgs.dwStartInstanceLocation
        );
    
        return true;
    }
    
    BOOL FDX12CommandList::DrawIndexed(const FDrawArguments& crArgs)
    {
        UpdateGraphicsVolatileBuffers();

        m_pActiveCmdList->pD3D12CommandList->DrawIndexedInstanced(
            crArgs.dwIndexOrVertexCount, 
            crArgs.dwInstanceCount, 
            crArgs.dwStartIndexLocation, 
            crArgs.dwStartVertexLocation, 
            crArgs.dwStartInstanceLocation
        );

        return true;
    }

    BOOL FDX12CommandList::Dispatch(UINT32 dwGroupsX, UINT32 dwGroupsY, UINT32 dwGroupsZ)
    {
        UpdateComputeVolatileBuffers();

        m_pActiveCmdList->pD3D12CommandList->Dispatch(dwGroupsX, dwGroupsY, dwGroupsZ);
    
        return true;
    }

    BOOL FDX12CommandList::BeginTimerQuery(ITimerQuery* pQuery)
    {
        m_pInstance->pReferencedTimerQueries.emplace_back(pQuery);

        FDX12TimerQuery* pDX12TimerQuery = CheckedCast<FDX12TimerQuery*>(pQuery);

        m_pActiveCmdList->pD3D12CommandList->EndQuery(
            m_cpContext->pTimerQueryHeap.Get(), 
            D3D12_QUERY_TYPE_TIMESTAMP, 
            pDX12TimerQuery->m_dwBeginQueryIndex
        );

        return true;
    }

    BOOL FDX12CommandList::EndTimerQuery(ITimerQuery* pQuery)
    {
        m_pInstance->pReferencedTimerQueries.emplace_back(pQuery);

        FDX12TimerQuery* pDX12TimerQuery = CheckedCast<FDX12TimerQuery*>(pQuery);

        m_pActiveCmdList->pD3D12CommandList->EndQuery(
            m_cpContext->pTimerQueryHeap.Get(), 
            D3D12_QUERY_TYPE_TIMESTAMP, 
            pDX12TimerQuery->m_dwEndQueryIndex
        );

        FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(m_cpContext->pTimerQueryResolveBuffer.Get());

        m_pActiveCmdList->pD3D12CommandList->ResolveQueryData(
            m_cpContext->pTimerQueryHeap.Get(), 
            D3D12_QUERY_TYPE_TIMESTAMP, 
            pDX12TimerQuery->m_dwEndQueryIndex, 
            2, 
            pDX12Buffer->m_pD3D12Resource.Get(), 
            pDX12TimerQuery->m_dwEndQueryIndex * 8
        );

        return true;
    }
    
    BOOL FDX12CommandList::BeginMarker(const CHAR* cpcName)
    {
        PIXBeginEvent(m_pActiveCmdList->pD3D12CommandList.Get(), 0, cpcName);
        return true;
    }
    BOOL FDX12CommandList::EndMarker()
    {
        PIXEndEvent(m_pActiveCmdList->pD3D12CommandList.Get());
        return true;
    }

    BOOL FDX12CommandList::SetResourceStatesForBindingSet(IBindingSet* pBindingSet)
    {
        if (pBindingSet->IsBindless()) return false;
        FBindingSetDesc BindingSetDesc = pBindingSet->GetDesc();

        FDX12BindingSet* pDX12BindingSet = CheckedCast<FDX12BindingSet*>(pBindingSet);

        for (UINT16 ix : pDX12BindingSet->m_wBindingsWhichNeedTransition)
        {
            const auto& crBinding = BindingSetDesc.BindingItems[ix];

            switch (crBinding.Type)
            {
            case EResourceType::Texture_SRV:
                {
                    ITexture* pTexture = nullptr;
                    ReturnIfFalse(crBinding.pResource->QueryInterface(IID_ITexture, PPV_ARG(&pTexture)));
                    EResourceStates RequireState = m_Desc.QueueType == ECommandQueueType::Compute ? 
                                                   EResourceStates::NonPixelShaderResource : EResourceStates::PixelShaderResource;
                    ReturnIfFalse(RequireTextureState(pTexture, crBinding.Subresource, RequireState));
                    break;
                }
            case EResourceType::Texture_UAV:
                {
                    ITexture* pTexture = nullptr;
                    ReturnIfFalse(crBinding.pResource->QueryInterface(IID_ITexture, PPV_ARG(&pTexture)));
                    ReturnIfFalse(RequireTextureState(pTexture, crBinding.Subresource, EResourceStates::UnorderedAccess));
                    break;
                }
            case EResourceType::RawBuffer_SRV:
            case EResourceType::TypedBuffer_SRV:
            case EResourceType::StructuredBuffer_SRV:
                {
                    IBuffer* pBuffer = nullptr;
                    ReturnIfFalse(crBinding.pResource->QueryInterface(IID_IBuffer, PPV_ARG(&pBuffer)));
					EResourceStates RequireState = m_Desc.QueueType == ECommandQueueType::Compute ?
						                           EResourceStates::NonPixelShaderResource : EResourceStates::PixelShaderResource;
                    ReturnIfFalse(RequireBufferState(pBuffer, RequireState));
                    break;
                }
            case EResourceType::RawBuffer_UAV:
            case EResourceType::TypedBuffer_UAV:
            case EResourceType::StructuredBuffer_UAV:
                {
                    IBuffer* pBuffer = nullptr;
                    ReturnIfFalse(crBinding.pResource->QueryInterface(IID_IBuffer, PPV_ARG(&pBuffer)));
                    ReturnIfFalse(RequireBufferState(pBuffer, EResourceStates::UnorderedAccess));
                    break;
                }
            case EResourceType::ConstantBuffer:
                {
                    IBuffer* pBuffer = nullptr;
                    ReturnIfFalse(crBinding.pResource->QueryInterface(IID_IBuffer, PPV_ARG(&pBuffer)));
                    ReturnIfFalse(RequireBufferState(pBuffer, EResourceStates::ConstantBuffer));
                    break;
                }
            default: break;
            }
        }
        return true;
    }

    BOOL FDX12CommandList::SetResourceStatesForFramebuffer(IFrameBuffer* pFrameBuffer)
    {
        FFrameBufferDesc FrameBufferDesc = pFrameBuffer->GetDesc();

        for (UINT32 ix = 0; ix < FrameBufferDesc.ColorAttachments.Size(); ++ix)
        {
            const auto& crAttachment = FrameBufferDesc.ColorAttachments[ix];
            ReturnIfFalse(RequireTextureState(crAttachment.pTexture, crAttachment.Subresource, EResourceStates::RenderTarget));
        }
        
        if (FrameBufferDesc.DepthStencilAttachment.IsValid())
        {
            ReturnIfFalse(RequireTextureState(
                FrameBufferDesc.DepthStencilAttachment.pTexture, 
                FrameBufferDesc.DepthStencilAttachment.Subresource, 
                EResourceStates::DepthWrite
            ));
        }
    
        return true;
    }

    BOOL FDX12CommandList::SetEnableUavBarriersForTexture(ITexture* pTexture, BOOL bEnableBarriers)
    {
        ITextureStateTrack* pTextureStateTrack = nullptr;
        ReturnIfFalse(pTexture->QueryInterface(IID_ITextureStateTrack, PPV_ARG(&pTextureStateTrack)));

        m_ResourceStateTracker.SetTextureEnableUAVBarriers(pTextureStateTrack, bEnableBarriers);

        return true;
    }

    BOOL FDX12CommandList::SetEnableUavBarriersForBuffer(IBuffer* pBuffer, BOOL bEnableBarriers)
    {
        IBufferStateTrack* pBufferStateTrack = nullptr;
        ReturnIfFalse(pBuffer->QueryInterface(IID_IBufferStateTrack, PPV_ARG(&pBufferStateTrack)));

        m_ResourceStateTracker.SetBufferEnableUAVBarriers(pBufferStateTrack, bEnableBarriers);

        return true;
    }
    
    BOOL FDX12CommandList::SetTextureState(ITexture* pTexture, const FTextureSubresourceSet& crSubresourceSet, EResourceStates States)
    {
        ITextureStateTrack* pTextureStateTrack = nullptr;
        ReturnIfFalse(pTexture->QueryInterface(IID_ITextureStateTrack, PPV_ARG(&pTextureStateTrack)));

        ReturnIfFalse(m_ResourceStateTracker.RequireTextureState(pTextureStateTrack, crSubresourceSet, States));
        
        if (m_pInstance != nullptr)
        {
            m_pInstance->pReferencedResources.emplace_back(pTexture);
        }

        return true;
    }

    BOOL FDX12CommandList::SetBufferState(IBuffer* pBuffer, EResourceStates States)
    {
        IBufferStateTrack* pBufferStateTrack = nullptr;
        ReturnIfFalse(pBuffer->QueryInterface(IID_IBufferStateTrack, PPV_ARG(&pBufferStateTrack)));

        ReturnIfFalse(m_ResourceStateTracker.RequireBufferState(pBufferStateTrack, States));

        if (m_pInstance != nullptr)
        {
            m_pInstance->pReferencedResources.emplace_back(pBuffer);
        }

        return true;
    }

    void FDX12CommandList::CommitBarriers()
    {
        const auto& crTextureBarriers = m_ResourceStateTracker.GetTextureBarriers();
        const auto& crBufferBarriers = m_ResourceStateTracker.GetBufferBarriers();

        const UINT64 cstBarriersNum = crTextureBarriers.size() + crBufferBarriers.size();
        if (cstBarriersNum == 0) return;


        m_D3D12Barriers.clear();
        m_D3D12Barriers.reserve(cstBarriersNum);

        for (const auto& crBarrier : crTextureBarriers)
        {
            const D3D12_RESOURCE_STATES cD3D12StateBefore = ConvertResourceStates(crBarrier.StateBefore);
            const D3D12_RESOURCE_STATES cD3D12StateAfter = ConvertResourceStates(crBarrier.StateAfter);
            
            FDX12Texture* pDX12Texture = CheckedCast<FDX12Texture*>(crBarrier.pTexture);

            D3D12_RESOURCE_BARRIER D3D12Barrier{};
            D3D12Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            if (cD3D12StateBefore != cD3D12StateAfter)
            {
                D3D12Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                D3D12Barrier.Transition.StateBefore = cD3D12StateBefore;
                D3D12Barrier.Transition.StateAfter = cD3D12StateAfter;
                D3D12Barrier.Transition.pResource = pDX12Texture->m_pD3D12Resource.Get();

                if (crBarrier.bEntireTexture)
                {
                    D3D12Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                    m_D3D12Barriers.emplace_back(D3D12Barrier);
                }
                else 
                {
                    UINT32 dwPlaneCount = pDX12Texture->m_btPlaneCount;
                    for (UINT32 dwPlaneIndex = 0; dwPlaneIndex < dwPlaneCount; ++dwPlaneIndex)
                    {
                        D3D12Barrier.Transition.Subresource = CalcTextureSubresource(
                            crBarrier.dwMipLevel, 
                            crBarrier.dwArraySlice, 
                            dwPlaneIndex, 
                            pDX12Texture->m_Desc.dwMipLevels, 
                            pDX12Texture->m_Desc.dwArraySize
                        );

                        m_D3D12Barriers.emplace_back(D3D12Barrier);
                    }
                }
            }
            else if ((cD3D12StateAfter & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0)
            {
                // 切换 uav 的资源时也需要放入 barrier.
                D3D12Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                D3D12Barrier.UAV.pResource = pDX12Texture->m_pD3D12Resource.Get();

                m_D3D12Barriers.emplace_back(D3D12Barrier);
            }
        }

        for (const auto& crBarrier : crBufferBarriers)
        {
            const D3D12_RESOURCE_STATES cD3D12StateBefore = ConvertResourceStates(crBarrier.StateBefore);
            const D3D12_RESOURCE_STATES cD3D12StateAfter = ConvertResourceStates(crBarrier.StateAfter);
            
            FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(crBarrier.pBuffer);

            D3D12_RESOURCE_BARRIER D3D12Barrier{};
            D3D12Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            if (cD3D12StateBefore != cD3D12StateAfter)
            {
                D3D12Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                D3D12Barrier.Transition.StateBefore = cD3D12StateBefore;
                D3D12Barrier.Transition.StateAfter = cD3D12StateAfter;
                D3D12Barrier.Transition.pResource = pDX12Buffer->m_pD3D12Resource.Get();
                D3D12Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                m_D3D12Barriers.emplace_back(D3D12Barrier);
            }
            else if ((cD3D12StateAfter & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0)
            {
                // 切换 uav 的资源时也需要放入 barrier.
                D3D12Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                D3D12Barrier.UAV.pResource = pDX12Buffer->m_pD3D12Resource.Get();

                m_D3D12Barriers.emplace_back(D3D12Barrier);
            }
        }

        if (!m_D3D12Barriers.empty())
        {
            m_pActiveCmdList->pD3D12CommandList->ResourceBarrier(static_cast<UINT32>(m_D3D12Barriers.size()), m_D3D12Barriers.data());
        }

        m_ResourceStateTracker.ClearBarriers();
    }
    
    BOOL FDX12CommandList::GetTextureSubresourceState(
        ITexture* pTexture, 
        UINT32 dwArraySlice, 
        UINT32 dwMipLevel, 
        EResourceStates* pResourceStates
    )
    {
        if (pResourceStates == nullptr) return false;

        ITextureStateTrack* pTextureStateTrack = nullptr;
        ReturnIfFalse(pTexture->QueryInterface(IID_ITextureStateTrack, PPV_ARG(&pTextureStateTrack)));

        *pResourceStates = m_ResourceStateTracker.GetTextureSubresourceState(pTextureStateTrack, dwArraySlice, dwMipLevel);
        
        return true;
    }
    
    BOOL FDX12CommandList::GetBufferState(IBuffer* pBuffer, EResourceStates* pResourceStates)
    {
        if (pResourceStates == nullptr) return false;

        IBufferStateTrack* pBufferStateTrack = nullptr;
        ReturnIfFalse(pBuffer->QueryInterface(IID_IBufferStateTrack, PPV_ARG(&pBufferStateTrack)));

        *pResourceStates = m_ResourceStateTracker.GetBufferSubresourceState(pBufferStateTrack);
        
        return true;
    }

    IDevice* FDX12CommandList::GetDevice()
    {
        return m_pDevice;
    }

    FCommandListDesc FDX12CommandList::GetDesc()
    {
        return m_Desc;
    }

    void* FDX12CommandList::GetNativeObject()
    {
        return static_cast<void*>(m_pActiveCmdList->pD3D12CommandList.Get());
    }


#ifdef RAY_TRACING

    BOOL FDX12CommandList::SetRayTracingState(const RayTracing::FPipelineState& crState)
    {
        return false;
    }

    BOOL FDX12CommandList::DispatchRays(const RayTracing::FDispatchRaysArguments& crArguments)
    {
        ReturnIfFalse(m_bCurrRayTracingStateValid && UpdateComputeVolatileBuffers());

        D3D12_DISPATCH_RAYS_DESC Desc = GetShaderTableState(m_CurrRayTracingState.pShaderTable)->DispatchRaysDesc;
        Desc.Width = crArguments.dwWidth;
        Desc.Height = crArguments.dwHeight;
        Desc.Depth = crArguments.dwDepth;

        m_pActiveCmdList->pD3D12CommandList4->DispatchRays(&Desc);
        return true;
    }
    
    BOOL FDX12CommandList::BuildBottomLevelAccelStruct(
        RayTracing::IAccelStruct* pAccelStruct,
        const RayTracing::FGeometryDesc* pGeometryDescs,
        UINT32 dwGeometryDescNum
    )
    {
        RayTracing::FDX12AccelStruct* pDX12AccelStruct = CheckedCast<RayTracing::FDX12AccelStruct*>(pAccelStruct);
        auto& rDesc = pDX12AccelStruct->m_Desc;
        ReturnIfFalse(!rDesc.bIsTopLevel);

        BOOL bPerformUpdate = (rDesc.Flags & RayTracing::EAccelStructBuildFlags::PerformUpdate) != 0;

        rDesc.BottomLevelGeometryDescs.clear();
        rDesc.BottomLevelGeometryDescs.reserve(dwGeometryDescNum);
        for (UINT32 ix = 0; ix < dwGeometryDescNum; ++ix)
        {
            const auto& crGeometryDesc = pGeometryDescs[ix];
            rDesc.BottomLevelGeometryDescs[ix] = crGeometryDesc;

            if (crGeometryDesc.Type == RayTracing::EGeometryType::Triangle)
            {
                ReturnIfFalse(RequireBufferState(crGeometryDesc.Triangles.pVertexBuffer, EResourceStates::AccelStructBuildInput));
                ReturnIfFalse(RequireBufferState(crGeometryDesc.Triangles.pIndexBuffer, EResourceStates::AccelStructBuildInput));

                m_pInstance->pReferencedResources.push_back(crGeometryDesc.Triangles.pVertexBuffer);
                m_pInstance->pReferencedResources.push_back(crGeometryDesc.Triangles.pIndexBuffer);
            }
            else 
            {
                ReturnIfFalse(RequireBufferState(crGeometryDesc.AABBs.pBuffer, EResourceStates::AccelStructBuildInput));
                m_pInstance->pReferencedResources.push_back(crGeometryDesc.AABBs.pBuffer);
            }

        }

        CommitBarriers();


        RayTracing::FDX12AccelStructBuildInputs BuildInputs;
        BuildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        BuildInputs.dwDescNum = static_cast<UINT32>(rDesc.BottomLevelGeometryDescs.size());
        BuildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY_OF_POINTERS;
        BuildInputs.GeometryDescs.resize(rDesc.BottomLevelGeometryDescs.size());
        BuildInputs.pGeometryDescs.resize(rDesc.BottomLevelGeometryDescs.size());
        for (UINT32 ix = 0; ix < static_cast<UINT32>(BuildInputs.GeometryDescs.size()); ++ix)
        {
            BuildInputs.pGeometryDescs[ix] = BuildInputs.GeometryDescs.data() + ix;
        }
        BuildInputs.cpcpGeometryDesc = BuildInputs.pGeometryDescs.data();


        BuildInputs.GeometryDescs.resize(dwGeometryDescNum);
        for (UINT32 ix = 0; ix < static_cast<UINT32>(rDesc.BottomLevelGeometryDescs.size()); ++ix)
        {
            const auto& crGeometryDesc = rDesc.BottomLevelGeometryDescs[ix];
            D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = 0;
            if (crGeometryDesc.bUseTransform)
            {
                UINT8* CpuVA = nullptr;
                ReturnIfFalse(!m_UploadManager.SuballocateBuffer(
                    sizeof(RayTracing::FAffineMatrix), 
                    nullptr, 
                    nullptr, 
                    &CpuVA, 
                    &GpuAddress, 
                    m_stRecordingVersion, 
                    D3D12_RAYTRACING_TRANSFORM3X4_BYTE_ALIGNMENT
                ));

                memcpy(CpuVA, &crGeometryDesc.bUseTransform, sizeof(RayTracing::FAffineMatrix));
            }

            RayTracing::FDX12GeometryDesc& rOutGeometryDesc = BuildInputs.GeometryDescs[ix];
            RayTracing::FDX12AccelStruct::FillGeometryDesc(rOutGeometryDesc, crGeometryDesc, GpuAddress);
        }

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = pDX12AccelStruct->GetAccelStructPrebuildInfo();

        ReturnIfFalse(ASPreBuildInfo.ResultDataMaxSizeInBytes <= pDX12AccelStruct->m_pDataBuffer->GetDesc().stByteSize);

        UINT64 stScratchSize = bPerformUpdate ? 
            ASPreBuildInfo.UpdateScratchDataSizeInBytes :
            ASPreBuildInfo.ScratchDataSizeInBytes;

        D3D12_GPU_VIRTUAL_ADDRESS ScratchGPUAddress{};
        ReturnIfFalse(m_DxrScratchManager.SuballocateBuffer(
            stScratchSize, 
            nullptr, 
            nullptr, 
            nullptr, 
            &ScratchGPUAddress, 
            m_stRecordingVersion, 
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT
        ));

        RequireBufferState(pDX12AccelStruct->m_pDataBuffer.Get(), EResourceStates::AccelStructWrite);
        CommitBarriers();

        D3D12_GPU_VIRTUAL_ADDRESS ASDataAddress = CheckedCast<FDX12Buffer*>(pDX12AccelStruct->m_pDataBuffer.Get())->m_GpuAddress;

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC ASBuildDesc = {};
        ASBuildDesc.Inputs = BuildInputs.Convert();
        ASBuildDesc.ScratchAccelerationStructureData = ScratchGPUAddress;
        ASBuildDesc.DestAccelerationStructureData = ASDataAddress;
        ASBuildDesc.SourceAccelerationStructureData = bPerformUpdate ? ASDataAddress : 0;
        m_pActiveCmdList->pD3D12CommandList4->BuildRaytracingAccelerationStructure(&ASBuildDesc, 0, nullptr);

        m_pInstance->pReferencedResources.push_back(pAccelStruct);

        return true;
    }

    BOOL FDX12CommandList::BuildTopLevelAccelStruct(
        RayTracing::IAccelStruct* pAccelStruct, 
        const RayTracing::FInstanceDesc* cpInstanceDescs, 
        UINT32 dwInstanceNum
    )
    {
        RayTracing::FDX12AccelStruct* pDX12AccelStruct = CheckedCast<RayTracing::FDX12AccelStruct*>(pAccelStruct);
        const auto& crDesc = pDX12AccelStruct->GetDesc();
        ReturnIfFalse(crDesc.bIsTopLevel);

        pDX12AccelStruct->m_DxrInstanceDescs.resize(dwInstanceNum);
        pDX12AccelStruct->m_pBottomLevelAccelStructs.clear();
        
        for (UINT32 ix = 0; ix < dwInstanceNum; ++ix)
        {
            const auto& crInstanceDesc = cpInstanceDescs[ix];
            auto& rD3D12InstanceDesc = pDX12AccelStruct->m_DxrInstanceDescs[ix];

            if (crInstanceDesc.pBottomLevelAS)
            {
                pDX12AccelStruct->m_pBottomLevelAccelStructs.emplace_back(crInstanceDesc.pBottomLevelAS);
                RayTracing::FDX12AccelStruct* pDX12Blas = CheckedCast<RayTracing::FDX12AccelStruct*>(crInstanceDesc.pBottomLevelAS);
                
                rD3D12InstanceDesc = RayTracing::ConvertInstanceDesc(crInstanceDesc);
                rD3D12InstanceDesc.AccelerationStructure = CheckedCast<FDX12Buffer*>(pDX12Blas->m_pDataBuffer.Get())->m_GpuAddress;
                ReturnIfFalse(RequireBufferState(pDX12Blas->m_pDataBuffer.Get(), EResourceStates::AccelStructBuildBlas));
            }
            else 
            {
                rD3D12InstanceDesc.AccelerationStructure = D3D12_GPU_VIRTUAL_ADDRESS{0};
            }
        }

        UINT64 stUploadSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * pDX12AccelStruct->m_DxrInstanceDescs.size();
        D3D12_GPU_VIRTUAL_ADDRESS GPUAddress{};
        UINT8* CPUAddress = nullptr;
        ReturnIfFalse(m_UploadManager.SuballocateBuffer(
            stUploadSize, 
            nullptr, 
            nullptr, 
            &CPUAddress, 
            &GPUAddress, 
            m_stRecordingVersion, 
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
        ));

        memcpy(CPUAddress, pDX12AccelStruct->m_DxrInstanceDescs.data(), stUploadSize);

        ReturnIfFalse(RequireBufferState(pDX12AccelStruct->m_pDataBuffer.Get(), EResourceStates::AccelStructWrite));
        CommitBarriers();

        BOOL bPerformUpdate = (crDesc.Flags & RayTracing::EAccelStructBuildFlags::AllowUpdate) != 0;

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs;
        ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        ASInputs.InstanceDescs = GPUAddress;
        ASInputs.NumDescs = dwInstanceNum;
        ASInputs.Flags = RayTracing::ConvertAccelStructureBuildFlags(crDesc.Flags);
        if (bPerformUpdate) ASInputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
        m_cpContext->pDevice5->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

        ReturnIfFalse(ASPreBuildInfo.ResultDataMaxSizeInBytes <= pDX12AccelStruct->m_pDataBuffer->GetDesc().stByteSize);

        UINT64 stScratchSize = bPerformUpdate ? 
            ASPreBuildInfo.UpdateScratchDataSizeInBytes :
            ASPreBuildInfo.ScratchDataSizeInBytes;

        D3D12_GPU_VIRTUAL_ADDRESS ScratchGPUAddress{};
        ReturnIfFalse(m_DxrScratchManager.SuballocateBuffer(
            stScratchSize, 
            nullptr, 
            nullptr, 
            nullptr, 
            &ScratchGPUAddress, 
            m_stRecordingVersion, 
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT
        ));


        D3D12_GPU_VIRTUAL_ADDRESS ASDataAddress = CheckedCast<FDX12Buffer*>(pDX12AccelStruct->m_pDataBuffer.Get())->m_GpuAddress;

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC Desc = {};
        Desc.Inputs = ASInputs;
        Desc.ScratchAccelerationStructureData = ScratchGPUAddress;
        Desc.DestAccelerationStructureData = ASDataAddress;
        Desc.SourceAccelerationStructureData = bPerformUpdate ? ASDataAddress : 0;

        m_pActiveCmdList->pD3D12CommandList4->BuildRaytracingAccelerationStructure(&Desc, 0, nullptr);

        m_pInstance->pReferencedResources.push_back(pAccelStruct);

        return true;
    }

    BOOL FDX12CommandList::SetAccelStructState(RayTracing::IAccelStruct* pAccelStruct, EResourceStates State)
    {
        IBufferStateTrack* pBufferStateTrack = nullptr;
        ReturnIfFalse(CheckedCast<RayTracing::FDX12AccelStruct*>(pAccelStruct)->m_pDataBuffer->QueryInterface(
            IID_IBufferStateTrack, 
            PPV_ARG(&pBufferStateTrack)
        ));

        ReturnIfFalse(m_ResourceStateTracker.RequireBufferState(pBufferStateTrack, State));

        if (m_pInstance != nullptr)
        {
            m_pInstance->pReferencedResources.emplace_back(pAccelStruct);
        }
        return true;
    }

    RayTracing::FDX12ShaderTableState* FDX12CommandList::GetShaderTableState(RayTracing::IShaderTable* pShaderTable)
    {
        auto Iter = m_ShaderTableStatesMap.find(pShaderTable);
        if (Iter != m_ShaderTableStatesMap.end()) return Iter->second.get();

        std::unique_ptr<RayTracing::FDX12ShaderTableState> pShaderTableState = std::make_unique<RayTracing::FDX12ShaderTableState>();
        auto* pRet = pShaderTableState.get();

        m_ShaderTableStatesMap[pShaderTable] = std::move(pShaderTableState);

        return pRet;
    }

#endif


    BOOL FDX12CommandList::AllocateUploadBuffer(UINT64 stSize, UINT8** ppCpuAddress, D3D12_GPU_VIRTUAL_ADDRESS* pGpuAddress)
    {
        m_UploadManager.SuballocateBuffer(stSize, nullptr, nullptr, ppCpuAddress, pGpuAddress, m_stRecordingVersion, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        
        return true;
    }

    BOOL FDX12CommandList::CommitDescriptorHeaps()
    {
        ID3D12DescriptorHeap* pD3D12SRVetcHeap = m_pDescriptorHeaps->ShaderResourceHeap.GetShaderVisibleHeap();
        ID3D12DescriptorHeap* pD3D12SamplerHeap = m_pDescriptorHeaps->SamplerHeap.GetShaderVisibleHeap();

        if (pD3D12SRVetcHeap != m_pCurrSRVetcHeap || pD3D12SamplerHeap != m_pCurrSamplerHeap)
        {
            ID3D12DescriptorHeap* ppHeaps[2] = { pD3D12SRVetcHeap, pD3D12SamplerHeap };
            m_pActiveCmdList->pD3D12CommandList->SetDescriptorHeaps(2, ppHeaps);

            m_pCurrSRVetcHeap = pD3D12SRVetcHeap;
            m_pCurrSamplerHeap = pD3D12SamplerHeap;

            m_pInstance->pReferencedNativeResources.emplace_back(pD3D12SRVetcHeap);
            m_pInstance->pReferencedNativeResources.emplace_back(pD3D12SamplerHeap);
            
            return true;
        }
        return false;
    }

    BOOL FDX12CommandList::GetBufferGpuVA(IBuffer* pBuffer, D3D12_GPU_VIRTUAL_ADDRESS* pGpuAddress)
    {
        if (pBuffer == nullptr || pGpuAddress == nullptr) return false;
        
        FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(pBuffer);

        if (pDX12Buffer->m_Desc.bIsVolatile)
        {
            *pGpuAddress = m_VolatileCBAddressMap[pBuffer];
        }
        else 
        {
            *pGpuAddress = pDX12Buffer->m_GpuAddress;
        }
    
        return true;
    }

    BOOL FDX12CommandList::UpdateGraphicsVolatileBuffers()
    {
        // If there are some volatile buffers bound, 
        // and they have been written into since the last draw or setGraphicsState, patch their views. 
        if (!m_bAnyVolatileCBWrites) return false;

        for (auto& rBinding : m_CurrGraphicsVolatileCBs)
        {
            D3D12_GPU_VIRTUAL_ADDRESS CurrGpuAddress = m_VolatileCBAddressMap[rBinding.pBuffer];
            if (CurrGpuAddress != rBinding.GpuAddress)
            {
                m_pActiveCmdList->pD3D12CommandList->SetGraphicsRootConstantBufferView(rBinding.dwBindingPoint, CurrGpuAddress);
                rBinding.GpuAddress = CurrGpuAddress;
            }
        }
        m_bAnyVolatileCBWrites = false;
        
        return true;
    }

    BOOL FDX12CommandList::UpdateComputeVolatileBuffers()
    {
        // If there are some volatile buffers bound, 
        // and they have been written into since the last draw or setComputeState, patch their views. 
        if (m_bAnyVolatileCBWrites) return false;

        for (auto& rBinding : m_CurrComputeVolatileCBs)
        {
            D3D12_GPU_VIRTUAL_ADDRESS CurrGpuAddress = m_VolatileCBAddressMap[rBinding.pBuffer];
            if (CurrGpuAddress != rBinding.GpuAddress)
            {
                m_pActiveCmdList->pD3D12CommandList->SetComputeRootConstantBufferView(rBinding.dwBindingPoint, CurrGpuAddress);
                rBinding.GpuAddress = CurrGpuAddress;
            }
        }
        m_bAnyVolatileCBWrites = false;
        
        return true;
    }

    std::shared_ptr<FDX12CommandListInstance> FDX12CommandList::Executed(ID3D12Fence* pD3D12Fence, UINT64 stLastSubmittedValue)
    {
        std::shared_ptr<FDX12CommandListInstance> pRet = m_pInstance;
        pRet->pFence = pD3D12Fence;
        pRet->stSubmittedValue = stLastSubmittedValue;

        m_pInstance.reset();

        m_pActiveCmdList->stLastSubmittedValue = stLastSubmittedValue;
        m_pCmdListPool.emplace_back(m_pActiveCmdList);
        m_pActiveCmdList.reset();

        for (const auto& p : pRet->pReferencedStagingTextures)
        {
            FDX12StagingTexture* pDX12StagingTexture = CheckedCast<FDX12StagingTexture*>(p.Get());
            pDX12StagingTexture->m_pLastUsedFence = pD3D12Fence;
            pDX12StagingTexture->m_stLastUsedFenceValue = stLastSubmittedValue;
        }
        for (const auto& p : pRet->pReferencedStagingBuffers)
        {
            FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(p.Get());
            pDX12Buffer->m_pLastUsedFence = pD3D12Fence;
            pDX12Buffer->m_stLastUsedFenceValue = stLastSubmittedValue;
        }
        for (const auto& p : pRet->pReferencedTimerQueries)
        {
            FDX12TimerQuery* pDX12TimerQuery = CheckedCast<FDX12TimerQuery*>(p.Get());
            pDX12TimerQuery->m_bStarted = true;
            pDX12TimerQuery->m_bResolved = false;
            pDX12TimerQuery->m_pD3D12Fence = pD3D12Fence;
            pDX12TimerQuery->m_stFenceCounter = stLastSubmittedValue;
        }

        UINT64 stSubmittedVersion = MakeVersion(stLastSubmittedValue, m_Desc.QueueType, true);
        m_UploadManager.SubmitChunks(m_stRecordingVersion, stSubmittedVersion);
        m_stRecordingVersion = 0;

        return pRet;
    }

    BOOL FDX12CommandList::RequireTextureState(ITexture* pTexture, const FTextureSubresourceSet& crSubresourceSet, EResourceStates State)
    {
        ITextureStateTrack* pTextureStateTrack = nullptr;
        ReturnIfFalse(pTexture->QueryInterface(IID_ITextureStateTrack, PPV_ARG(&pTextureStateTrack)));
        
        ReturnIfFalse(m_ResourceStateTracker.RequireTextureState(pTextureStateTrack, crSubresourceSet, State));

        return true;
    }

    BOOL FDX12CommandList::RequireStagingTextureState(IStagingTexture* pStagingTexture, EResourceStates State)
    {
        IBufferStateTrack* pBufferStateTrack = nullptr;
		FDX12StagingTexture* pDX12StagingTexture = CheckedCast<FDX12StagingTexture*>(pStagingTexture);
        ReturnIfFalse(pDX12StagingTexture->m_pBuffer->QueryInterface(IID_IBufferStateTrack, PPV_ARG(&pBufferStateTrack)));
        
        ReturnIfFalse(m_ResourceStateTracker.RequireBufferState(pBufferStateTrack, State));

        return true;
    }

    BOOL FDX12CommandList::RequireBufferState(IBuffer* pBuffer, EResourceStates State)
    {
        IBufferStateTrack* pBufferStateTrack = nullptr;
        ReturnIfFalse(pBuffer->QueryInterface(IID_IBufferStateTrack, PPV_ARG(&pBufferStateTrack)));
        
        ReturnIfFalse(m_ResourceStateTracker.RequireBufferState(pBufferStateTrack, State));

        return true;
    }

    BOOL FDX12CommandList::SetGraphicsBindings(
        const FPipelineStateBindingSetArray& crBindingSets,  
        UINT32 dwBindingUpdateMask, 
        IDX12RootSignature* pRootSignature
    )
    {
        if (dwBindingUpdateMask > 0)
        {
            std::vector<VolatileConstantBufferBinding> NewGraphicsVolatileCBs;

            for (UINT32 ix = 0; ix < crBindingSets.Size(); ++ix)
            {
                IBindingSet* pBindingSet = crBindingSets[ix];
                if (pBindingSet == nullptr) continue;

                // cbUpdateTheSet 放在后边做判断是为了能够进行一些判断增加容错.
                const BOOL cbUpdateTheSet = (dwBindingUpdateMask & (1 << ix)) != 0;

                FDX12RootSignature* pDX12RootSignature = CheckedCast<FDX12RootSignature*>(pRootSignature);
                const auto& crLayoutOffset = pDX12RootSignature->m_BindingLayoutIndexMap[ix];
                UINT32 dwBindingLayoutRootParamOffset = crLayoutOffset.second;

                if (!pBindingSet->IsBindless())
                {
                    if (pBindingSet->GetLayout() != crLayoutOffset.first.Get()) return false;

                    FDX12BindingSet* pDX12BindingSet = CheckedCast<FDX12BindingSet*>(pBindingSet);


                    for (UINT32 dwCBIndex = 0; dwCBIndex < pDX12BindingSet->m_RootParameterIndexVolatileCBMaps.size(); ++dwCBIndex)
                    {
                        UINT32 dwCBRootParamIndex = pDX12BindingSet->m_RootParameterIndexVolatileCBMaps[dwCBIndex].first;
                        IBuffer* pVolatileCB = pDX12BindingSet->m_RootParameterIndexVolatileCBMaps[dwCBIndex].second;

                        // Plus the offset. 
                        dwCBRootParamIndex += dwBindingLayoutRootParamOffset;

                        if (pVolatileCB != nullptr)
                        {
                            FDX12Buffer* pDX12VolatileCB = CheckedCast<FDX12Buffer*>(pVolatileCB);

                            if (pDX12VolatileCB->m_Desc.bIsVolatile)
                            {
                                D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = m_VolatileCBAddressMap[pVolatileCB];

                                if (GpuAddress == 0)
                                {
                                    LOG_ERROR("Attempted use of a volatile constant buffer before it was written into. ");
                                    return false;
                                }
                                if (cbUpdateTheSet || GpuAddress != m_CurrGraphicsVolatileCBs[NewGraphicsVolatileCBs.size()].GpuAddress)
                                {
                                    m_pActiveCmdList->pD3D12CommandList->SetGraphicsRootConstantBufferView(dwCBRootParamIndex, GpuAddress);
                                }
                                NewGraphicsVolatileCBs.emplace_back(VolatileConstantBufferBinding{ dwCBRootParamIndex, pVolatileCB, GpuAddress });
                            }
                            else if (cbUpdateTheSet)
                            {
                                D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = pDX12VolatileCB->m_GpuAddress;
                                if (GpuAddress != 0)
                                {
                                    m_pActiveCmdList->pD3D12CommandList->SetGraphicsRootConstantBufferView(dwCBRootParamIndex, GpuAddress);
                                }
                            }
                        }
                        else if (cbUpdateTheSet)
                        {
                            // This can only happen as a result of an improperly built binding set. 
                            // Such binding set should fail to create.
                            m_pActiveCmdList->pD3D12CommandList->SetGraphicsRootConstantBufferView(dwCBRootParamIndex, 0);
                        }
                    }

                    if (cbUpdateTheSet)
                    {
                        if (pDX12BindingSet->m_bDescriptorTableValidSampler)
                        {
                            m_pActiveCmdList->pD3D12CommandList->SetGraphicsRootDescriptorTable(
                                dwBindingLayoutRootParamOffset + pDX12BindingSet->m_dwRootParameterSamplerIndex,
                                m_pDescriptorHeaps->SamplerHeap.GetGpuHandleShaderVisible(pDX12BindingSet->m_dwDescriptorTableSamplerBaseIndex));
                        }

                        if (pDX12BindingSet->m_bDescriptorTableValidSRVetc)
                        {
                            m_pActiveCmdList->pD3D12CommandList->SetGraphicsRootDescriptorTable(
                                dwBindingLayoutRootParamOffset + pDX12BindingSet->m_dwRootParameterSRVetcIndex,
                                m_pDescriptorHeaps->ShaderResourceHeap.GetGpuHandleShaderVisible(pDX12BindingSet->m_dwDescriptorTableSRVetcBaseIndex));
                        }
                        
                        if (pBindingSet->IsBindless()) return false;
                        FBindingSetDesc Desc = pBindingSet->GetDesc();
                        if (Desc.bTrackLiveness)
                            m_pInstance->pReferencedResources.emplace_back(pBindingSet);
                    }

                    if ((cbUpdateTheSet || pDX12BindingSet->m_bHasUAVBindings)) // UAV bindings may place UAV barriers on the same binding set
                    {
                        SetResourceStatesForBindingSet(pBindingSet);
                    }
                    
                }
                else if (cbUpdateTheSet)
                {
                    FDX12BindlessSet* pDX12DescriptorTable = CheckedCast<FDX12BindlessSet*>(pBindingSet);

                    m_pActiveCmdList->pD3D12CommandList->SetGraphicsRootDescriptorTable(
                        dwBindingLayoutRootParamOffset, 
                        m_pDescriptorHeaps->ShaderResourceHeap.GetGpuHandleShaderVisible(pDX12DescriptorTable->m_dwFirstDescriptorIndex)
                    );
                }
            }
            m_CurrGraphicsVolatileCBs = NewGraphicsVolatileCBs;
        }

        UINT32 dwBindingMask = (1 << crBindingSets.Size()) - 1;
        if ((dwBindingUpdateMask & dwBindingMask) == dwBindingMask)
        {
            // Only reset this flag when this function has gone over all the binging sets. 
            m_bAnyVolatileCBWrites = false;
        }
        
        return true;
    }

    BOOL FDX12CommandList::SetComputeBindings(
        const FPipelineStateBindingSetArray& crBindingSets, 
        UINT32 dwBindingUpdateMask, 
        IDX12RootSignature* pRootSignature
    )
    {
        if (dwBindingUpdateMask > 0)
        {
            std::vector<VolatileConstantBufferBinding> NewComputeVolatileCBs;

            for (UINT32 ix = 0; ix < crBindingSets.Size(); ++ix)
            {
                IBindingSet* pBindingSet = crBindingSets[ix];
                if (pBindingSet == nullptr) continue;

                const BOOL cbUpdateTheSet = (dwBindingUpdateMask & (1 << ix)) != 0;

                FDX12RootSignature* pDX12RootSignature = CheckedCast<FDX12RootSignature*>(pRootSignature);
                const auto& crLayoutIndex = pDX12RootSignature->m_BindingLayoutIndexMap[ix];
                UINT32 dwRootParamOffset = crLayoutIndex.second;

                if (!pBindingSet->IsBindless())
                {
                    IBindingLayout* pTempBindingLayoutForCompare = pBindingSet->GetLayout();
                    ReturnIfFalse(pTempBindingLayoutForCompare != nullptr);
                    
                    if (pTempBindingLayoutForCompare != crLayoutIndex.first.Get()) return false;

                    FDX12BindingSet* pDX12BindingSet = CheckedCast<FDX12BindingSet*>(pBindingSet);

                    for (UINT32 dwCBIndex = 0; dwCBIndex < pDX12BindingSet->m_RootParameterIndexVolatileCBMaps.size(); ++dwCBIndex)
                    {
                        IBuffer* pVolatileCB = pDX12BindingSet->m_RootParameterIndexVolatileCBMaps[dwCBIndex].second;
                        UINT32 dwCBRootParamIndex = pDX12BindingSet->m_RootParameterIndexVolatileCBMaps[dwCBIndex].first;
                        
                        // Plus the offset. 
                        dwCBRootParamIndex += dwRootParamOffset;

                        if (pVolatileCB != nullptr)
                        {
                            FDX12Buffer* pDX12VolatileCB = CheckedCast<FDX12Buffer*>(pVolatileCB);

                            if (pDX12VolatileCB->m_Desc.bIsVolatile)
                            {
                                D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = m_VolatileCBAddressMap[pVolatileCB];

                                if (GpuAddress == 0)
                                {
                                    LOG_ERROR("Attempted use of a volatile constant buffer before it was written into. ");
                                    return false;
                                }
                                if (cbUpdateTheSet || GpuAddress != m_CurrComputeVolatileCBs[NewComputeVolatileCBs.size()].GpuAddress)
                                {
                                    m_pActiveCmdList->pD3D12CommandList->SetComputeRootConstantBufferView(dwCBRootParamIndex, GpuAddress);
                                }
                                NewComputeVolatileCBs.emplace_back(VolatileConstantBufferBinding{ dwCBRootParamIndex, pVolatileCB, GpuAddress });
                            }
                            else if (cbUpdateTheSet)
                            {
                                D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = pDX12VolatileCB->m_GpuAddress;
                                if (GpuAddress == 0)
                                {
                                    LOG_ERROR("Attempted use of a volatile constant buffer before it was written into. ");
                                    return false;
                                }
                                m_pActiveCmdList->pD3D12CommandList->SetComputeRootConstantBufferView(dwCBRootParamIndex, GpuAddress);
                            }
                        }
                        else if (cbUpdateTheSet)
                        {
                            // This can only happen as a result of an improperly built binding set. 
                            // Such binding set should fail to create.
                            m_pActiveCmdList->pD3D12CommandList->SetComputeRootConstantBufferView(dwCBRootParamIndex, 0);
                        }
                    }

                    if (cbUpdateTheSet)
                    {
                        if (pDX12BindingSet->m_bDescriptorTableValidSampler)
                        {
                            m_pActiveCmdList->pD3D12CommandList->SetComputeRootDescriptorTable(
                                dwRootParamOffset + pDX12BindingSet->m_dwRootParameterSamplerIndex,
                                m_pDescriptorHeaps->SamplerHeap.GetGpuHandleShaderVisible(pDX12BindingSet->m_dwDescriptorTableSamplerBaseIndex));
                        }

                        if (pDX12BindingSet->m_bDescriptorTableValidSRVetc)
                        {
                            m_pActiveCmdList->pD3D12CommandList->SetComputeRootDescriptorTable(
                                dwRootParamOffset + pDX12BindingSet->m_dwRootParameterSRVetcIndex,
                                m_pDescriptorHeaps->ShaderResourceHeap.GetGpuHandleShaderVisible(pDX12BindingSet->m_dwDescriptorTableSRVetcBaseIndex));
                        }
                        
                        if (pBindingSet->IsBindless()) return false;
                        FBindingSetDesc Desc = pBindingSet->GetDesc();
                        if (Desc.bTrackLiveness)
                            m_pInstance->pReferencedResources.emplace_back(pBindingSet);
                    }

                    if ((cbUpdateTheSet || pDX12BindingSet->m_bHasUAVBindings)) // UAV bindings may place UAV barriers on the same binding set
                    {
                        SetResourceStatesForBindingSet(pBindingSet);
                    }
                }
                else
                {
                    FDX12BindlessSet* pDX12DescriptorTable = CheckedCast<FDX12BindlessSet*>(pBindingSet);

                    m_pActiveCmdList->pD3D12CommandList->SetComputeRootDescriptorTable(
                        crLayoutIndex.second, 
                        m_pDescriptorHeaps->ShaderResourceHeap.GetGpuHandleShaderVisible(pDX12DescriptorTable->m_dwFirstDescriptorIndex)
                    );
                }
            }
            m_CurrComputeVolatileCBs = NewComputeVolatileCBs;
        }

        UINT32 dwBindingMask = (1 << crBindingSets.Size()) - 1;
        if ((dwBindingUpdateMask & dwBindingMask) == dwBindingMask)
        {
            // Only reset this flag when this function has gone over all the binging sets. 
            m_bAnyVolatileCBWrites = false;
        }
        
        return true;
    }

    void FDX12CommandList::ClearStateCache()
    {
        m_bAnyVolatileCBWrites = false;
        m_bCurrGraphicsStateValid = false;
        m_bCurrComputeStateValid = false;
        m_pCurrSRVetcHeap = nullptr;
        m_pCurrSamplerHeap = nullptr;
        m_CurrGraphicsVolatileCBs.clear();
        m_CurrComputeVolatileCBs.clear();
    }

    BOOL FDX12CommandList::BindGraphicsPipeline(IGraphicsPipeline* pGraphicsPipeline, BOOL bUpdateRootSignature) const
    {
        if (pGraphicsPipeline == nullptr) return false;

        FDX12GraphicsPipeline* pDX12GraphicsPipeline = CheckedCast<FDX12GraphicsPipeline*>(pGraphicsPipeline);

        FGraphicsPipelineDesc Desc = pGraphicsPipeline->GetDesc();
        
        if (bUpdateRootSignature)
        {
            FDX12RootSignature* pDX12RootSignature = CheckedCast<FDX12RootSignature*>(pDX12GraphicsPipeline->m_pDX12RootSignature.Get());
            m_pActiveCmdList->pD3D12CommandList->SetGraphicsRootSignature(pDX12RootSignature->m_pD3D12RootSignature.Get());
        }

        m_pActiveCmdList->pD3D12CommandList->SetPipelineState(pDX12GraphicsPipeline->m_pD3D12PipelineState.Get());

        m_pActiveCmdList->pD3D12CommandList->IASetPrimitiveTopology(ConvertPrimitiveType(Desc.PrimitiveType, Desc.dwPatchControlPoints));

        return true;
    }
    
    BOOL FDX12CommandList::BindFrameBuffer(IFrameBuffer* pFrameBuffer)
    {
		SetResourceStatesForFramebuffer(pFrameBuffer);

        FDX12FrameBuffer* pDX12FrameBuffer = CheckedCast<FDX12FrameBuffer*>(pFrameBuffer);

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> Rtvs;
        for (UINT32 ix : pDX12FrameBuffer->m_dwRTVIndices)
        {
            Rtvs.emplace_back(m_pDescriptorHeaps->RenderTargetHeap.GetCpuHandle(ix));
        }

        BOOL bHasDepthStencil = pDX12FrameBuffer->m_dwDSVIndex != gdwInvalidViewIndex;
        D3D12_CPU_DESCRIPTOR_HANDLE Dsv;
        if (bHasDepthStencil)
        {
            Dsv = m_pDescriptorHeaps->DepthStencilHeap.GetCpuHandle(pDX12FrameBuffer->m_dwDSVIndex);
        }

        m_pActiveCmdList->pD3D12CommandList->OMSetRenderTargets(
            static_cast<UINT32>(pDX12FrameBuffer->m_dwRTVIndices.size()),
            Rtvs.data(),
            false,
            bHasDepthStencil ? &Dsv : nullptr
        );

        return true;
    }
    
    std::shared_ptr<FDX12InternalCommandList> FDX12CommandList::CreateInternalCmdList() const
    {
        std::shared_ptr<FDX12InternalCommandList> Ret = std::make_shared<FDX12InternalCommandList>();

        D3D12_COMMAND_LIST_TYPE CmdListType;
        switch (m_Desc.QueueType)
        {
        case ECommandQueueType::Graphics: CmdListType = D3D12_COMMAND_LIST_TYPE_DIRECT; break;
        case ECommandQueueType::Compute:  CmdListType = D3D12_COMMAND_LIST_TYPE_COMPUTE; break;
        case ECommandQueueType::Copy:     CmdListType = D3D12_COMMAND_LIST_TYPE_COPY; break;
        default: 
            assert(!"Invalid Enumeration Value");
            return nullptr;
        }

        m_cpContext->pDevice->CreateCommandAllocator(CmdListType, IID_PPV_ARGS(Ret->pCmdAllocator.GetAddressOf()));
        m_cpContext->pDevice->CreateCommandList(0, CmdListType, Ret->pCmdAllocator.Get(), nullptr, IID_PPV_ARGS(Ret->pD3D12CommandList.GetAddressOf()));

#ifdef RAY_TRACING
        Ret->pD3D12CommandList->QueryInterface(IID_PPV_ARGS(Ret->pD3D12CommandList4.GetAddressOf()));
#endif
        Ret->pD3D12CommandList->QueryInterface(IID_PPV_ARGS(Ret->pD3D12CommandList6.GetAddressOf()));

        return Ret;
    }
}
