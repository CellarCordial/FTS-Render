#include "../include/FrameBuffer.h"
#include <algorithm>
#include "../include/CommandList.h"

namespace FTS
{

    FFrameBufferInfo::FFrameBufferInfo(const FFrameBufferDesc& crDesc)
    {
        for (const auto& crAttachment : crDesc.ColorAttachments)
        {
            RTVFormats.PushBack(crAttachment.Format == EFormat::UNKNOWN && crAttachment.pTexture ? crAttachment.pTexture->GetDesc().Format : crAttachment.Format);
        }
        
        if (crDesc.DepthStencilAttachment.IsValid())
        {
            FTextureDesc TextureDesc = crDesc.DepthStencilAttachment.pTexture->GetDesc();
            DepthFormat = crDesc.DepthStencilAttachment.Format == EFormat::UNKNOWN ? TextureDesc.Format : crDesc.DepthStencilAttachment.Format;
            
            dwSampleCount = TextureDesc.dwSampleCount;
            dwSampleQuality = TextureDesc.dwSampleQuality;
            dwWidth = std::max(TextureDesc.dwWidth >> crDesc.DepthStencilAttachment.Subresource.dwBaseMipLevelIndex, 1u);
            dwHeight = std::max(TextureDesc.dwHeight >> crDesc.DepthStencilAttachment.Subresource.dwBaseMipLevelIndex, 1u);
        }
        else if (!crDesc.ColorAttachments.Empty() && crDesc.ColorAttachments[0].IsValid())
        {   
            FTextureDesc TextureDesc = crDesc.ColorAttachments[0].pTexture->GetDesc();
            dwSampleCount = TextureDesc.dwSampleCount;
            dwSampleQuality = TextureDesc.dwSampleQuality;
            dwWidth = std::max(TextureDesc.dwWidth >> crDesc.ColorAttachments[0].Subresource.dwBaseMipLevelIndex, 1u);
            dwHeight = std::max(TextureDesc.dwHeight >> crDesc.ColorAttachments[0].Subresource.dwBaseMipLevelIndex, 1u);
        }
        else 
        {
            assert(!"Create FrameBuffer without any attachments.");
        }
    }

    BOOL ClearColorAttachment(ICommandList* pCmdList, IFrameBuffer* pFramebuffer, UINT32 dwAttachmentIndex)
    {
        const auto& crAttachment = pFramebuffer->GetDesc().ColorAttachments[dwAttachmentIndex];
        if (crAttachment.IsValid() && pCmdList->ClearTextureFloat(crAttachment.pTexture, crAttachment.Subresource, crAttachment.pTexture->GetDesc().ClearValue))
        {
            return true;
        }
        return false;
    }

    BOOL ClearDepthStencilAttachment(ICommandList* pCmdList, IFrameBuffer* pFramebuffer)
    {
        const auto& crAttachment = pFramebuffer->GetDesc().DepthStencilAttachment;
        FColor ClearValue = crAttachment.pTexture->GetDesc().ClearValue;
        if (crAttachment.IsValid() && pCmdList->ClearDepthStencilTexture(crAttachment.pTexture, crAttachment.Subresource, true, ClearValue.r, true, static_cast<UINT8>(ClearValue.g)))
        {
            return true;
        }
        return false;
    }
}
