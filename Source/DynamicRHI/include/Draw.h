#ifndef RHI_DRAW_H
#define RHI_DRAW_H

#include "Descriptor.h"
#include "Forward.h"
#include "Pipeline.h"
#include "Resource.h"

namespace FTS
{

    extern const IID IID_ITimerQuery;

    struct NO_VTABLE ITimerQuery : public IUnknown
    {
    };


    extern const IID IID_IEventQuery;

    struct NO_VTABLE IEventQuery : public IUnknown
    {
        
    };

    

    struct FVertexBufferBinding
    {
        IBuffer* pBuffer = nullptr;
        UINT32 dwSlot = 0;
        UINT64 stOffset = 0;

        BOOL operator==(const FVertexBufferBinding& crBinding) const
        {
            return  pBuffer == crBinding.pBuffer &&
                    stOffset == crBinding.stOffset &&
                    dwSlot == crBinding.dwSlot;
        }

        BOOL operator!=(const FVertexBufferBinding& crBinding) const
        {
            return !((*this) == crBinding);
        }

    };

    using FVertexBufferBindingArray = TStackArray<FVertexBufferBinding, gdwMaxVertexAttributes>;

    struct FIndexBufferBinding
    {
        IBuffer* pBuffer = 0;
        EFormat Format = EFormat::R32_UINT;
        UINT32 dwOffset = 0;

        BOOL operator==(const FIndexBufferBinding& crBinding) const
        {
            return  pBuffer == crBinding.pBuffer &&
                    dwOffset == crBinding.dwOffset &&
                    Format == crBinding.Format;
        }

        BOOL operator!=(const FIndexBufferBinding& crBinding) const
        {
            return !((*this) == crBinding);
        }

        BOOL IsValid() const { return pBuffer != nullptr; }
    };


    struct FDrawArguments
    {
        UINT32 dwIndexOrVertexCount = 0;    // 当使用 Draw() 时为 VertexCount, 使用 DrawIndexed() 时为 IndexCount.
        UINT32 dwInstanceCount = 1;
        UINT32 dwStartIndexLocation = 0;
        UINT32 dwStartVertexLocation = 0;
        UINT32 dwStartInstanceLocation = 0;
    };

    using FPipelineStateBindingSetArray = TStackArray<IBindingSet*, gdwMaxBindingLayouts>;

    struct FGraphicsState
    {
        IGraphicsPipeline* pPipeline = nullptr;
        FPipelineStateBindingSetArray pBindingSets; // 需要与 BindingLaouts 数组的顺序相对应.
        FColor BlendConstantColor;
        
        IFrameBuffer* pFramebuffer = nullptr;
        FViewportState ViewportState;
        
        FVertexBufferBindingArray VertexBufferBindings;
        FIndexBufferBinding IndexBufferBinding;

        UINT8 btDynamicStencilRefValue = 0;
    };

    struct FComputeState
    {
        IComputePipeline* pPipeline = nullptr;
        FPipelineStateBindingSetArray pBindingSets;
    };

}






































#endif