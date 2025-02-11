#include "vk_cmdlist.h"
#include "../../core/tools/check_cast.h"
#include "vk_device.h"
#include "vk_resource.h"
#include "vk_pipeline.h"
#include "vk_binding.h"
#include "vk_convert.h"
#include "vk_frame_buffer.h"
#include "../device.h"
#include <cstdint>
#include <memory>

namespace fantasy 
{
    VKCommandBuffer::VKCommandBuffer(const VKContext* context_, uint64_t recording_id_) : 
        context(context_), recording_id(recording_id_)
    {
    }

    VKCommandBuffer::~VKCommandBuffer()
    {
        context->device.destroyCommandPool(vk_cmdpool, context->allocation_callbacks);
    }

    bool VKCommandBuffer::initialize(uint32_t queue_family_index)
    {
        vk::CommandPoolCreateInfo cmd_pool_create_info;
        cmd_pool_create_info.queueFamilyIndex = queue_family_index;
        cmd_pool_create_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
                                     vk::CommandPoolCreateFlagBits::eTransient;

        ReturnIfFalse(vk::Result::eSuccess != context->device.createCommandPool(
            &cmd_pool_create_info, 
            context->allocation_callbacks, 
            &vk_cmdpool
        ));
        
        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = vk_cmdpool;
        alloc_info.commandBufferCount = 1;
        return context->device.allocateCommandBuffers(&alloc_info, &vk_cmdbuffer) != vk::Result::eSuccess;
    }

    VKCommandQueue::VKCommandQueue(
        const VKContext* context, 
        CommandQueueType queue_type, 
        vk::Queue vk_queue_, 
        uint32_t queue_family_index
    ) : 
        _context(context), 
        queue_type(queue_type),
        vk_queue(vk_queue_),
        queue_family_index(queue_family_index)
    {
        last_recording_id = 0;

        vk::SemaphoreTypeCreateInfo vk_semaphore_type_info{};
        vk_semaphore_type_info.pNext = nullptr;
        vk_semaphore_type_info.semaphoreType = vk::SemaphoreType::eTimeline;
        vk_semaphore_type_info.initialValue = 0;
        
        vk::SemaphoreCreateInfo vk_semaphore_info;
        vk_semaphore_info.pNext = &vk_semaphore_type_info;
        vk_semaphore_info.flags = vk::SemaphoreCreateFlags();

        vk_tracking_semaphore = context->device.createSemaphore(vk_semaphore_info, context->allocation_callbacks);
    }

    VKCommandQueue::~VKCommandQueue()
    {
        _context->device.destroySemaphore(vk_tracking_semaphore, _context->allocation_callbacks);
    }

    std::shared_ptr<VKCommandBuffer> VKCommandQueue::get_command_buffer()
    {
        std::lock_guard lock(_mutex);

        std::shared_ptr<VKCommandBuffer> cmdbuffer;
        if (_cmdbuffers_pool.empty())
        {
            cmdbuffer = std::make_shared<VKCommandBuffer>(_context, ++last_recording_id);
            if (!cmdbuffer->initialize(queue_family_index))
            {
                LOG_ERROR("Create command buffer failed.");
                return nullptr;
            }
        }
        else
        {
            cmdbuffer = _cmdbuffers_pool.front();
            _cmdbuffers_pool.pop_front();
        }
        
        return cmdbuffer;
    }

    void VKCommandQueue::add_wait_semaphore(vk::Semaphore semaphore, uint64_t value)
    {
        if (!semaphore) return;

        _vk_wait_semaphores.push_back(semaphore);
        _wait_semaphore_values.push_back(value);
    }

    void VKCommandQueue::add_signal_semaphore(vk::Semaphore semaphore, uint64_t value)
    {
        if (!semaphore) return;

        _vk_signal_semaphores.push_back(semaphore);
        _signal_semaphore_values.push_back(value);
    }

    uint64_t VKCommandQueue::submit(CommandListInterface* const* cmdlists, uint32_t cmd_count)
    {
        if (cmd_count == 0) return last_submitted_id;

        last_submitted_id++;

        std::vector<vk::PipelineStageFlags> vk_wait_stages(
            _vk_wait_semaphores.size(), 
            vk::PipelineStageFlagBits::eTopOfPipe
        );

        std::vector<vk::CommandBuffer> vk_cmdbuffers(cmd_count);
        for (size_t ix = 0; ix < cmd_count; ix++)
        {
            auto cmdbuffer = check_cast<VKCommandList*>(cmdlists[ix])->get_current_command_buffer();
            
            vk_cmdbuffers[ix] = cmdbuffer->vk_cmdbuffer;
            
            _cmdbuffers_in_flight.push_back(cmdbuffer);
        }
        
        _vk_signal_semaphores.push_back(vk_tracking_semaphore);
        _signal_semaphore_values.push_back(last_submitted_id);

        vk::TimelineSemaphoreSubmitInfo vk_timeline_semaphore_submit_info{};
        vk_timeline_semaphore_submit_info.pNext = nullptr;
        vk_timeline_semaphore_submit_info.signalSemaphoreValueCount = static_cast<uint32_t>(_signal_semaphore_values.size());
        vk_timeline_semaphore_submit_info.pSignalSemaphoreValues = _signal_semaphore_values.data();

        if (!_wait_semaphore_values.empty())
        {
            vk_timeline_semaphore_submit_info.waitSemaphoreValueCount = static_cast<uint32_t>(_wait_semaphore_values.size());
            vk_timeline_semaphore_submit_info.pWaitSemaphoreValues = _wait_semaphore_values.data();
        }

        vk::SubmitInfo vk_submit_info;
        vk_submit_info.pNext = &vk_timeline_semaphore_submit_info;
        vk_submit_info.waitSemaphoreCount = static_cast<uint32_t>(_vk_wait_semaphores.size());
        vk_submit_info.pWaitSemaphores = _vk_wait_semaphores.data();
        vk_submit_info.pWaitDstStageMask = vk_wait_stages.data();
        vk_submit_info.commandBufferCount = cmd_count;
        vk_submit_info.pCommandBuffers = vk_cmdbuffers.data();
        vk_submit_info.signalSemaphoreCount = static_cast<uint32_t>(_vk_signal_semaphores.size());
        vk_submit_info.pSignalSemaphores = _vk_signal_semaphores.data();

        try 
        {
            vk_queue.submit(vk_submit_info);
        }
        catch (vk::DeviceLostError e)
        {
            LOG_CRITICAL("Device moved!");
            return INVALID_SIZE_64;
        }

        for (size_t ix = 0; ix < cmd_count; ix++)
        {
            check_cast<VKCommandList*>(cmdlists[ix])->executed(*this, last_submitted_id);
        }

        _vk_wait_semaphores.clear();
        _wait_semaphore_values.clear();
        _vk_signal_semaphores.clear();
        _signal_semaphore_values.clear();
        
        return last_submitted_id;
    }

    uint64_t VKCommandQueue::get_last_finished_id()
    {
        return _context->device.getSemaphoreCounterValue(vk_tracking_semaphore);
    }

    void VKCommandQueue::retire_command_buffers()
    {
        std::list<std::shared_ptr<VKCommandBuffer>> flight_cmdbuffers = std::move(_cmdbuffers_in_flight);

        for (const auto& cmdbuffer : flight_cmdbuffers)
        {
            if (cmdbuffer->submit_id <= get_last_finished_id())
            {
                cmdbuffer->submit_id = INVALID_SIZE_64;

                _cmdbuffers_pool.push_back(cmdbuffer);
            }
            else
            {
                _cmdbuffers_in_flight.push_back(cmdbuffer);
            }
        }
    }
    
    std::shared_ptr<VKCommandBuffer> VKCommandQueue::get_command_buffer_in_flight(uint64_t submit_id)
    {
        for (const auto& cmdbuffer : _cmdbuffers_in_flight)
        {
            if (cmdbuffer->submit_id == submit_id) return cmdbuffer;
        }

        return nullptr;
    }

    bool VKCommandQueue::wait_command_list(uint64_t cmdlist_id, uint64_t timeout)
    {
        if (cmdlist_id == INVALID_SIZE_32) return false;

        return wait_for_semaphore(_context, vk_tracking_semaphore, cmdlist_id);
    }


    VKBufferChunk::VKBufferChunk(DeviceInterface* device, const BufferDesc& desc, bool map_memory)
    {
        buffer = std::shared_ptr<BufferInterface>(device->create_buffer(desc));
        mapped_memory = map_memory ? buffer->map(CpuAccessMode::Write) : nullptr;
        buffer_size = desc.byte_size;
    }

    VKBufferChunk::~VKBufferChunk()
    {
        if (mapped_memory != nullptr) buffer->unmap();
    }


    VKUploadManager::VKUploadManager(DeviceInterface* device, uint64_t default_chunk_size, uint64_t max_memory, bool is_scratch_buffer) :
        _device(device), 
        _default_chunk_size(default_chunk_size), 
        _scratch_max_memory(max_memory), 
        _is_scratch_buffer(is_scratch_buffer)
    {
    }

    std::shared_ptr<VKBufferChunk> VKUploadManager::create_chunk(uint64_t size)
    {
        std::shared_ptr<VKBufferChunk> chunk = std::make_shared<VKBufferChunk>();

        if (_is_scratch_buffer)
        {
            BufferDesc desc{};
            desc.name = "scratch buffer chunk";
            desc.byte_size = size;
            desc.cpu_access = CpuAccessMode::None;
            desc.allow_unordered_access = true;

            chunk->buffer = std::shared_ptr<BufferInterface>(_device->create_buffer(desc));
            chunk->mapped_memory = nullptr;
            chunk->buffer_size = size;
        }
        else
        {
            BufferDesc desc;
            desc.name = "upload buffer chunk";
            desc.byte_size = size;
            desc.cpu_access = CpuAccessMode::Write;

            chunk->buffer = std::shared_ptr<BufferInterface>(_device->create_buffer(desc));
            chunk->mapped_memory = chunk->buffer->map(CpuAccessMode::Write);
            chunk->buffer_size = size;
        }

        return chunk;
    }

    bool VKUploadManager::suballocate_buffer(
        uint64_t size, 
        BufferInterface** out_buffer, 
        uint64_t* out_offset, 
        void** out_cpu_address, 
        uint64_t current_version, 
        uint32_t alignment
    )
    {
        ReturnIfFalse(out_cpu_address);

        std::shared_ptr<VKBufferChunk> retire_chunk;

        if (_current_chunk)
        {
            uint64_t offset = align(_current_chunk->write_pointer, static_cast<uint64_t>(alignment));
            uint64_t data_end = offset + size;

            if (data_end <= _current_chunk->buffer_size)
            {
                _current_chunk->write_pointer = data_end;

                *out_buffer = _current_chunk->buffer.get();
                *out_offset = offset;
                if (_current_chunk->mapped_memory)
                {
                    *out_cpu_address = (char*)_current_chunk->mapped_memory + offset;
                }

                return true;
            }

            retire_chunk = _current_chunk;
            _current_chunk.reset();
        }

        if (retire_chunk) _chunk_pool.push_back(retire_chunk);


        uint64_t completed_id = check_cast<VKDevice*>(_device)->queue_get_completed_id(get_version_queue_type(current_version));

        for (auto iter = _chunk_pool.begin(); iter != _chunk_pool.end(); ++iter)
        {
            std::shared_ptr<VKBufferChunk> chunk = *iter;

            if (is_version_submitted(chunk->version) && get_version_id(chunk->version) <= completed_id)
            {
                chunk->version = 0;
            }

            if (chunk->version == 0 && chunk->buffer_size >= size)
            {
                _chunk_pool.erase(iter);
                _current_chunk = chunk;
                break;
            }
        }

        if (!_current_chunk)
        {
            uint64_t alloc_size = align(std::max(size, _default_chunk_size), VKBufferChunk::align_size);

            ReturnIfFalse((_scratch_max_memory > 0 && _allocated_memory + alloc_size <= _scratch_max_memory));

            _current_chunk = create_chunk(alloc_size);
        }

        _current_chunk->version = current_version;
        _current_chunk->write_pointer = size;

        *out_buffer = _current_chunk->buffer.get();
        *out_offset = 0;
        *out_cpu_address = _current_chunk->mapped_memory;

        return true;
    }

    void VKUploadManager::submit_chunks(uint64_t current_version, uint64_t submitted_version)
    {
        if (_current_chunk)
        {
            _chunk_pool.push_back(_current_chunk);
            _current_chunk.reset();
        }

        for (const auto& chunk : _chunk_pool)
        {
            if (chunk->version == current_version)
            {
                chunk->version = submitted_version;
            }
        }
    }


    VKCommandList::VKCommandList(const VKContext* context, DeviceInterface* device, const CommandListDesc& desc) : 
        _context(context), 
        _device(device), 
        _desc(desc), 
        _upload_manager(device, desc.upload_chunk_size, 0, false), 
        _scratch_manager(device, desc.scratch_chunk_size, desc.scratch_max_mamory, true)
    {
    }

    bool VKCommandList::initialize()
    {
        return true;
    }

    
    bool VKCommandList::open()
    {
        _current_cmdbuffer = check_cast<VKDevice*>(_device)->get_queue(_desc.queue_type)->get_command_buffer();

        vk::CommandBufferBeginInfo vk_cmd_buffer_beginInfo{};
        vk_cmd_buffer_beginInfo.pNext = nullptr;
        vk_cmd_buffer_beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        vk_cmd_buffer_beginInfo.pInheritanceInfo = nullptr;

        ReturnIfFalse(_current_cmdbuffer->vk_cmdbuffer.begin(&vk_cmd_buffer_beginInfo) == vk::Result::eSuccess);
        return true;
    }

    bool VKCommandList::close()
    {
        commit_barriers();

        _current_cmdbuffer->vk_cmdbuffer.end();

        flush_volatile_buffer_mapped_memory();
        return true;
    }

    
    void VKCommandList::clear_texture_float(
        TextureInterface* texture_, 
        const TextureSubresourceSet& subresource, 
        const Color& clear_color
    )
    {
        vk::ClearColorValue vk_clear_color{};
        vk_clear_color.float32[0] = clear_color.r;
        vk_clear_color.float32[1] = clear_color.g;
        vk_clear_color.float32[2] = clear_color.b;
        vk_clear_color.float32[3] = clear_color.a;

        VKTexture* texture = check_cast<VKTexture*>(texture_);
        
        set_texture_state(texture, subresource, ResourceStates::CopyDst);
        commit_barriers();

        vk::ImageSubresourceRange vk_subresource_range{};
        vk_subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
        vk_subresource_range.baseArrayLayer = subresource.base_array_slice;
        vk_subresource_range.layerCount = subresource.array_slice_count;
        vk_subresource_range.baseMipLevel = subresource.base_mip_level;
        vk_subresource_range.levelCount = subresource.mip_level_count;
        
        _current_cmdbuffer->vk_cmdbuffer.clearColorImage(
            texture->vk_image,
            vk::ImageLayout::eTransferDstOptimal,
            &vk_clear_color,
            1, 
            &vk_subresource_range
        );
    }
    
    void VKCommandList::clear_texture_uint(
        TextureInterface* texture_,
        const TextureSubresourceSet& subresource,
        uint32_t clear_color
    )
    {
        int int_clear_color = static_cast<int>(clear_color);

        vk::ClearColorValue vk_clear_color{};
        vk_clear_color.uint32[0] = clear_color;
        vk_clear_color.uint32[1] = clear_color;
        vk_clear_color.uint32[2] = clear_color;
        vk_clear_color.uint32[3] = clear_color;
        vk_clear_color.int32[0] = int_clear_color;
        vk_clear_color.int32[1] = int_clear_color;
        vk_clear_color.int32[2] = int_clear_color;
        vk_clear_color.int32[3] = int_clear_color;

        VKTexture* texture = check_cast<VKTexture*>(texture_);
        
        set_texture_state(texture, subresource, ResourceStates::CopyDst);
        commit_barriers();

        vk::ImageSubresourceRange vk_subresource_range{};
        vk_subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
        vk_subresource_range.baseArrayLayer = subresource.base_array_slice;
        vk_subresource_range.layerCount = subresource.array_slice_count;
        vk_subresource_range.baseMipLevel = subresource.base_mip_level;
        vk_subresource_range.levelCount = subresource.mip_level_count;
        
        _current_cmdbuffer->vk_cmdbuffer.clearColorImage(
            texture->vk_image,
            vk::ImageLayout::eTransferDstOptimal,
            &vk_clear_color,
            1, 
            &vk_subresource_range
        );
    }
    
    void VKCommandList::clear_depth_stencil_texture(
        TextureInterface* texture_,
        const TextureSubresourceSet& subresource,
        bool clear_depth,
        float depth,
        bool clear_stencil,
        uint8_t stencil
    )
    {
        assert(clear_depth || clear_stencil);

        VKTexture* texture = check_cast<VKTexture*>(texture_);
        
        set_texture_state(texture, subresource, ResourceStates::CopyDst);
        commit_barriers();

        vk::ImageAspectFlags vk_image_aspect_flags{};
        if (clear_depth) vk_image_aspect_flags |= vk::ImageAspectFlagBits::eDepth;
        if (clear_stencil) vk_image_aspect_flags |= vk::ImageAspectFlagBits::eStencil;

        vk::ImageSubresourceRange vk_subresource_range{};
        vk_subresource_range.aspectMask = vk_image_aspect_flags;
        vk_subresource_range.baseArrayLayer = subresource.base_array_slice;
        vk_subresource_range.layerCount = subresource.array_slice_count;
        vk_subresource_range.baseMipLevel = subresource.base_mip_level;
        vk_subresource_range.levelCount = subresource.mip_level_count;

        vk::ClearDepthStencilValue vk_clear_value{};
        vk_clear_value.depth = depth;
        vk_clear_value.stencil = stencil;

        _current_cmdbuffer->vk_cmdbuffer.clearDepthStencilImage(
            texture->vk_image,
            vk::ImageLayout::eTransferDstOptimal,
            &vk_clear_value,
            1, 
            &vk_subresource_range
        );
    }

    void VKCommandList::clear_Texture(TextureInterface* texture_, TextureSubresourceSet subresource, const vk::ClearColorValue& clear_color)
    {
        VKTexture* texture = check_cast<VKTexture*>(texture_);
        
        set_texture_state(texture, subresource, ResourceStates::CopyDst);
        commit_barriers();

        vk::ImageSubresourceRange vk_subresource_range{};
        vk_subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
        vk_subresource_range.baseArrayLayer = subresource.base_array_slice;
        vk_subresource_range.layerCount = subresource.array_slice_count;
        vk_subresource_range.baseMipLevel = subresource.base_mip_level;
        vk_subresource_range.levelCount = subresource.mip_level_count;
        
        _current_cmdbuffer->vk_cmdbuffer.clearColorImage(
            texture->vk_image,
            vk::ImageLayout::eTransferDstOptimal,
            &clear_color,
            1, 
            &vk_subresource_range
        );
    }

    void VKCommandList::copy_texture(
        TextureInterface* dst,
        const TextureSlice& dst_slice,
        TextureInterface* src,
        const TextureSlice& src_slice
    )
    {
        VKTexture* dst_texture = check_cast<VKTexture*>(dst);
        VKTexture* src_texture = check_cast<VKTexture*>(src);

        FormatInfo src_format_info = get_format_info(src_texture->desc.format);
        vk::ImageAspectFlags vk_src_aspect_flags = vk::ImageAspectFlagBits::eNone;
        if (src_format_info.has_depth || src_format_info.has_stencil)
        {
            if (src_format_info.has_depth) vk_src_aspect_flags |= vk::ImageAspectFlagBits::eDepth;
            if (src_format_info.has_stencil) vk_src_aspect_flags |= vk::ImageAspectFlagBits::eStencil;
        }
        else 
        {
            vk_src_aspect_flags = vk::ImageAspectFlagBits::eColor;
        }

        TextureSubresourceSet src_subresource = TextureSubresourceSet(
            src_slice.mip_level, 1,
            src_slice.array_slice, 1
        );    

        vk::ImageSubresourceLayers vk_src_subresource_layer{};
        vk_src_subresource_layer.aspectMask = vk_src_aspect_flags;
        vk_src_subresource_layer.mipLevel = src_subresource.base_mip_level;
        vk_src_subresource_layer.baseArrayLayer = src_subresource.base_array_slice;
        vk_src_subresource_layer.layerCount = src_subresource.array_slice_count;   


        FormatInfo dst_format_info = get_format_info(dst_texture->desc.format);
        vk::ImageAspectFlags vk_dst_aspect_flags = vk::ImageAspectFlagBits::eNone;
        if (dst_format_info.has_depth || dst_format_info.has_stencil)
        {
            if (dst_format_info.has_depth) vk_dst_aspect_flags |= vk::ImageAspectFlagBits::eDepth;
            if (dst_format_info.has_stencil) vk_dst_aspect_flags |= vk::ImageAspectFlagBits::eStencil;
        }
        else 
        {
            vk_dst_aspect_flags = vk::ImageAspectFlagBits::eColor;
        }

        TextureSubresourceSet dst_subresource = TextureSubresourceSet(
            dst_slice.mip_level, 1,
            dst_slice.array_slice, 1
        );

        vk::ImageSubresourceLayers vk_dst_subresource_layer{};
        vk_dst_subresource_layer.aspectMask = vk_dst_aspect_flags;
        vk_dst_subresource_layer.mipLevel = dst_subresource.base_mip_level;
        vk_dst_subresource_layer.baseArrayLayer = dst_subresource.base_array_slice;
        vk_dst_subresource_layer.layerCount = dst_subresource.array_slice_count;   

        
        const VkExtent3D vk_extent = {
            std::min(src_slice.width, dst_slice.width),
            std::min(src_slice.height, dst_slice.height),
            std::min(src_slice.depth, dst_slice.depth)
        };
        

        vk::ImageCopy vk_image_copy{};
        vk_image_copy.srcSubresource = vk_src_subresource_layer;
        vk_image_copy.srcOffset = vk::Offset3D(src_slice.x, src_slice.y, src_slice.z);
        vk_image_copy.srcSubresource = vk_dst_subresource_layer;
        vk_image_copy.srcOffset = vk::Offset3D(dst_slice.x, dst_slice.y, dst_slice.z);
        vk_image_copy.extent = vk_extent;
        
        set_texture_state(src_texture, TextureSubresourceSet(src_slice.mip_level, 1, src_slice.array_slice, 1), ResourceStates::CopySrc);
        set_texture_state(dst_texture, TextureSubresourceSet(dst_slice.mip_level, 1, dst_slice.array_slice, 1), ResourceStates::CopyDst);
        commit_barriers();

        _current_cmdbuffer->vk_cmdbuffer.copyImage(
            src_texture->vk_image, 
            vk::ImageLayout::eTransferSrcOptimal,
            dst_texture->vk_image, 
            vk::ImageLayout::eTransferDstOptimal,
            { vk_image_copy }
        );
    }
    
    void VKCommandList::copy_texture(
        StagingTextureInterface* dst,
        const TextureSlice& dst_slice,
        TextureInterface* src,
        const TextureSlice& src_slice
    )
    {
        VKTexture* src_texture = check_cast<VKTexture*>(src);
        VKStagingTexture* dst_texture = check_cast<VKStagingTexture*>(dst);

        VKSliceRegion dst_region = dst_texture->get_slice_region(dst_slice.mip_level, dst_slice.array_slice, dst_slice.z);
        
        // Vulkan 规范要求.
        assert((dst_region.offset & 0x3) == 0);

        FormatInfo src_format_info = get_format_info(src_texture->desc.format);
        vk::ImageAspectFlags vk_src_aspect_flags = vk::ImageAspectFlagBits::eNone;
        if (src_format_info.has_depth || src_format_info.has_stencil)
        {
            if (src_format_info.has_depth) vk_src_aspect_flags |= vk::ImageAspectFlagBits::eDepth;
            if (src_format_info.has_stencil) vk_src_aspect_flags |= vk::ImageAspectFlagBits::eStencil;
        }
        else 
        {
            vk_src_aspect_flags = vk::ImageAspectFlagBits::eColor;
        }

        TextureSubresourceSet src_subresource = TextureSubresourceSet(
            src_slice.mip_level, 1,
            src_slice.array_slice, 1
        );

        vk::ImageSubresourceLayers vk_src_subresource_layer{};
        vk_src_subresource_layer.aspectMask = vk_src_aspect_flags;
        vk_src_subresource_layer.mipLevel = src_subresource.base_mip_level;
        vk_src_subresource_layer.baseArrayLayer = src_subresource.base_array_slice;
        vk_src_subresource_layer.layerCount = 1;   


        const VkExtent3D vk_extent = {
            src_slice.width,
            src_slice.height,
            src_slice.depth
        };
        
        vk::BufferImageCopy vk_buffer_image_copy{};
        vk_buffer_image_copy.imageSubresource = vk_src_subresource_layer;
        vk_buffer_image_copy.imageOffset = vk::Offset3D(src_slice.x, src_slice.y, src_slice.z);
        vk_buffer_image_copy.imageExtent = vk_extent;
        vk_buffer_image_copy.bufferOffset = dst_region.offset;
        vk_buffer_image_copy.bufferRowLength = dst_slice.width;
        vk_buffer_image_copy.bufferImageHeight = dst_slice.height;


        set_buffer_state(dst_texture->get_buffer().get(), ResourceStates::CopyDst);
        set_texture_state(src_texture, src_subresource, ResourceStates::CopySrc);
        commit_barriers();

        _current_cmdbuffer->vk_cmdbuffer.copyImageToBuffer(
            src_texture->vk_image, 
            vk::ImageLayout::eTransferSrcOptimal,
            check_cast<VKBuffer>(dst_texture->get_buffer())->vk_buffer, 
            1,
            &vk_buffer_image_copy
        );
    }
    
    void VKCommandList::copy_texture(
        TextureInterface* dst,
        const TextureSlice& dst_slice,
        StagingTextureInterface* src,
        const TextureSlice& src_slice
    )
    {
        VKTexture* dst_texture = check_cast<VKTexture*>(dst);
        VKStagingTexture* src_texture = check_cast<VKStagingTexture*>(src);

        VKSliceRegion src_region = src_texture->get_slice_region(src_slice.mip_level, src_slice.array_slice, src_slice.z);
        
        // Vulkan 规范要求.
        assert((src_region.offset & 0x3) == 0);

        FormatInfo dst_format_info = get_format_info(dst_texture->desc.format);
        vk::ImageAspectFlags vk_dst_aspect_flags = vk::ImageAspectFlagBits::eNone;
        if (dst_format_info.has_depth || dst_format_info.has_stencil)
        {
            if (dst_format_info.has_depth) vk_dst_aspect_flags |= vk::ImageAspectFlagBits::eDepth;
            if (dst_format_info.has_stencil) vk_dst_aspect_flags |= vk::ImageAspectFlagBits::eStencil;
        }
        else 
        {
            vk_dst_aspect_flags = vk::ImageAspectFlagBits::eColor;
        }

        TextureSubresourceSet dst_subresource = TextureSubresourceSet(
            dst_slice.mip_level, 1,
            dst_slice.array_slice, 1
        );    

        vk::ImageSubresourceLayers vk_dst_subresource_layer{};
        vk_dst_subresource_layer.aspectMask = vk_dst_aspect_flags;
        vk_dst_subresource_layer.mipLevel = dst_subresource.base_mip_level;
        vk_dst_subresource_layer.baseArrayLayer = dst_subresource.base_array_slice;
        vk_dst_subresource_layer.layerCount = 1;   


        const VkExtent3D vk_extent = {
            dst_slice.width,
            dst_slice.height,
            dst_slice.depth
        };
        
        vk::BufferImageCopy vk_buffer_image_copy{};
        vk_buffer_image_copy.imageSubresource = vk_dst_subresource_layer;
        vk_buffer_image_copy.imageOffset = vk::Offset3D(src_slice.x, src_slice.y, src_slice.z);
        vk_buffer_image_copy.imageExtent = vk_extent;
        vk_buffer_image_copy.bufferOffset = src_region.offset;
        vk_buffer_image_copy.bufferRowLength = src_slice.width;
        vk_buffer_image_copy.bufferImageHeight = src_slice.height;


        set_buffer_state(src_texture->get_buffer().get(), ResourceStates::CopySrc);
        set_texture_state(dst_texture, dst_subresource, ResourceStates::CopyDst);
        commit_barriers();

        _current_cmdbuffer->vk_cmdbuffer.copyBufferToImage(
            check_cast<VKBuffer>(src_texture->get_buffer())->vk_buffer, 
            dst_texture->vk_image, 
            vk::ImageLayout::eTransferDstOptimal,
            1,
            &vk_buffer_image_copy
        );
    }
        
    bool VKCommandList::write_texture(
        TextureInterface* dst,
        uint32_t array_slice,
        uint32_t mip_level,
        const uint8_t* data,
        uint64_t data_size
    ) 
    {
        const TextureDesc& texture_desc = dst->get_desc();

        uint64_t mip_width = std::max(texture_desc.width >> mip_level, uint32_t(1));
        uint64_t mip_height = std::max(texture_desc.height >> mip_level, uint32_t(1));
        uint64_t mip_depth = std::max(texture_desc.depth >> mip_level, uint32_t(1));

        const FormatInfo& format_info = get_format_info(texture_desc.format);
        uint64_t row_pitch = mip_width * format_info.size;
        uint64_t depth_pitch = row_pitch * mip_height;
        uint64_t texture_size = depth_pitch * mip_depth;

        ReturnIfFalse(texture_size == data_size);

        BufferInterface* upload_buffer;
        uint64_t upload_offset;
        void* mapped_address;
        ReturnIfFalse(_upload_manager.suballocate_buffer(
            data_size,
            &upload_buffer,
            &upload_offset,
            &mapped_address,
            make_version(_current_cmdbuffer->recording_id, _desc.queue_type, false)
        ));

        uint8_t* dst_address = reinterpret_cast<uint8_t*>(mapped_address);
        for (uint32_t z = 0; z < mip_depth; z++)
        {
            const uint8_t* src_address = reinterpret_cast<const uint8_t*>(data) + depth_pitch * z;
            for (uint32_t row = 0; row < mip_height; row++)
            {
                memcpy(dst_address, src_address, row_pitch);
                dst_address += row_pitch;
                src_address += row_pitch;
            }
        }

        vk::ImageAspectFlags vk_src_aspect_flags = vk::ImageAspectFlagBits::eNone;
        if (format_info.has_depth || format_info.has_stencil)
        {
            if (format_info.has_depth) vk_src_aspect_flags |= vk::ImageAspectFlagBits::eDepth;
            if (format_info.has_stencil) vk_src_aspect_flags |= vk::ImageAspectFlagBits::eStencil;
        }
        else 
        {
            vk_src_aspect_flags = vk::ImageAspectFlagBits::eColor;
        }

        vk::ImageSubresourceLayers vk_image_subresource_layer{};
        vk_image_subresource_layer.aspectMask = vk_src_aspect_flags;
        vk_image_subresource_layer.mipLevel = mip_level;
        vk_image_subresource_layer.baseArrayLayer = array_slice;
        vk_image_subresource_layer.layerCount = 1;

        vk::BufferImageCopy vk_buffer_image_copy;
        vk_buffer_image_copy.bufferOffset = upload_offset;
        vk_buffer_image_copy.bufferRowLength = mip_width;
        vk_buffer_image_copy.bufferImageHeight = mip_height;
        vk_buffer_image_copy.imageSubresource = vk_image_subresource_layer;
        vk_buffer_image_copy.imageExtent = vk::Extent3D(mip_width, mip_height, mip_depth);


        set_texture_state(dst, TextureSubresourceSet(mip_level, 1, array_slice, 1), ResourceStates::CopyDst);
        commit_barriers();

        _current_cmdbuffer->vk_cmdbuffer.copyBufferToImage(
            check_cast<VKBuffer*>(upload_buffer)->vk_buffer,
            check_cast<VKTexture*>(dst)->vk_image, 
            vk::ImageLayout::eTransferDstOptimal,
            1, 
            &vk_buffer_image_copy
        );
        return true;
    }

    bool VKCommandList::write_buffer(
        BufferInterface* buffer_, 
        const void* data, 
        uint64_t data_size, 
        uint64_t dst_byte_offset
    )
    {
        VKBuffer* buffer = check_cast<VKBuffer*>(buffer_);

        ReturnIfFalse(data_size <= buffer->desc.byte_size);

        if (buffer->desc.is_volatile_constant_buffer)
        {
            ReturnIfFalse(dst_byte_offset == 0);
            return write_volatile_buffer(buffer, data, data_size);
        }

        // 根据 Vulkan 规范, vkCmdUpdateBuffer 要求数据小于或等于 64 kB, 并且 offset 和 Data Size 是 4 的倍数.
        if (data_size <= 65536 && (dst_byte_offset & 3) == 0)
        {
            set_buffer_state(buffer_, ResourceStates::CopyDst);
            commit_barriers();

            _current_cmdbuffer->vk_cmdbuffer.updateBuffer(
                buffer->vk_buffer, 
                dst_byte_offset, 
                (data_size + 3) & ~3ull, 
                data
            );
        }
        else
        {
            ReturnIfFalse(buffer->desc.cpu_access != CpuAccessMode::Write);

            BufferInterface* upload_buffer;
            uint64_t upload_offset;
            void* mapped_address;
            _upload_manager.suballocate_buffer(
                data_size, 
                &upload_buffer, 
                &upload_offset, 
                &mapped_address, 
                make_version(_current_cmdbuffer->recording_id, _desc.queue_type, false)
            );

            memcpy(mapped_address, data, data_size);

            VKBuffer* dst = buffer;
            VKBuffer* src = check_cast<VKBuffer*>(upload_buffer);

            copy_buffer(buffer, dst_byte_offset, upload_buffer, upload_offset, data_size);
        }
        return true;
    }

    bool VKCommandList::write_volatile_buffer(BufferInterface* buffer_, const void* data, size_t data_size)
    {
        std::array<uint64_t, uint32_t(CommandQueueType::Count)> queue_complete_ids = {
            check_cast<VKDevice*>(_device)->get_queue(CommandQueueType::Graphics)->get_last_finished_id(),
            check_cast<VKDevice*>(_device)->get_queue(CommandQueueType::Compute)->get_last_finished_id()
        };

        VKBuffer* buffer = check_cast<VKBuffer*>(buffer_);

        uint32_t version_search_start = buffer->version_search_start;
        uint32_t max_versions = volatile_constant_buffer_max_version;
        uint32_t version = 0;

        uint64_t origin_version = 0;

        while (true)
        {
            bool found = false;

            for (uint32_t ix = 0; ix < max_versions; ix++)
            {
                version = (version_search_start + ix) % max_versions;

                origin_version = buffer->version_tracking[version];

                if (origin_version == 0) { found = true; break; }

                CommandQueueType queue_type = get_version_queue_type(origin_version);

                if (
                    is_version_submitted(origin_version) && 
                    get_version_id(origin_version) <= queue_complete_ids[static_cast<uint32_t>(queue_type)]
                )
                {
                    found = true;
                    break;
                }
            }

            // volatile_constant_buffer_max_version 不够大.
            ReturnIfFalse(!found);

            uint64_t new_version = make_version(_current_cmdbuffer->recording_id, _desc.queue_type, false);

            if (buffer->version_tracking[version].compare_exchange_weak(origin_version, new_version)) break;
        }

        buffer->version_search_start = (version + 1) % max_versions;

        
        VKVolatileBufferVersion& volatile_version = _volatile_buffer_versions[buffer_];

        if (!volatile_version.initialized)
        {
            volatile_version.min_version = volatile_constant_buffer_max_version;
            volatile_version.max_version = -1;
            volatile_version.initialized = true;
        }

        volatile_version.latest_version = static_cast<int32_t>(version);
        volatile_version.min_version = std::min(static_cast<int32_t>(version), volatile_version.min_version);
        volatile_version.max_version = std::max(static_cast<int32_t>(version), volatile_version.max_version);

        void* dst_address = (char*)buffer->mapped_volatile_memory + version * buffer->desc.byte_size;
        memcpy(dst_address, data, data_size);

        return true;
    }

    void VKCommandList::clear_buffer_uint(BufferInterface* buffer, uint32_t clear_value)
    {        
        set_buffer_state(buffer, ResourceStates::CopyDst);
        commit_barriers();

        _current_cmdbuffer->vk_cmdbuffer.fillBuffer(
            *reinterpret_cast<vk::Buffer*>(buffer->get_native_object()), 
            0, 
            buffer->get_desc().byte_size, 
            clear_value
        );
    }
        
    void VKCommandList::copy_buffer(
        BufferInterface* dst,
        uint64_t dst_byte_offset,
        BufferInterface* src,
        uint64_t src_byte_offset,
        uint64_t data_byte_size
    )
    {
        VKBuffer* dst_buffer = check_cast<VKBuffer*>(dst);
        VKBuffer* src_buffer = check_cast<VKBuffer*>(src);

        set_buffer_state(src, ResourceStates::CopySrc);
        set_buffer_state(dst, ResourceStates::CopyDst);
        commit_barriers();

        vk::BufferCopy vk_buffer_copy{};
        vk_buffer_copy.dstOffset = dst_byte_offset;
        vk_buffer_copy.srcOffset = src_byte_offset;
        vk_buffer_copy.size = data_byte_size;

        _current_cmdbuffer->vk_cmdbuffer.copyBuffer(src_buffer->vk_buffer, dst_buffer->vk_buffer, { vk_buffer_copy });
    }
    
    bool VKCommandList::set_graphics_state(const GraphicsState& state)
    {
        _current_graphics_state = state;

        VKGraphicsPipeline* pipeline = check_cast<VKGraphicsPipeline*>(_current_graphics_state.pipeline);

        for (size_t ix = 0; ix < _current_graphics_state.binding_sets.size(); ix++)
        {
            ReturnIfFalse(_current_graphics_state.binding_sets[ix]->get_layout() == pipeline->desc.binding_layouts[ix].get());

            set_binding_resource_state(_current_graphics_state.binding_sets[ix]);
        }

        if (_current_graphics_state.index_buffer_binding.is_valid())
        {
            set_buffer_state(_current_graphics_state.index_buffer_binding.buffer.get(), ResourceStates::IndexBuffer);
        }

        for (const auto& vertex_binding : _current_graphics_state.vertex_buffer_bindings)
        {
            set_buffer_state(vertex_binding.buffer.get(), ResourceStates::VertexBuffer);
        }

        const FrameBufferDesc& frame_buffer_desc = _current_graphics_state.frame_buffer->get_desc();
        for (const auto& attachment : frame_buffer_desc.color_attachments)
        {
            set_texture_state(attachment.texture.get(), attachment.subresource, ResourceStates::RenderTarget);
        }
        if (frame_buffer_desc.depth_stencil_attachment.is_valid())
        {
            set_texture_state(
                frame_buffer_desc.depth_stencil_attachment.texture.get(), 
                frame_buffer_desc.depth_stencil_attachment.subresource,
                ResourceStates::DepthWrite
            );
        }

        if (_current_graphics_state.indirect_buffer)
        {
            set_buffer_state(_current_graphics_state.indirect_buffer, ResourceStates::IndirectArgument);
        }

        commit_barriers();

        
        VKFrameBuffer* frame_buffer = check_cast<VKFrameBuffer*>(_current_graphics_state.frame_buffer);


        _current_cmdbuffer->vk_cmdbuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->vk_pipeline);


        const auto& frame_buffer_info = frame_buffer->get_info();

        vk::Rect2D vk_render_area{};
        vk_render_area.offset = vk::Offset2D(0, 0);
        vk_render_area.extent = vk::Extent2D(frame_buffer_info.width, frame_buffer_info.height);

        vk::RenderPassBeginInfo vk_render_pass_begin_info{};
        vk_render_pass_begin_info.renderPass = frame_buffer->vk_render_pass;
        vk_render_pass_begin_info.framebuffer = frame_buffer->vk_frame_buffer;
        vk_render_pass_begin_info.renderArea = vk_render_area;
        vk_render_pass_begin_info.clearValueCount = 0;
        vk_render_pass_begin_info.pClearValues = nullptr;
            
        _current_cmdbuffer->vk_cmdbuffer.beginRenderPass(vk_render_pass_begin_info, vk::SubpassContents::eInline);


        bind_binding_sets(_current_graphics_state.binding_sets, vk::PipelineBindPoint::eGraphics, pipeline->vk_pipeline_layout);


        if (!_current_graphics_state.viewport_state.viewports.empty())
        {
            StackArray<vk::Viewport, MAX_VIEWPORTS> viewports;
            for (const auto& viewport : _current_graphics_state.viewport_state.viewports)
            {
                viewports.push_back(vk_viewport_with_dx_coords(viewport));
            }

            _current_cmdbuffer->vk_cmdbuffer.setViewport(0, static_cast<uint32_t>(viewports.size()), viewports.data());
        }

        if (!_current_graphics_state.viewport_state.scissor_rects.empty())
        {
            StackArray<vk::Rect2D, MAX_VIEWPORTS> scissors;
            for (const auto& scissor : _current_graphics_state.viewport_state.scissor_rects)
            {
                scissors.push_back(vk::Rect2D(
                        vk::Offset2D(scissor.min_x, scissor.min_y),
                        vk::Extent2D(scissor.max_x - scissor.min_x, scissor.max_y - scissor.min_y)
                ));
            }

            _current_cmdbuffer->vk_cmdbuffer.setScissor(0, uint32_t(scissors.size()), scissors.data());
        }

        if (pipeline->desc.render_state.depth_stencil_state.dynamic_stencil_ref)
        {
            _current_cmdbuffer->vk_cmdbuffer.setStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, _current_graphics_state.dynamic_stencil_ref_value);
        }

        if (pipeline->use_blend_constant)
        {
            _current_cmdbuffer->vk_cmdbuffer.setBlendConstants(&_current_graphics_state.blend_constant_color.r);
        }

        if (_current_graphics_state.index_buffer_binding.is_valid())
        {
            vk::IndexType vk_index_type;
            switch (_current_graphics_state.index_buffer_binding.format) 
            {
            case Format::R16_UINT: vk_index_type = vk::IndexType::eUint16; break;
            case Format::R32_UINT: vk_index_type = vk::IndexType::eUint32; break;
            default:
                LOG_ERROR("Invalid index binding type.");
                return false;
            }

            _current_cmdbuffer->vk_cmdbuffer.bindIndexBuffer(
                check_cast<VKBuffer>(_current_graphics_state.index_buffer_binding.buffer)->vk_buffer,
                _current_graphics_state.index_buffer_binding.offset,
                vk_index_type
            );
        }

        if (!_current_graphics_state.vertex_buffer_bindings.empty())
        {
            vk::Buffer vk_vertex_buffers[MAX_VERTEX_ATTRIBUTES];
            vk::DeviceSize vk_vertex_buffer_offsets[MAX_VERTEX_ATTRIBUTES];
            uint32_t max_vertex_buffer_index = 0;

            for (const auto& binding : _current_graphics_state.vertex_buffer_bindings)
            {
                vk_vertex_buffers[binding.slot] = check_cast<VKBuffer>(binding.buffer)->vk_buffer;
                vk_vertex_buffer_offsets[binding.slot] = binding.offset;
                max_vertex_buffer_index = std::max(max_vertex_buffer_index, binding.slot);
            }

            _current_cmdbuffer->vk_cmdbuffer.bindVertexBuffers(0, max_vertex_buffer_index + 1, vk_vertex_buffers, vk_vertex_buffer_offsets);
        }

        return true;
    }

    bool VKCommandList::set_compute_state(const ComputeState& state)
    {
        _current_compute_state = state;

        VKComputePipeline* pipeline = check_cast<VKComputePipeline*>(_current_compute_state.pipeline);
        
        ReturnIfFalse(pipeline->desc.binding_layouts.size() == _current_compute_state.binding_sets.size());

        for (uint32_t ix = 0; ix < _current_compute_state.binding_sets.size(); ix++)
        {
            ReturnIfFalse(_current_compute_state.binding_sets[ix]->get_layout() == pipeline->desc.binding_layouts[ix].get());

            set_binding_resource_state(_current_compute_state.binding_sets[ix]);
        }

        if (_current_compute_state.indirect_buffer)
        {
            set_buffer_state(_current_compute_state.indirect_buffer, ResourceStates::IndirectArgument);
        }

        commit_barriers();


        _current_cmdbuffer->vk_cmdbuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline->vk_pipeline);

        bind_binding_sets(_current_compute_state.binding_sets, vk::PipelineBindPoint::eCompute, pipeline->vk_pipeline_layout);

        return true;
    }

    void VKCommandList::set_binding_resource_state(BindingSetInterface* binding_set_)
    {
        std::vector<BindingSetItem> binding_items;

        if (binding_set_->is_bindless())
        {
            const auto& bindings = check_cast<VKBindlessSet*>(binding_set_)->binding_items;
            binding_items.insert(binding_items.end(), bindings.begin(), bindings.end());
        }
        else 
        {
            const auto& bindings = binding_set_->get_desc().binding_items;
            binding_items.insert(binding_items.end(), bindings.begin(), bindings.end());
        }

        for (auto binding : binding_items)
        {
            switch(binding.type)
            {
                case ResourceViewType::Texture_SRV:
                    set_texture_state(check_cast<TextureInterface>(binding.resource).get(), binding.subresource, ResourceStates::ShaderResource);
                    break;

                case ResourceViewType::Texture_UAV:
                    set_texture_state(check_cast<TextureInterface>(binding.resource).get(), binding.subresource, ResourceStates::UnorderedAccess);
                    break;

                case ResourceViewType::TypedBuffer_SRV:
                case ResourceViewType::StructuredBuffer_SRV:
                case ResourceViewType::RawBuffer_SRV:
                    set_buffer_state(check_cast<BufferInterface>(binding.resource).get(), ResourceStates::ShaderResource);
                    break;

                case ResourceViewType::TypedBuffer_UAV:
                case ResourceViewType::StructuredBuffer_UAV:
                case ResourceViewType::RawBuffer_UAV:
                    set_buffer_state(check_cast<BufferInterface>(binding.resource).get(), ResourceStates::UnorderedAccess);
                    break;

                case ResourceViewType::ConstantBuffer:
                    set_buffer_state(check_cast<BufferInterface>(binding.resource).get(), ResourceStates::ConstantBuffer);
                    break;
                default:
                    continue;
            }
        }
    }

    void VKCommandList::bind_binding_sets(
        const PipelineStateBindingSetArray& binding_sets,
        vk::PipelineBindPoint vk_bind_point, 
        vk::PipelineLayout vk_layout
    )
    {
        StackArray<vk::DescriptorSet, MAX_BINDING_LAYOUTS> vk_descriptor_sets;
        StackArray<uint32_t, MAX_VOLATILE_CONSTANT_BUFFERS> dynamic_offsets;

        for (uint32_t ix = 0; ix < binding_sets.size(); ++ix)
        {
            BindingSetInterface* binding_set = binding_sets[ix];

            const BindingSetDesc& desc = binding_set->get_desc();
            if (binding_set->is_bindless())
            {
                vk_descriptor_sets.push_back(check_cast<VKBindlessSet*>(binding_set)->vk_descriptor_set);
            }
            else
            {
                VKBindingSet* bindingSet = check_cast<VKBindingSet*>(binding_set);
                vk_descriptor_sets.push_back(bindingSet->vk_descriptor_set);

                for (BufferInterface* buffer : bindingSet->volatile_constant_buffers)
                {
                    auto iter = _volatile_buffer_versions.find(buffer);
                    assert(iter != _volatile_buffer_versions.end());

                    uint64_t offset = iter->second.latest_version * buffer->get_desc().byte_size;
                    dynamic_offsets.push_back(static_cast<uint32_t>(offset));
                }
            }
        }

        if (!vk_descriptor_sets.empty())
        {
            _current_cmdbuffer->vk_cmdbuffer.bindDescriptorSets(
                vk_bind_point, 
                vk_layout,
                0, 
                static_cast<uint32_t>(vk_descriptor_sets.size()), 
                vk_descriptor_sets.data(),
                static_cast<uint32_t>(dynamic_offsets.size()), 
                dynamic_offsets.data()
            );
        }
    }

    bool VKCommandList::dispatch(
        const ComputeState& state,
        uint32_t thread_group_num_x, 
        uint32_t thread_group_num_y,
        uint32_t thread_group_num_z,
        const void* push_constant  
    )
    {
        ReturnIfFalse(set_compute_state(state));

        if (push_constant)
        {
            VKComputePipeline* pipeline = check_cast<VKComputePipeline*>(_current_compute_state.pipeline);
            _current_cmdbuffer->vk_cmdbuffer.pushConstants(
                pipeline->vk_pipeline_layout, 
                pipeline->vk_push_constant_visibility, 
                0, 
                static_cast<uint32_t>(pipeline->push_constant_size), 
                push_constant
            );
        }

        _current_cmdbuffer->vk_cmdbuffer.dispatch(thread_group_num_x, thread_group_num_y, thread_group_num_z);

        return true;
    }
        
    bool VKCommandList::dispatch_indirect(
        const ComputeState& state, 
        uint32_t offset_bytes,
        const void* push_constant
    )
    {
        ReturnIfFalse(set_compute_state(state));

        if (push_constant)
        {
            VKComputePipeline* pipeline = check_cast<VKComputePipeline*>(_current_compute_state.pipeline);
            _current_cmdbuffer->vk_cmdbuffer.pushConstants(
                pipeline->vk_pipeline_layout, 
                pipeline->vk_push_constant_visibility, 
                0, 
                static_cast<uint32_t>(pipeline->push_constant_size), 
                push_constant
            );
        }

        VKBuffer* buffer = check_cast<VKBuffer*>(state.indirect_buffer);
        _current_cmdbuffer->vk_cmdbuffer.dispatchIndirect(buffer->vk_buffer, offset_bytes);

        return true;
    }

    

    bool VKCommandList::draw(
        const GraphicsState& state, 
        const DrawArguments& arguments, 
        const void* push_constant
    )
    {
        ReturnIfFalse(set_graphics_state(state));

        if (push_constant)
        {
            VKGraphicsPipeline* pipeline = check_cast<VKGraphicsPipeline*>(_current_graphics_state.pipeline);
            _current_cmdbuffer->vk_cmdbuffer.pushConstants(
                pipeline->vk_pipeline_layout, 
                pipeline->vk_push_constant_visibility, 
                0, 
                static_cast<uint32_t>(pipeline->push_constant_size), 
                push_constant
            );
        }

        _current_cmdbuffer->vk_cmdbuffer.draw(
            arguments.index_count,
            arguments.instance_count,
            arguments.start_vertex_location,
            arguments.start_instance_location
        );
        _current_cmdbuffer->vk_cmdbuffer.endRenderPass();
        return true;
    }

    bool VKCommandList::draw_indexed(
        const GraphicsState& state, 
        const DrawArguments& arguments, 
        const void* push_constant
    )
    {
        ReturnIfFalse(set_graphics_state(state));
        
        if (push_constant)
        {
            VKGraphicsPipeline* pipeline = check_cast<VKGraphicsPipeline*>(_current_graphics_state.pipeline);
            _current_cmdbuffer->vk_cmdbuffer.pushConstants(
                pipeline->vk_pipeline_layout, 
                pipeline->vk_push_constant_visibility, 
                0, 
                static_cast<uint32_t>(pipeline->push_constant_size), 
                push_constant
            );
        }

        _current_cmdbuffer->vk_cmdbuffer.drawIndexed(
            arguments.index_count,
            arguments.instance_count,
            arguments.start_index_location,
            arguments.start_vertex_location,
            arguments.start_instance_location
        );
        _current_cmdbuffer->vk_cmdbuffer.endRenderPass();
        return true;
    }

    bool VKCommandList::draw_indirect(
        const GraphicsState& state, 
        uint32_t offset_bytes,
        uint32_t draw_count,
        const void* push_constant
    )
    {
        ReturnIfFalse(set_graphics_state(state));

        if (push_constant)
        {
            VKGraphicsPipeline* pipeline = check_cast<VKGraphicsPipeline*>(_current_graphics_state.pipeline);
            _current_cmdbuffer->vk_cmdbuffer.pushConstants(
                pipeline->vk_pipeline_layout, 
                pipeline->vk_push_constant_visibility, 
                0, 
                static_cast<uint32_t>(pipeline->push_constant_size), 
                push_constant
            );
        }

        VKBuffer* buffer = check_cast<VKBuffer*>(state.indirect_buffer);
        _current_cmdbuffer->vk_cmdbuffer.drawIndirect(
            buffer->vk_buffer, 
            offset_bytes, 
            draw_count, 
            sizeof(DrawIndirectArguments)
        );
        _current_cmdbuffer->vk_cmdbuffer.endRenderPass();
        return true;
    }

    bool VKCommandList::draw_indexed_indirect(
        const GraphicsState& state, 
        uint32_t offset_bytes,
        uint32_t draw_count,
        const void* push_constant
    )
    {
        ReturnIfFalse(set_graphics_state(state));

        if (push_constant)
        {
            VKGraphicsPipeline* pipeline = check_cast<VKGraphicsPipeline*>(_current_graphics_state.pipeline);
            _current_cmdbuffer->vk_cmdbuffer.pushConstants(
                pipeline->vk_pipeline_layout, 
                pipeline->vk_push_constant_visibility, 
                0, 
                static_cast<uint32_t>(pipeline->push_constant_size), 
                push_constant
            );
        }

        VKBuffer* buffer = check_cast<VKBuffer*>(state.indirect_buffer);
        _current_cmdbuffer->vk_cmdbuffer.drawIndexedIndirect(
            buffer->vk_buffer, 
            offset_bytes, 
            draw_count, 
            sizeof(DrawIndexedIndirectArguments)
        );
        _current_cmdbuffer->vk_cmdbuffer.endRenderPass();
        return true;
    }

    void VKCommandList::set_enable_uav_barrier_for_texture(TextureInterface* texture, bool enable_barriers)
    {
        _resource_state_tracker.set_texture_enable_uav_barriers(texture, enable_barriers);
    }

    void VKCommandList::set_enable_uav_barrier_for_buffer(BufferInterface* buffer, bool enable_barriers)
    {
        _resource_state_tracker.set_buffer_enable_uav_barriers(buffer, enable_barriers);
    }

    void VKCommandList::set_texture_state(TextureInterface* texture, const TextureSubresourceSet& subresource_set, ResourceStates states)
    {
        set_texture_state(texture, subresource_set, states);
    }

    void VKCommandList::set_buffer_state(BufferInterface* buffer, ResourceStates states)
    {
        set_buffer_state(buffer, states);
    }

    void VKCommandList::commit_barriers()
    {
        if (
            _resource_state_tracker.get_buffer_barriers().empty() && 
            _resource_state_tracker.get_texture_barriers().empty()
        ) 
            return;

        std::vector<vk::ImageMemoryBarrier> vk_image_barriers;
        
        vk::PipelineStageFlags vk_before_stage_flags = vk::PipelineStageFlags(0);
        vk::PipelineStageFlags vk_after_stage_flags = vk::PipelineStageFlags(0);

        for (const TextureBarrier& barrier : _resource_state_tracker.get_texture_barriers())
        {
            ResourceStateMapping before = convert_resource_state(barrier.state_before);
            ResourceStateMapping after = convert_resource_state(barrier.state_after);

            if (
                (before.vk_stage_flags != vk_before_stage_flags || after.vk_stage_flags != vk_after_stage_flags) && 
                !vk_image_barriers.empty()
            )
            {
                _current_cmdbuffer->vk_cmdbuffer.pipelineBarrier(
                    vk_before_stage_flags, 
                    vk_after_stage_flags,
                    vk::DependencyFlags(), 
                    {}, 
                    {}, 
                    vk_image_barriers
                );
                vk_image_barriers.clear();
            }

            vk_before_stage_flags = before.vk_stage_flags;
            vk_after_stage_flags = after.vk_stage_flags;

            assert(after.vk_image_layout != vk::ImageLayout::eUndefined);

            VKTexture* texture = static_cast<VKTexture*>(barrier.texture);

            const FormatInfo& format_info = get_format_info(texture->desc.format);

            vk::ImageAspectFlags vk_aspect_mask = vk::ImageAspectFlagBits(0);
            if (format_info.has_depth) vk_aspect_mask |= vk::ImageAspectFlagBits::eDepth;
            if (format_info.has_stencil) vk_aspect_mask |= vk::ImageAspectFlagBits::eStencil;
            if (vk_aspect_mask == vk::ImageAspectFlagBits(0)) vk_aspect_mask = vk::ImageAspectFlagBits::eColor;

            vk::ImageSubresourceRange vk_subresource_range{};
            vk_subresource_range.aspectMask = vk_aspect_mask;
            vk_subresource_range.baseArrayLayer = barrier.is_entire_texture ? 0 : barrier.array_slice;
            vk_subresource_range.layerCount = barrier.is_entire_texture ? texture->desc.array_size : 1;
            vk_subresource_range.baseMipLevel = barrier.is_entire_texture ? 0 : barrier.mip_level;
            vk_subresource_range.levelCount = barrier.is_entire_texture ? texture->desc.mip_levels : 1;

            vk::ImageMemoryBarrier vk_memory_barrire{};
            vk_memory_barrire.srcAccessMask = before.vk_access_flags;
            vk_memory_barrire.dstAccessMask = after.vk_access_flags;
            vk_memory_barrire.oldLayout = before.vk_image_layout;
            vk_memory_barrire.newLayout = after.vk_image_layout;
            vk_memory_barrire.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vk_memory_barrire.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vk_memory_barrire.image = texture->vk_image;
            vk_memory_barrire.subresourceRange = vk_subresource_range;

            vk_image_barriers.push_back(vk_memory_barrire);
        }

        if (!vk_image_barriers.empty())
        {
            _current_cmdbuffer->vk_cmdbuffer.pipelineBarrier(
                vk_before_stage_flags, 
                vk_after_stage_flags,
                vk::DependencyFlags(), 
                {}, 
                {}, 
                vk_image_barriers
            );
        }
        vk_image_barriers.clear();

        std::vector<vk::BufferMemoryBarrier> vk_buffer_barriers;

        vk_before_stage_flags = vk::PipelineStageFlags(0);
        vk_after_stage_flags = vk::PipelineStageFlags(0);

        for (const BufferBarrier& barrier : _resource_state_tracker.get_buffer_barriers())
        {
            ResourceStateMapping before = convert_resource_state(barrier.state_before);
            ResourceStateMapping after = convert_resource_state(barrier.state_after);

            if (
                (before.vk_stage_flags != vk_before_stage_flags || after.vk_stage_flags != vk_after_stage_flags) && 
                !vk_buffer_barriers.empty())
            {
                _current_cmdbuffer->vk_cmdbuffer.pipelineBarrier(
                    vk_before_stage_flags, 
                    vk_after_stage_flags,
                    vk::DependencyFlags(), 
                    {}, 
                    vk_buffer_barriers, 
                    {}
                );

                vk_buffer_barriers.clear();
            }

            vk_before_stage_flags = before.vk_stage_flags;
            vk_after_stage_flags = after.vk_stage_flags;

            VKBuffer* buffer = static_cast<VKBuffer*>(barrier.buffer);

            vk::BufferMemoryBarrier vk_buffer_barrier{};
            vk_buffer_barrier.srcAccessMask = before.vk_access_flags;
            vk_buffer_barrier.dstAccessMask = after.vk_access_flags;
            vk_buffer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vk_buffer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vk_buffer_barrier.buffer = buffer->vk_buffer;
            vk_buffer_barrier.offset = 0;
            vk_buffer_barrier.size = buffer->desc.byte_size;

            vk_buffer_barriers.push_back(vk_buffer_barrier);
        }

        if (!vk_buffer_barriers.empty())
        {
            _current_cmdbuffer->vk_cmdbuffer.pipelineBarrier(
                vk_before_stage_flags, 
                vk_after_stage_flags,
                vk::DependencyFlags(), 
                {}, 
                vk_buffer_barriers, 
                {}
            );
        }
        vk_buffer_barriers.clear();

        _resource_state_tracker.clear_barriers();
    }

    
    ResourceStates VKCommandList::get_buffer_state(BufferInterface* buffer)
    {
        return _resource_state_tracker.get_buffer_state(buffer);
    }

    ResourceStates VKCommandList::get_texture_state(
        TextureInterface* texture,
        uint32_t array_slice,
        uint32_t mip_level
    )
    {
        return _resource_state_tracker.get_texture_state(texture, array_slice, mip_level);
    }

    DeviceInterface* VKCommandList::get_deivce()
    {
        return _device;
    }

    const CommandListDesc& VKCommandList::get_desc()
    {
        return _desc;
    }

    void* VKCommandList::get_native_object()
    {
        return &_current_cmdbuffer->vk_cmdbuffer;
    }

    std::shared_ptr<VKCommandBuffer> VKCommandList::get_current_command_buffer() const
    {
        return _current_cmdbuffer;
    }
    
    void VKCommandList::executed(VKCommandQueue& queue, uint64_t submit_id)
    {
        _current_cmdbuffer->submit_id = submit_id;

        const CommandQueueType queue_type = queue.queue_type;
        const uint64_t recording_id = _current_cmdbuffer->recording_id;

        _current_cmdbuffer = nullptr;
        
        _upload_manager.submit_chunks(
            make_version(recording_id, queue_type, false),
            make_version(submit_id, queue_type, true)
        );
        
        _scratch_manager.submit_chunks(
            make_version(recording_id, queue_type, false),
            make_version(submit_id, queue_type, true)
        );
        
        submit_volatile_buffers(recording_id, submit_id);
        _volatile_buffer_versions.clear();
    }

    void VKCommandList::flush_volatile_buffer_mapped_memory()
    {
        // 确保在 GPU 使用数据之前，CPU 对 volatile 缓冲区的写入操作已经同步到 GPU.

        std::vector<vk::MappedMemoryRange> vk_mapped_ranges;

        for (auto& iter : _volatile_buffer_versions)
        {
            VKBuffer* buffer = check_cast<VKBuffer*>(iter.first);
            VKVolatileBufferVersion& state = iter.second;

            if (state.max_version < state.min_version || !state.initialized) continue;

            int32_t version_count = state.max_version - state.min_version + 1;

            vk::MappedMemoryRange vk_mapped_range{}; 
            vk_mapped_range.memory = buffer->vk_device_memory;
            vk_mapped_range.offset = state.min_version * buffer->desc.byte_size;
            vk_mapped_range.size = version_count * buffer->desc.byte_size;

            vk_mapped_ranges.push_back(vk_mapped_range);
        }

        if (!vk_mapped_ranges.empty())
        {
            _context->device.flushMappedMemoryRanges(vk_mapped_ranges);
        }
    }

    void VKCommandList::submit_volatile_buffers(uint64_t recording_id, uint64_t submitted_id)
    {
        uint64_t old_version = make_version(recording_id, _desc.queue_type, false);
        uint64_t new_version = make_version(submitted_id, _desc.queue_type, true);

        for (auto& iter : _volatile_buffer_versions)
        {
            VKVolatileBufferVersion& volatile_version = iter.second;
            
            if (!volatile_version.initialized) continue;
        
            VKBuffer* buffer = check_cast<VKBuffer*>(iter.first);
            for (int32_t version = volatile_version.min_version; version <= volatile_version.max_version; version++)
            {
                uint64_t expected = old_version;
                buffer->version_tracking[version].compare_exchange_strong(expected, new_version);
            }
        }
    }
     
}