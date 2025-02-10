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
        DX12FrameBuffer(const DX12Context* context, DX12DescriptorManager* descriptor_heaps, const FrameBufferDesc& desc);
        ~DX12FrameBuffer() noexcept;

        bool initialize();

        const FrameBufferDesc& get_desc() const override { return desc; }
        const FrameBufferInfo& get_info() const override { return info; }

    public:
        FrameBufferDesc desc;
        FrameBufferInfo info;

        std::vector<uint32_t> rtv_indices;
        uint32_t dsv_index = INVALID_SIZE_32;
        std::vector<std::shared_ptr<TextureInterface>> ref_textures;

    private:
        const DX12Context* _context;
        DX12DescriptorManager* _descriptor_manager;
    };


}



















 #endif