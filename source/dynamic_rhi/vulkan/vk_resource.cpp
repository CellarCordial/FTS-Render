#include "vk_resource.h"
#include "vk_memory.h"
#include "vk_convert.h"
#include "vk_forward.h"
#include "../../core/tools/check_cast.h"
#include <cstdint>
#include <memory>
#include <utility>

namespace fantasy 
{
    vk::ImageAspectFlags get_texture_subresource_aspect_flags(ResourceViewType view_type, const FormatInfo& format_info)
    {
        vk::ImageAspectFlags ret = vk::ImageAspectFlags();
        if (view_type == ResourceViewType::Texture_RTV) 
        {
            ret = vk::ImageAspectFlagBits::eColor;
        }
        else if (view_type == ResourceViewType::Texture_RTV)
        {
            // 通过 format 来判断具体是 depth only 还是 stencil only.
            if (format_info.has_depth) ret |= vk::ImageAspectFlagBits::eDepth;
            if(format_info.has_stencil) ret |= vk::ImageAspectFlagBits::eStencil;
        }
        return ret;
    }


    VKTexture::VKTexture(const VKContext* context, const VKMemoryAllocator* allocator, const TextureDesc& desc_) :
        _context(context), 
        _allocator(allocator),
        desc(desc_),
        vk_image_info(convert_image_info(desc))
    { 
    }

    bool VKTexture::initialize()
    {
        ReturnIfFalse(_context->device.createImage(&vk_image_info, _context->allocation_callbacks, &vk_image) == vk::Result::eSuccess);

        _context->name_object(vk_image, vk::ObjectType::eImage, desc.name.c_str());

        if (!desc.is_virtual)
        {
            ReturnIfFalse(_allocator->allocate_texture_memory(this));
            _context->name_object(vk_device_memory, vk::ObjectType::eDeviceMemory, desc.name.c_str());
        }

        return true;
    }

    VKTexture::~VKTexture()
    {
        for (auto& iter : view_cache) _context->device.destroyImageView(iter.second.vk_view, _context->allocation_callbacks);

        if (vk_image) _context->device.destroyImage(vk_image, _context->allocation_callbacks);
        if (vk_device_memory) _allocator->free_texture_memory(this);
    }

    const TextureDesc& VKTexture::get_desc() const
    {
        return desc; 
    }

    bool VKTexture::bind_memory(std::shared_ptr<HeapInterface> heap_, uint64_t offset)
    {
        if (heap != nullptr || !desc.is_virtual) return false;

        heap = heap_;
        _context->device.bindImageMemory(vk_image, vk_device_memory, offset);

        return true;
    }

    MemoryRequirements VKTexture::get_memory_requirements()
    {
        vk::MemoryRequirements vk_memory_requirement;
        _context->device.getImageMemoryRequirements(vk_image, &vk_memory_requirement);

        MemoryRequirements ret;
        ret.alignment = vk_memory_requirement.alignment;
        ret.size = vk_memory_requirement.size;
        return ret;
    }
    
    void* VKTexture::get_native_object() 
    {
        return vk_image; 
    }

    vk::ImageView VKTexture::get_view(ResourceViewType view_type, const TextureSubresourceSet& subresource)
    {
        std::lock_guard lock(_mutex);

        auto cache_key = std::make_pair(subresource, view_type);
        
        auto iter = view_cache.find(cache_key);
        if (iter != view_cache.end()) return iter->second.vk_view;


        FormatInfo format_info = get_format_info(desc.format);

        vk::ImageAspectFlags vk_aspect_flags = vk::ImageAspectFlags();
        if (view_type == ResourceViewType::Texture_RTV) 
        {
            vk_aspect_flags = vk::ImageAspectFlagBits::eColor;
        }
        else if (view_type == ResourceViewType::Texture_RTV)
        {
            if (format_info.has_depth) vk_aspect_flags |= vk::ImageAspectFlagBits::eDepth;
            if(format_info.has_stencil) vk_aspect_flags |= vk::ImageAspectFlagBits::eStencil;
        }

        auto iter_pair = view_cache.emplace(cache_key, VKTextureSubresourceView(this, subresource));
        auto& view = std::get<0>(iter_pair)->second;

        view.vk_subresource_range.aspectMask = vk_aspect_flags;
        view.vk_subresource_range.baseMipLevel = subresource.base_mip_level;
        view.vk_subresource_range.levelCount = subresource.mip_level_count;
        view.vk_subresource_range.baseArrayLayer = subresource.base_array_slice;
        view.vk_subresource_range.layerCount = subresource.array_slice_count;

        vk::ImageViewCreateInfo view_info{};
        view_info.image = vk_image;
        view_info.format = vk_image_info.format;
        view_info.subresourceRange = view.vk_subresource_range;
        view_info.viewType = convert_texture_dimension_to_image_view_type(desc.dimension);

        if (!format_info.has_depth && format_info.has_stencil) // stencil only
        {
            // 在 DX 中，深度/模板组合纹理 (如 D24_S8) 的模板值通常位于 G 分量 (通过 SV_StencilRef 或 .g 访问), 而深度值在 R 分量.
            // 在 Vulkan 中, 模板值默认可能位于 R 分量 (取决于格式和视图配置), 需通过 ComponentSwizzle 手动映射到 G 分量以保持兼容性.
            view_info.components.setG(vk::ComponentSwizzle::eR);
        }

        if (_context->device.createImageView(&view_info, _context->allocation_callbacks, &view.vk_view) != vk::Result::eSuccess)
        {
            LOG_ERROR("Create image view failed.");
            return VK_NULL_HANDLE;
        }

        _context->name_object(VkImageView(view.vk_view), vk::ObjectType::eImageView, std::string("ImageView for  " + desc.name).c_str());

        return view.vk_view;
    }


    VKBuffer::VKBuffer(const VKContext* context, const VKMemoryAllocator* allocator, const BufferDesc& desc_) :
        _context(context), _allocator(allocator), desc(desc_)
    {
    }

    VKBuffer::~VKBuffer()
    {
        for (auto&& iter : view_cache) _context->device.destroyBufferView(iter.second, _context->allocation_callbacks);

        if (mapped_volatile_memory) _context->device.unmapMemory(vk_device_memory);
        
        if (vk_buffer) _context->device.destroyBuffer(vk_buffer, _context->allocation_callbacks);
        if (vk_device_memory) _allocator->free_buffer_memory(this);
    }

    bool VKBuffer::initialize()
    {
        if (desc.is_volatile_constant_buffer)
        {
            uint64_t alignment = _context->vk_physical_device_properties.limits.minUniformBufferOffsetAlignment;

            uint64_t atom_size = _context->vk_physical_device_properties.limits.nonCoherentAtomSize;
            alignment = std::max(alignment, atom_size);

            ReturnIfFalse(is_power_of_2(alignment));
            
            desc.byte_size = align(desc.byte_size, alignment);
            desc.byte_size *= volatile_constant_buffer_max_version;

            version_tracking.resize(volatile_constant_buffer_max_version);
            std::fill(version_tracking.begin(), version_tracking.end(), 0);

            desc.cpu_access = CpuAccessMode::Write;
        }
        else if (desc.byte_size < 65536)
        {
            // vulkan 允许使用 vkCmdUpdateBuffer 进行内联更新, 但数据大小必须是 4 的倍数，因此将大小向上对齐.
            desc.byte_size = align(desc.byte_size, 4ull);
        }

        vk_buffer_info = convert_buffer_info(desc);

        ReturnIfFalse(_context->device.createBuffer(&vk_buffer_info, _context->allocation_callbacks, &vk_buffer) == vk::Result::eSuccess);
        _context->name_object(vk_buffer, vk::ObjectType::eBuffer, desc.name.c_str());

        if (!desc.is_virtual)
        {
            ReturnIfFalse(_allocator->allocate_buffer_memory(this));
            _context->name_object(vk_device_memory, vk::ObjectType::eDeviceMemory, desc.name.c_str());

            if (desc.is_volatile_constant_buffer)
            {
                mapped_volatile_memory = _context->device.mapMemory(vk_device_memory, 0, desc.byte_size);
                ReturnIfFalse(mapped_volatile_memory != nullptr);
            }
        }
        return true;
    }

    const BufferDesc& VKBuffer::get_desc() const
    { 
        return desc; 
    }
    
    void* VKBuffer::get_native_object() 
    { 
        return vk_buffer; 
    }

    MemoryRequirements VKBuffer::get_memory_requirements()
    {
        vk::MemoryRequirements vk_memory_requirements;
        _context->device.getBufferMemoryRequirements(vk_buffer, &vk_memory_requirements);

        return MemoryRequirements{
            .alignment = vk_memory_requirements.alignment,
            .size = vk_memory_requirements.size
        };
    }

    bool VKBuffer::bind_memory(std::shared_ptr<HeapInterface> heap_, uint64_t offset)
    {
        if (heap != nullptr || !desc.is_virtual) return false;

        heap = heap_;

        _context->device.bindBufferMemory(vk_buffer, vk_device_memory, offset);

        return true;
    }

    void* VKBuffer::map(CpuAccessMode cpu_access)
    {
        if (cpu_access == CpuAccessMode::None)
        {
            LOG_ERROR("Buffer map has invalid cpu access mode.");
            return nullptr;
        }

        void* data = nullptr;
        if (_context->device.mapMemory(vk_device_memory, 0, desc.byte_size, vk::MemoryMapFlags(), &data) != vk::Result::eSuccess)
        {
            LOG_ERROR("Buffer map failed.");
            return nullptr;
        }

        return data;
    }

    void VKBuffer::unmap()
    {
        _context->device.unmapMemory(vk_device_memory);
    }

    vk::BufferView VKBuffer::get_typed_buffer_view(const BufferRange& range, ResourceViewType type)
    {
        vk::Format vk_format = convert_format(desc.format);

        size_t view_info_hash = 0;
        hash_combine(view_info_hash, range.byte_offset);
        hash_combine(view_info_hash, range.byte_size);
        hash_combine(view_info_hash, static_cast<uint64_t>(vk_format));

        auto iter = view_cache.find(view_info_hash);
        auto& buffer_view_ref = (iter == view_cache.end()) ? view_cache[view_info_hash] : iter->second;
        if (iter == view_cache.end())
        {

            vk::BufferViewCreateInfo buffer_view_info{};
            buffer_view_info.buffer = vk_buffer;
            buffer_view_info.offset = range.byte_offset;
            buffer_view_info.range = range.byte_size;
            buffer_view_info.format = vk_format;

            if (
                vk::Result::eSuccess != _context->device.createBufferView(
                    &buffer_view_info, 
                    _context->allocation_callbacks, 
                    &buffer_view_ref
                )
            )
            {
                return VK_NULL_HANDLE;
            }
        }
        return buffer_view_ref;
    }



    VKStagingTexture::VKStagingTexture(const VKContext* context, const TextureDesc& desc_, CpuAccessMode access_mode_) : 
        _context(context), desc(desc_), access_mode(access_mode_)
    {
    }

    bool VKStagingTexture::initialize(const VKMemoryAllocator* allocator)
    {
        ReturnIfFalse(access_mode != CpuAccessMode::None);

        slice_regions.clear();
        const FormatInfo& format_info = get_format_info(desc.format);

        uint32_t offset = 0;
        for(uint32_t mip = 0; mip < desc.mip_levels; mip++)
        {
            uint32_t width = std::max((desc.width >> mip), 1u);
            uint32_t height = std::max((desc.height >> mip), 1u);

            auto slice_size =  format_info.size * width * height;

            uint32_t depth = std::max(desc.depth >> mip, 1u);
            uint32_t slice_count = desc.array_size * depth;

            for (uint32_t slice = 0; slice < slice_count; slice++)
            {
                slice_regions.push_back({ offset, slice_size });
                
                static constexpr uint32_t buffer_aligment_bytes = 4;
                offset = align(offset + slice_size, buffer_aligment_bytes) * buffer_aligment_bytes;
            }
        }

        BufferDesc buffer_desc;
        buffer_desc.name = desc.name;
        buffer_desc.byte_size = slice_regions.back().offset + slice_regions.back().size;
        buffer_desc.cpu_access = access_mode;
        buffer_desc.allow_shader_resource = false;
        
        VKBuffer* buffer = new VKBuffer(_context, allocator, buffer_desc);
        if (!buffer->initialize())
        {
            delete buffer;
            return false;
        }
        _buffer = std::shared_ptr<BufferInterface>(buffer);

        return true;
    }

    const TextureDesc& VKStagingTexture::get_desc() const 
    { 
        return desc; 
    }

    void* VKStagingTexture::get_native_object() 
    { 
        return _buffer->get_native_object(); 
    }

    void* VKStagingTexture::map(const TextureSlice& texture_slice, CpuAccessMode cpu_access_mode, uint64_t* row_pitch)
    {
        if (
            row_pitch == nullptr ||
            texture_slice.x != 0 || 
            texture_slice.y != 0 || 
            cpu_access_mode == CpuAccessMode::None
        )
        {
            LOG_ERROR("Invalid staging texture slice.");
            return nullptr;
        } 


        VKSliceRegion region = get_slice_region(texture_slice.mip_level, texture_slice.array_slice, texture_slice.z);

        // vulkan 规范要求纹理区域的偏移量是 4 字节对齐的.
        if ((region.offset & 0x3) != 0 && region.size == 0)
        {
            LOG_ERROR("Invalid staging texture map.");
            return nullptr;
        }

        *row_pitch = texture_slice.width * get_format_info(desc.format).size;

        void* data = nullptr;
        if (
            vk::Result::eSuccess == _context->device.mapMemory(
                check_cast<VKBuffer*>(_buffer.get())->vk_device_memory, 
                region.offset, 
                region.size, 
                vk::MemoryMapFlags(), 
                &data
            )
        )
        {
            LOG_ERROR("Staging texture map failed.");
            return nullptr;
        }

        return data;
    }

    void VKStagingTexture::unmap()
    {
        _buffer->unmap();
    }

    std::shared_ptr<BufferInterface> VKStagingTexture::get_buffer() 
    { 
        return _buffer;
    }

    const VKSliceRegion& VKStagingTexture::get_slice_region(uint32_t mip_level, uint32_t array_slice, uint32_t depth_index)
    {
        if (desc.depth != 1)
        {
            assert(array_slice == 0);
            assert(z < desc.depth);

            uint32_t depth = std::max(desc.depth, uint32_t(1));

            uint32_t index = 0;
            while (mip_level-- > 0) index += depth;

            return slice_regions[index + depth_index];
        }
        else if (desc.array_size != 1)
        {
            assert(z == 0);
            assert(array_slice < desc.array_size);
            assert(slice_regions.size() == desc.mip_levels * desc.array_size);
            return slice_regions[mip_level * desc.array_size + array_slice];
        }
        else
        {
            assert(array_slice == 0);
            assert(z == 0);
            assert(slice_regions.size() == desc.mip_levels);
            return slice_regions[mip_level];
        }
    }

    VKSampler::VKSampler(const VKContext* context, const SamplerDesc& desc_)  : 
        _context(context), desc(desc_), vk_sampler_info(convert_sampler_info(desc))
    {
    }

    VKSampler::~VKSampler()
    {
        _context->device.destroySampler(vk_sampler);
    }

    bool VKSampler::initialize()
    {
        return _context->device.createSampler(&vk_sampler_info, _context->allocation_callbacks, &vk_sampler) == vk::Result::eSuccess;
    }
}