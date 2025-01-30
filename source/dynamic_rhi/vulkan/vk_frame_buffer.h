#ifndef DYNAMIC_RHI_VULKAN_FRAME_BUFFER_H
#define DYNAMIC_RHI_VULKAN_FRAME_BUFFER_H

#include "../frame_buffer.h"
#include "vk_forward.h"

namespace fantasy 
{
    class VKFrameBuffer : public FrameBufferInterface
    {
    public:
        explicit VKFrameBuffer(const VKContext* context)
            : _context(context)
        { }

        ~VKFrameBuffer() override;
        const FrameBufferDesc& get_desc() const override { return desc; }
        const FrameBufferInfo& get_info() const override;

    public:
        FrameBufferDesc desc;
        FrameBufferInfo frame_buffer_info;
        
        vk::RenderPass render_pass = vk::RenderPass();
        vk::Framebuffer frame_buffer = vk::Framebuffer();

        std::vector<ResourceInterface> resources;

        bool managed = true;

    private:
        const VKContext* _context;
    };

}

#endif