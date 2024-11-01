#include "DX12FrameBuffer.h"
#include "DX12Resource.h"
#include <sstream>

namespace FTS 
{
    FDX12FrameBuffer::FDX12FrameBuffer(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps, const FFrameBufferDesc& crDesc) :
        m_cpContext(cpContext), 
        m_pDescriptorHeaps(pDescriptorHeaps), 
        m_Desc(crDesc), 
        m_Info(crDesc)
    {
    }

    FDX12FrameBuffer::~FDX12FrameBuffer() noexcept
    {
        for (UINT32 dwRTVIndex : m_dwRTVIndices)
        {
            m_pDescriptorHeaps->RenderTargetHeap.ReleaseDescriptor(dwRTVIndex);
        }

        if (m_dwDSVIndex != gdwInvalidViewIndex)
        {
            m_pDescriptorHeaps->DepthStencilHeap.ReleaseDescriptor(m_dwDSVIndex);
        }
    }

    BOOL FDX12FrameBuffer::Initialize()
    {
        if (m_Desc.ColorAttachments.Size() != 0)
        {
            FTextureDesc RtvDesc = m_Desc.ColorAttachments[0].pTexture->GetDesc();
            m_dwWidth = RtvDesc.dwWidth;
            m_dwHeight = RtvDesc.dwHeight;
        }
        else if (m_Desc.DepthStencilAttachment.pTexture != nullptr)
        {
            FTextureDesc DsvDesc = m_Desc.DepthStencilAttachment.pTexture->GetDesc();
            m_dwWidth = DsvDesc.dwWidth;
            m_dwHeight = DsvDesc.dwHeight;
        }
 
        for (UINT32 ix = 0; ix < m_Desc.ColorAttachments.Size(); ++ix)
        {
            auto& rAttachment = m_Desc.ColorAttachments[ix];
            
            ITexture* pTexture = rAttachment.pTexture;
            ReturnIfFalse(pTexture != nullptr);

            FTextureDesc TextureDesc = pTexture->GetDesc();
            if (TextureDesc.dwWidth != m_dwWidth || TextureDesc.dwHeight != m_dwHeight)
            {
                std::stringstream ss;
                ss  << "The render target which index is " << ix
                    << " has different size texture with frame buffer."
                    << "Render target width is " << TextureDesc.dwWidth << "."
                    << "              height is " << TextureDesc.dwHeight << "."
                    << "Frame buffer width is " << m_dwWidth << "."
                    << "             height is " << m_dwHeight << ".";
                LOG_ERROR(ss.str());
                return false;
            }

            UINT32 dwRtvIndex = m_pDescriptorHeaps->RenderTargetHeap.AllocateDescriptor();
            D3D12_CPU_DESCRIPTOR_HANDLE View = m_pDescriptorHeaps->RenderTargetHeap.GetCpuHandle(dwRtvIndex);

            FDX12Texture* pDX12Texture = CheckedCast<FDX12Texture*>(pTexture);
            pDX12Texture->CreateRTV(View.ptr, rAttachment.Format, rAttachment.Subresource);

            m_dwRTVIndices.push_back(dwRtvIndex);
            m_pRefTextures.emplace_back(pTexture);
        }

        if (m_Desc.DepthStencilAttachment.pTexture != nullptr)
        {
            ITexture* pTexture = m_Desc.DepthStencilAttachment.pTexture;
			ReturnIfFalse(pTexture != nullptr);

            FTextureDesc TextureDesc = pTexture->GetDesc();
            if (TextureDesc.dwWidth != m_dwWidth || TextureDesc.dwHeight != m_dwHeight)
            {
                std::stringstream ss;
                ss  << "Depth buffer has different size texture with frame buffer."
                    << "Depth buffer width is " << TextureDesc.dwWidth << "."
                    << "              height is " << TextureDesc.dwHeight << "."
                    << "Frame buffer width is " << m_dwWidth << "."
                    << "             height is " << m_dwHeight << ".";
                LOG_ERROR(ss.str());
                return false;
            }

            UINT32 dwDsvIndex = m_pDescriptorHeaps->DepthStencilHeap.AllocateDescriptor();
            D3D12_CPU_DESCRIPTOR_HANDLE View = m_pDescriptorHeaps->DepthStencilHeap.GetCpuHandle(dwDsvIndex);

            FDX12Texture* pDX12Texture = CheckedCast<FDX12Texture*>(pTexture);
            pDX12Texture->CreateDSV(View.ptr, m_Desc.DepthStencilAttachment.Subresource, m_Desc.DepthStencilAttachment.bIsReadOnly);

            m_dwDSVIndex = dwDsvIndex;
            m_pRefTextures.emplace_back(pTexture);
        }
        return true;
    }

}
