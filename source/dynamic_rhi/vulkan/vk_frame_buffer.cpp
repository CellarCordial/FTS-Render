#include "vk_frame_buffer.h"
#include "vk_resource.h"
#include "../../core/tools/check_cast.h"

namespace fantasy 
{
    VKFrameBuffer::VKFrameBuffer(const VKContext* context, const FrameBufferDesc& desc_) : 
        _context(context), desc(desc_), info(desc_)
    {
    }

    VKFrameBuffer::~VKFrameBuffer()
    {
        if (vk_frame_buffer && managed)
        {
            _context->device.destroyFramebuffer(vk_frame_buffer);
        }

        if (vk_render_pass && managed)
        {
            _context->device.destroyRenderPass(vk_render_pass);
        }
    }

    bool VKFrameBuffer::initialize()
    {
        // Render Target + Depth Stencil.
        StackArray<vk::AttachmentDescription2, MAX_RENDER_TARGETS + 1> vk_attachment_descriptions(desc.color_attachments.size());
        StackArray<vk::AttachmentReference2, MAX_RENDER_TARGETS + 1> vk_attachment_references(desc.color_attachments.size());
        vk::AttachmentReference2 depth_attachment_ref{};

        StackArray<vk::ImageView, MAX_RENDER_TARGETS + 1> vk_attachment_views;
        vk_attachment_views.resize(desc.color_attachments.size());

        uint32_t array_slice_count = 0;

        for(uint32_t ix = 0; ix < desc.color_attachments.size(); ix++)
        {
            const auto& attachment = desc.color_attachments[ix];
            auto texture = check_cast<VKTexture>(attachment.texture);

            vk_attachment_descriptions[ix].flags = vk::AttachmentDescriptionFlags();
            vk_attachment_descriptions[ix].format = texture->vk_image_info.format;
            vk_attachment_descriptions[ix].samples = texture->vk_image_info.samples;
            vk_attachment_descriptions[ix].loadOp = vk::AttachmentLoadOp::eLoad;
            vk_attachment_descriptions[ix].storeOp = vk::AttachmentStoreOp::eStore;
            vk_attachment_descriptions[ix].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            vk_attachment_descriptions[ix].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            vk_attachment_descriptions[ix].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
            vk_attachment_descriptions[ix].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

            vk_attachment_references[ix].attachment = ix;
            vk_attachment_references[ix].layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk_attachment_views[ix] = texture->get_view(ResourceViewType::Texture_RTV, attachment.subresource);

            ref_resources.push_back(attachment.texture);

            if (array_slice_count)
            {
                ReturnIfFalse(array_slice_count == attachment.subresource.array_slice_count);
            }
            else
            {
                array_slice_count = attachment.subresource.array_slice_count;
            }
        }

        if (desc.depth_stencil_attachment.is_valid())
        {
            const auto& attachment = desc.depth_stencil_attachment;

            auto texture = check_cast<VKTexture>(attachment.texture);

            vk::AttachmentDescription2 vk_attachment_description{};
            vk_attachment_description.flags = vk::AttachmentDescriptionFlags();
            vk_attachment_description.format = texture->vk_image_info.format;
            vk_attachment_description.samples = texture->vk_image_info.samples;
            vk_attachment_description.loadOp = vk::AttachmentLoadOp::eLoad;
            vk_attachment_description.storeOp = vk::AttachmentStoreOp::eStore;
            vk_attachment_description.stencilLoadOp = vk::AttachmentLoadOp::eLoad;
            vk_attachment_description.stencilStoreOp = vk::AttachmentStoreOp::eStore;
            vk_attachment_description.initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            vk_attachment_description.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            vk_attachment_descriptions.push_back(vk_attachment_description);

            depth_attachment_ref.attachment = static_cast<uint32_t>(vk_attachment_descriptions.size()) - 1;
            depth_attachment_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            vk_attachment_views.push_back(texture->get_view(ResourceViewType::Texture_DSV, attachment.subresource));

            ref_resources.push_back(attachment.texture);

            
            if (array_slice_count)
            {
                ReturnIfFalse(array_slice_count == attachment.subresource.array_slice_count);
            }
            else
            {
                array_slice_count = attachment.subresource.array_slice_count;
            }
        }

        vk::SubpassDescription2 vk_subpass_description{};
        vk_subpass_description.flags = vk::SubpassDescriptionFlags();
        vk_subpass_description.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        vk_subpass_description.inputAttachmentCount = 0;
        vk_subpass_description.pInputAttachments = nullptr;
        vk_subpass_description.colorAttachmentCount = uint32_t(desc.color_attachments.size());
        vk_subpass_description.pColorAttachments = vk_attachment_references.data();
        vk_subpass_description.pResolveAttachments = nullptr;
        vk_subpass_description.pDepthStencilAttachment = desc.depth_stencil_attachment.is_valid() ? &depth_attachment_ref : nullptr;
        vk_subpass_description.preserveAttachmentCount = 0;
        vk_subpass_description.pPreserveAttachments = nullptr;

        vk::RenderPassCreateInfo2 vk_render_pass_info{};
        vk_render_pass_info.pNext = nullptr;
        vk_render_pass_info.flags = vk::RenderPassCreateFlags();
        vk_render_pass_info.attachmentCount = static_cast<uint32_t>(vk_attachment_descriptions.size());
        vk_render_pass_info.pAttachments = vk_attachment_descriptions.data();
        vk_render_pass_info.subpassCount = 1;
        vk_render_pass_info.pSubpasses = &vk_subpass_description;
        vk_render_pass_info.dependencyCount = 0;
        vk_render_pass_info.pDependencies = nullptr;

        ReturnIfFalse(vk::Result::eSuccess == _context->device.createRenderPass2(
            &vk_render_pass_info, 
            _context->allocation_callbacks, 
            &vk_render_pass
        ));
        
        vk::FramebufferCreateInfo vk_frame_buffer_info{};
        vk_frame_buffer_info.pNext = nullptr;
        vk_frame_buffer_info.flags = vk::FramebufferCreateFlags();
        vk_frame_buffer_info.renderPass = vk_render_pass;
        vk_frame_buffer_info.attachmentCount = static_cast<uint32_t>(vk_attachment_views.size());
        vk_frame_buffer_info.pAttachments = vk_attachment_views.data();
        vk_frame_buffer_info.width = vk_frame_buffer_info.width;
        vk_frame_buffer_info.height = vk_frame_buffer_info.height;
        vk_frame_buffer_info.layers = array_slice_count;

        return vk::Result::eSuccess == _context->device.createFramebuffer(
            &vk_frame_buffer_info, 
            _context->allocation_callbacks,
            &vk_frame_buffer
        );
    }

    const FrameBufferDesc& VKFrameBuffer::get_desc() const
    { 
        return desc; 
    }

    const FrameBufferInfo& VKFrameBuffer::get_info() const
    { 
        return info; 
    }

}