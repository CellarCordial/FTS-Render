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


#include "../frame_buffer.h"
#include "dx12_descriptor.h"
#include <memory>
#include <vector>

namespace fantasy 
{

    class DX12FrameBuffer : public FrameBufferInterface
    {
    public:
        DX12FrameBuffer(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps, const FrameBufferDesc& desc);
        ~DX12FrameBuffer() noexcept;

        bool initialize();

        // FrameBufferInterface
        const FrameBufferDesc& get_desc() const override { return _desc; }
        const FrameBufferInfo& get_info() const override { return _info; }

    public:
        std::vector<uint32_t> _rtv_indices;
        uint32_t _dsv_index = INVALID_SIZE_32;
        std::vector<std::shared_ptr<TextureInterface>> _textures;

    private:
        const DX12Context* _context;
        DX12DescriptorHeaps* _descriptor_heaps = nullptr;

        FrameBufferDesc _desc;
        FrameBufferInfo _info;

        uint32_t _width = 0;
        uint32_t _height = 0;
    };


}



















 #endif