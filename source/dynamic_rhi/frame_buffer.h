#ifndef RHI_FRAME_BUFFER_H
#define RHI_FRAME_BUFFER_H

#include "../core/tools/stack_array.h"
#include "resource.h"
#include "forward.h"
#include "format.h"
#include <memory>

namespace fantasy
{
    class CommandListInterface;

    struct FrameBufferAttachment
    {
        std::shared_ptr<TextureInterface> texture;
        TextureSubresourceSet subresource;

        bool is_valid() const { return texture != nullptr; }

        static FrameBufferAttachment create_attachment(
            const std::shared_ptr<TextureInterface>& texture,
            TextureSubresourceSet subresource = entire_subresource_set
        )
        {
            FrameBufferAttachment ret;
            ret.texture = texture;
            ret.subresource = subresource;
            return ret;
        }
    };

    using FrameBufferAttachmentArray = StackArray<FrameBufferAttachment, MAX_RENDER_TARGETS>;

    struct FrameBufferDesc
    {
        FrameBufferAttachmentArray color_attachments;
        FrameBufferAttachment depth_stencil_attachment;
    };

    using RenderTargetFormatArray = StackArray<Format, MAX_RENDER_TARGETS>;

    struct FrameBufferInfo
    {
        RenderTargetFormatArray rtv_formats;
        Format depth_format = Format::UNKNOWN;

        uint32_t width = 0;
        uint32_t height = 0;
        
        FrameBufferInfo() = default;
        FrameBufferInfo(const FrameBufferDesc& desc)
        {
            for (const auto& attachment : desc.color_attachments)
            {
                rtv_formats.push_back(attachment.texture->get_desc().format);
            }
            
            if (desc.depth_stencil_attachment.is_valid())
            {
                const TextureDesc& texture_desc = desc.depth_stencil_attachment.texture->get_desc();
                uint32_t base_mip_level = desc.depth_stencil_attachment.subresource.base_mip_level;

                depth_format = texture_desc.format;
                width = std::max(texture_desc.width >> base_mip_level, 1u);
                height = std::max(texture_desc.height >> base_mip_level, 1u);
            }
            else if (!desc.color_attachments.empty() && desc.color_attachments[0].is_valid())
            {   
                const TextureDesc& texture_desc = desc.color_attachments[0].texture->get_desc();
                uint32_t base_mip_level = desc.color_attachments[0].subresource.base_mip_level;

                width = std::max(texture_desc.width >> base_mip_level, 1u);
                height = std::max(texture_desc.height >> base_mip_level, 1u);
            }
            else 
            {
                assert(!"Frame buffer initialized.");
            }
        }

        Viewport get_viewport(float min_z, float max_z)
        {
            return Viewport { 0.f, static_cast<float>(width), 0.f, static_cast<float>(height), min_z, max_z };
        }
    };


    struct FrameBufferInterface : public ResourceInterface
    {
        virtual const FrameBufferDesc& get_desc() const = 0;
        virtual const FrameBufferInfo& get_info() const = 0;

		virtual ~FrameBufferInterface() = default;
    };

}












#endif