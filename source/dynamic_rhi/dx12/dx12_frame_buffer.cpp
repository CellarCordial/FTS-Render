#include "dx12_frame_buffer.h"
#include "dx12_resource.h"
#include <memory>
#include "../../core/tools/check_cast.h"

namespace fantasy 
{
    DX12FrameBuffer::DX12FrameBuffer(const DX12Context* context, DX12DescriptorManager* descriptor_heaps, const FrameBufferDesc& desc_) :
        _context(context), 
        _descriptor_manager(descriptor_heaps), 
        desc(desc_), 
        info(desc_)
    {
    }

    bool DX12FrameBuffer::initialize()
    {
        for (const auto& attachment : desc.color_attachments)
        {
            auto texture = check_cast<DX12Texture>(attachment.texture);

            uint32_t rtv_index = texture->get_view_index(ResourceViewType::Texture_RTV, attachment.subresource);

            rtv_indices.push_back(rtv_index);
            ref_textures.emplace_back(texture);
        }

        if (desc.depth_stencil_attachment.is_valid())
        {
            std::shared_ptr<DX12Texture> texture = check_cast<DX12Texture>(desc.depth_stencil_attachment.texture);

            dsv_index = texture->get_view_index(
                ResourceViewType::Texture_DSV, 
                desc.depth_stencil_attachment.subresource
            );

            ref_textures.emplace_back(texture);
        }
        return true;
    }

}
