#include "vk_resource.h"
#include "vk_allocator.h"
#include "vk_convert.h"

namespace fantasy 
{
    VKTexture::VKTexture(const VKContext* context, const VKMemoryAllocator* allocator, const TextureDesc& desc_) :
        _context(context), 
        _allocator(allocator),
        desc(desc_)
    { 
    }

    bool VKTexture::initialize()
    {
        image_info = vk::ImageCreateInfo();
        image_info.imageType = convert_texture_dimension(desc.dimension);
        image_info.extent = vk::Extent3D(desc.width, desc.height, desc.depth);
        image_info.mipLevels = desc.mip_levels;
        image_info.arrayLayers = desc.array_size;
        image_info.format = vk::Format(convert_format(desc.format));
        image_info.initialLayout = vk::ImageLayout::eUndefined;
        image_info.usage = get_image_usage_flag(desc);
        image_info.sharingMode = vk::SharingMode::eExclusive;
        image_info.samples = get_sample_count_flag(desc.sample_count);
        image_info.flags = get_image_create_flag(desc);
        
#if _WIN32
        const auto handle_type = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32;
#else
        const auto handle_type = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd;
#endif

        ReturnIfFalse(_context->device.createImage(&image_info, _context->allocation_callbacks, &image) == vk::Result::eSuccess);

        _context->name_object(
            image, 
            vk::ObjectType::eImage, 
            vk::DebugReportObjectTypeEXT::eImage, 
            desc.name.c_str()
        );

        if (!desc.is_virtual)
        {
            ReturnIfFalse(_allocator->allocate_texture_memory(this));
            _context->name_object(_vk_device_memory, vk::ObjectType::eDeviceMemory, vk::DebugReportObjectTypeEXT::eDeviceMemory, desc.name.c_str());
        }

        return true;
    }


    VKTexture::~VKTexture()
    {
        // 清除所有子资源的 view.
        for (auto& iter : subresource_views)
        {
            auto& view = iter.second.view;
            _context->device.destroyImageView(view, _context->allocation_callbacks);
            view = vk::ImageView();
        }
        subresource_views.clear();

        if (image)
        {
            _context->device.destroyImage(image, _context->allocation_callbacks);
            image = vk::Image();
        }
        if (_vk_device_memory)
        {
            _allocator->free_texture_memory(this);
            _vk_device_memory = vk::DeviceMemory();
        }
    }

    uint32_t VKTexture::get_view_index(ViewType view_type, TextureSubresourceSet subresource, bool is_read_only_dsv)
    {
        
    }

}