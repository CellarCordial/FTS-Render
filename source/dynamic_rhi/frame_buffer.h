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
        Format format = Format::UNKNOWN;
        bool is_read_only = false;

        bool is_valid() const { return texture != nullptr; }

        static FrameBufferAttachment create_attachment(const std::shared_ptr<TextureInterface>& texture)
        {
            FrameBufferAttachment ret;
            ret.texture = texture;
            ret.format = texture->get_desc().format;
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

        uint32_t sample_count = 1;
        uint32_t sample_quality = 0;

        uint32_t width = 0;
        uint32_t height = 0;

        
        FrameBufferInfo(const FrameBufferDesc& desc)
        {
            for (const auto& attachment : desc.color_attachments)
            {
                rtv_formats.push_back(attachment.format == Format::UNKNOWN && attachment.texture ? attachment.texture->get_desc().format : attachment.format);
            }
            
            if (desc.depth_stencil_attachment.is_valid())
            {
                TextureDesc TextureDesc = desc.depth_stencil_attachment.texture->get_desc();
                depth_format = desc.depth_stencil_attachment.format == Format::UNKNOWN ? TextureDesc.format : desc.depth_stencil_attachment.format;
                
                sample_count = TextureDesc.sample_count;
                sample_quality = TextureDesc.sample_quality;
                width = std::max(TextureDesc.width >> desc.depth_stencil_attachment.subresource.base_mip_level, 1u);
                height = std::max(TextureDesc.height >> desc.depth_stencil_attachment.subresource.base_mip_level, 1u);
            }
            else if (!desc.color_attachments.empty() && desc.color_attachments[0].is_valid())
            {   
                TextureDesc TextureDesc = desc.color_attachments[0].texture->get_desc();
                sample_count = TextureDesc.sample_count;
                sample_quality = TextureDesc.sample_quality;
                width = std::max(TextureDesc.width >> desc.color_attachments[0].subresource.base_mip_level, 1u);
                height = std::max(TextureDesc.height >> desc.color_attachments[0].subresource.base_mip_level, 1u);
            }
            else 
            {
                assert(!"Create FrameBuffer without any attachments.");
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