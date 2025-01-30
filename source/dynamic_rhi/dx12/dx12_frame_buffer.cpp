#include "dx12_frame_buffer.h"
#include "dx12_resource.h"
#include <memory>
#include <sstream>
#include "../../core/tools/check_cast.h"

namespace fantasy 
{
    DX12FrameBuffer::DX12FrameBuffer(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps, const FrameBufferDesc& desc) :
        _context(context), 
        _descriptor_heaps(descriptor_heaps), 
        _desc(desc), 
        _info(desc)
    {
    }

    DX12FrameBuffer::~DX12FrameBuffer() noexcept
    {
        for (uint32_t rtv_index : _rtv_indices)
        {
            _descriptor_heaps->render_target_heap.release_descriptor(rtv_index);
        }

        if (_dsv_index != INVALID_SIZE_32)
        {
            _descriptor_heaps->depth_stencil_heap.release_descriptor(_dsv_index);
        }
    }

    bool DX12FrameBuffer::initialize()
    {
        if (_desc.color_attachments.size() != 0)
        {
            TextureDesc rtv_desc = _desc.color_attachments[0].texture->get_desc();
            _width = rtv_desc.width;
            _height = rtv_desc.height;
        }
        else if (_desc.depth_stencil_attachment.texture != nullptr)
        {
            TextureDesc dsv_desc = _desc.depth_stencil_attachment.texture->get_desc();
            _width = dsv_desc.width;
            _height = dsv_desc.height;
        }
 
        for (uint32_t ix = 0; ix < _desc.color_attachments.size(); ++ix)
        {
            auto& attachment = _desc.color_attachments[ix];
            
            const auto& texture = attachment.texture;
            ReturnIfFalse(texture != nullptr);

            TextureDesc texture_desc = texture->get_desc();
            ReturnIfFalse(texture_desc.width == _width && texture_desc.height == _height);

            uint32_t rtv_index = _descriptor_heaps->render_target_heap.allocate_descriptor();
            D3D12_CPU_DESCRIPTOR_HANDLE view = _descriptor_heaps->render_target_heap.get_cpu_handle(rtv_index);

            std::shared_ptr<DX12Texture> dx12_texture = check_cast<DX12Texture>(texture);
            dx12_texture->CreateRTV(view.ptr, attachment.format, attachment.subresource);

            _rtv_indices.push_back(rtv_index);
            _textures.emplace_back(texture);
        }

        if (_desc.depth_stencil_attachment.texture != nullptr)
        {
            const auto& texture = _desc.depth_stencil_attachment.texture;
			ReturnIfFalse(texture != nullptr);

            TextureDesc texture_desc = texture->get_desc();
            if (texture_desc.width != _width || texture_desc.height != _height)
            {
                std::stringstream ss;
                ss  << "Depth buffer has different size texture with frame buffer."
                    << "Depth buffer width is " << texture_desc.width << "."
                    << "              height is " << texture_desc.height << "."
                    << "Frame buffer width is " << _width << "."
                    << "             height is " << _height << ".";
                LOG_ERROR(ss.str());
                return false;
            }

            uint32_t dsv_index = _descriptor_heaps->depth_stencil_heap.allocate_descriptor();
            D3D12_CPU_DESCRIPTOR_HANDLE view = _descriptor_heaps->depth_stencil_heap.get_cpu_handle(dsv_index);

            std::shared_ptr<DX12Texture> dx12_texture = check_cast<DX12Texture>(texture);
            dx12_texture->CreateDSV(view.ptr, _desc.depth_stencil_attachment.subresource, _desc.depth_stencil_attachment.is_read_only);

            _dsv_index = dsv_index;
            _textures.emplace_back(texture);
        }
        return true;
    }

}
