/**
 * *****************************************************************************
 * @file        DX12FrameBuffer.h
 * @brief       
 * @author      CellarCordial (591885295@qq.com)
 * @date        2024-06-02
 * @copyright Copyright (c) 2024
 * *****************************************************************************
 */

 #ifndef RHI_DX12_FRAME_BUFFER_H
 #define RHI_DX12_FRAME_BUFFER_H


#include "../../include/FrameBuffer.h"
#include "../../../Core/include/ComRoot.h"
#include "DX12Descriptor.h"
#include "DX12Forward.h"
#include <vector>

namespace FTS 
{

    class FDX12FrameBuffer :
        public TComObjectRoot<FComMultiThreadModel>,
        public IFrameBuffer
    {
    public:

        BEGIN_INTERFACE_MAP(FDX12FrameBuffer)
            INTERFACE_ENTRY(IID_IFrameBuffer, IFrameBuffer)
        END_INTERFACE_MAP

        FDX12FrameBuffer(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps, const FFrameBufferDesc& crDesc);
        ~FDX12FrameBuffer() noexcept;

        BOOL Initialize();


        // IFrameBuffer
        FFrameBufferDesc GetDesc() const override { return m_Desc; }
        FFrameBufferInfo GetInfo() const override { return m_Info; }

    public:
        std::vector<UINT32> m_dwRTVIndices;
        UINT32 m_dwDSVIndex = gdwInvalidViewIndex;
        std::vector<TComPtr<ITexture>> m_pRefTextures;

    private:
        const FDX12Context* m_cpContext;
        FDX12DescriptorHeaps* m_pDescriptorHeaps = nullptr;

        FFrameBufferDesc m_Desc;
        FFrameBufferInfo m_Info;

        UINT32 m_dwWidth = 0;
        UINT32 m_dwHeight = 0;
    };


}



















 #endif