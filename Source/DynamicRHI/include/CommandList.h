#ifndef RHI_COMMAND_LIST_H
#define RHI_COMMAND_LIST_H

#include "Draw.h"

namespace FTS
{
    enum class ECommandQueueType : UINT8
    {
        Graphics = 0,
        Compute,
        Copy,

        Count
    };

    struct FCommandListDesc
    {
        UINT64 stUploadChunkSize = 64 * 1024;

        ECommandQueueType QueueType = ECommandQueueType::Graphics;
    };

    extern const IID IID_ICommandList;

    struct NO_VTABLE ICommandList : public IUnknown
    {
    public:
        virtual BOOL Open() = 0;
        virtual BOOL Close() = 0;

        virtual BOOL ClearState() = 0;

        virtual BOOL ClearTextureFloat(ITexture* pTexture, const FTextureSubresourceSet& crSubresourcesSet, const FColor& crClearColor) = 0;
        
        virtual BOOL ClearTextureUInt(
            ITexture* pTexture,
            const FTextureSubresourceSet& crSubresourcesSet,
            UINT32 dwClearColor
        ) = 0;
        
        virtual BOOL ClearDepthStencilTexture(
            ITexture* pTexture,
            const FTextureSubresourceSet& crSubresourcesSet,
            BOOL bClearDepth,
            FLOAT fDepth,
            BOOL bClearStencil,
            UINT8 btStencil
        ) = 0;

        virtual BOOL CopyTexture(
            ITexture* pDst,
            const FTextureSlice& crDstSlice,
            ITexture* pSrc,
            const FTextureSlice& crSrcSlice
        ) = 0;
        
        virtual BOOL CopyTexture(
            IStagingTexture* pDst,
            const FTextureSlice& crDstSlice,
            ITexture* pSrc,
            const FTextureSlice& crSrcSlice
        ) = 0;
        
        virtual BOOL CopyTexture(
            ITexture* pDst,
            const FTextureSlice& crDstSlice,
            IStagingTexture* pSrc,
            const FTextureSlice& crSrcSlice
        ) = 0;
        
        virtual BOOL WriteTexture(
            ITexture* pDst,
            UINT32 dwArraySlice,
            UINT32 dwMipLevel,
            const UINT8* cpData,
            UINT64 stRowPitch,
            UINT64 stDepthPitch = 0
        ) = 0;
        
        virtual BOOL ResolveTexture(
            ITexture* pDst,
            const FTextureSubresourceSet& crDstSubresource,
            ITexture* pSrc,
            const FTextureSubresourceSet& crSrcSubresource
        ) = 0;

        virtual BOOL WriteBuffer(IBuffer* pBuffer, const void* cpvData, UINT64 stDataSize, UINT64 stDstOffsetBytes = 0) = 0;
        
        virtual BOOL ClearBufferUInt(IBuffer* pBuffer, UINT32 dwClearValue) = 0;
        
        virtual BOOL CopyBuffer(
            IBuffer* pDst,
            UINT64 stDstOffsetBytes,
            IBuffer* pSrc,
            UINT64 stSrcOffsetBytes,
            UINT64 stDataSizeBytes
        ) = 0;

        virtual BOOL SetPushConstants(const void* cpvData, UINT64 stByteSize) = 0;
        virtual BOOL SetGraphicsState(const FGraphicsState& crState) = 0;
        virtual BOOL SetComputeState(const FComputeState& crState) = 0;
        
        virtual BOOL Draw(const FDrawArguments& crArgs) = 0;
        virtual BOOL DrawIndexed(const FDrawArguments& crArgs) = 0;
        virtual BOOL Dispatch(UINT32 dwGroupsX, UINT32 dwGroupsY = 1, UINT32 dwGroupsZ = 1) = 0;
        
        virtual BOOL BeginTimerQuery(ITimerQuery* pQuery) = 0;
        virtual BOOL EndTimerQuery(ITimerQuery* pQuery) = 0;

        virtual BOOL BeginMarker(const CHAR* cpcName) = 0;
        virtual BOOL EndMarker() = 0;

        virtual BOOL SetEnableUavBarriersForTexture(ITexture* pTexture, BOOL bEnableBarriers) = 0;
        virtual BOOL SetEnableUavBarriersForBuffer(IBuffer* pBuffer, BOOL bEnableBarriers) = 0;
        virtual BOOL SetTextureState(ITexture* pTexture, const FTextureSubresourceSet& crSubresourcesSet, EResourceStates States) = 0;
        virtual BOOL SetBufferState(IBuffer* pBuffer, EResourceStates States) = 0;

        virtual void CommitBarriers() = 0;

		virtual BOOL BindFrameBuffer(IFrameBuffer* pFrameBuffer) = 0;
		virtual BOOL CommitDescriptorHeaps() = 0;

        virtual BOOL GetTextureSubresourceState(
            ITexture* pTexture,
            UINT32 dwArraySlice,
            UINT32 dwMipLevel,
            EResourceStates* pResourceStates
        ) = 0;
        
        virtual BOOL GetBufferState(IBuffer* pBuffer, /*Out*/EResourceStates* pResourceStates) = 0;

        virtual IDevice* GetDevice() = 0;
        
        virtual FCommandListDesc GetDesc() = 0;
        

        virtual void* GetNativeObject() = 0;
        
    };
}


























#endif