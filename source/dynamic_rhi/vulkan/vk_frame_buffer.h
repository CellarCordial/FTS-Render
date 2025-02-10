#ifndef DYNAMIC_RHI_VULKAN_FRAME_BUFFER_H
#define DYNAMIC_RHI_VULKAN_FRAME_BUFFER_H

#include "../frame_buffer.h"
#include "vk_forward.h"
#include <memory>

namespace fantasy 
{
    class VKFrameBuffer : public FrameBufferInterface
    {
    public:
        explicit VKFrameBuffer(const VKContext* context, const FrameBufferDesc& desc);
        ~VKFrameBuffer() override;

        bool initialize();

        const FrameBufferDesc& get_desc() const override;
        const FrameBufferInfo& get_info() const override;

    public:
        FrameBufferDesc desc;
        FrameBufferInfo info;
        
        vk::RenderPass vk_render_pass;
        vk::Framebuffer vk_frame_buffer;

        std::vector<std::shared_ptr<ResourceInterface>> ref_resources;

        bool managed = true;

    private:
        const VKContext* _context;
    };

}

#endif