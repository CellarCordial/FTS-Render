#ifndef RHI_FRAME_BUFFER_H
#define RHI_FRAME_BUFFER_H

#include "Format.h"
#include "Forward.h"
#include "Resource.h"
#include "StackArray.h"

namespace FTS
{
    class ICommandList;

    struct FFrameBufferAttachment
    {
        ITexture* pTexture = nullptr;
        FTextureSubresourceSet Subresource;
        EFormat Format = EFormat::UNKNOWN;
        BOOL bIsReadOnly = false;

        BOOL IsValid() const { return pTexture != nullptr; }

        static FFrameBufferAttachment CreateAttachment(ITexture* pTexture)
        {
            FFrameBufferAttachment Ret;
            Ret.pTexture = pTexture;
            Ret.Format = pTexture->GetDesc().Format;
            return Ret;
        }
    };

    using FFrameBufferAttachmentArray = TStackArray<FFrameBufferAttachment, gdwMaxRenderTargets>;

    struct FFrameBufferDesc
    {
        FFrameBufferAttachmentArray ColorAttachments;
        FFrameBufferAttachment DepthStencilAttachment;
    };

    using FRenderTargetFormatArray = TStackArray<EFormat, gdwMaxRenderTargets>;

    struct FFrameBufferInfo
    {
        FRenderTargetFormatArray RTVFormats;
        EFormat DepthFormat = EFormat::UNKNOWN;

        UINT32 dwSampleCount = 1;
        UINT32 dwSampleQuality = 0;

        UINT32 dwWidth = 0;
        UINT32 dwHeight = 0;


        FViewport GetViewport(FLOAT fMinZ, FLOAT fMaxZ)
        {
            return FViewport { 0.f, static_cast<FLOAT>(dwWidth), 0.f, static_cast<FLOAT>(dwHeight), fMinZ, fMaxZ };
        }

        FFrameBufferInfo() = default;
        explicit FFrameBufferInfo(const FFrameBufferDesc& crDesc);
    };


    extern const IID IID_IFrameBuffer;

    struct IFrameBuffer : public IResource
    {
        virtual FFrameBufferDesc GetDesc() const = 0;
        virtual FFrameBufferInfo GetInfo() const = 0;

		virtual ~IFrameBuffer() = default;
    };

    BOOL ClearColorAttachment(ICommandList* pCmdList, IFrameBuffer* pFramebuffer, UINT32 dwAttachmentIndex);

    BOOL ClearDepthStencilAttachment(ICommandList* pCmdList, IFrameBuffer* pFramebuffer);
    
}












#endif