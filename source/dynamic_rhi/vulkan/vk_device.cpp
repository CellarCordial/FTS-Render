#include "vk_device.h"
#include "vk_forward.h"
#include "vk_binding.h"
#include "vk_resource.h"
#include "vk_pipeline.h"
#include "vk_frame_buffer.h"
#include "../../core/tools/check_cast.h"
#include <cstdint>


namespace fantasy 
{

    DeviceInterface* CreateDevice(const VKDeviceDesc& desc)
    {
        VKDevice* device = new VKDevice(desc);
        if (!device->initialize())
        {
            delete device;
            return nullptr;
        }
        return device;
    }

    VKDevice::VKDevice(const VKDeviceDesc& desc_) : _allocator(&context), desc(desc_)
    {        
        context.device = desc.vk_device;
        context.vk_instance = desc.vk_instance;
        context.vk_physical_device = desc.vk_physical_device;
        context.allocation_callbacks = desc.vk_allocation_callbacks;
        context.vk_loader = vk::DispatchLoaderDynamic(context.vk_instance, vkGetInstanceProcAddr, context.device, vkGetDeviceProcAddr);

        cmd_queues[uint32_t(CommandQueueType::Graphics)] = 
            std::make_unique<VKCommandQueue>(&context, CommandQueueType::Graphics, desc.vk_graphics_queue, desc.graphics_queue_index);
        cmd_queues[uint32_t(CommandQueueType::Compute)] = 
            std::make_unique<VKCommandQueue>(&context, CommandQueueType::Compute, desc.vk_compute_queue, desc.compute_queue_index);

        vk::PhysicalDeviceConservativeRasterizationPropertiesEXT conservativeRasterizationProperties;
        vk::PhysicalDeviceProperties2 deviceProperties2;
        deviceProperties2.pNext = &conservativeRasterizationProperties;

        context.vk_physical_device.getProperties2(&deviceProperties2);
        context.vk_physical_device_properties = deviceProperties2.properties;

        auto pipelineInfo = vk::PipelineCacheCreateInfo();
        vk::Result res = context.device.createPipelineCache(&pipelineInfo,
            context.allocation_callbacks,
            &context.vk_pipeline_cache);
    }

    VKDevice::~VKDevice()
    {
        if (context.vk_pipeline_cache)
        {
            context.device.destroyPipelineCache(context.vk_pipeline_cache);
         }
    }

    bool VKDevice::initialize()
    {
        return true;
    }


    HeapInterface* VKDevice::create_heap(const HeapDesc& desc)
    {
        VKHeap* heap = new VKHeap(&context, &_allocator, desc);
        if (!heap->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_heap failed.");
            delete heap;
            return nullptr;
        }
        return heap;
    }

    TextureInterface* VKDevice::create_texture(const TextureDesc& desc)
    {
        VKTexture* texture = new VKTexture(&context, &_allocator, desc);
        if (!texture->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_texture failed.");
            delete texture;
            return nullptr;
        }
        return texture;
    }
    
    StagingTextureInterface* VKDevice::create_staging_texture(const TextureDesc& desc, CpuAccessMode cpu_access)
    {
        if (cpu_access == CpuAccessMode::None)
        {
            LOG_ERROR("Call to DeviceInterface::create_staging_texture failed for using CpuAccessMode::None.");
            return nullptr;
        }

        VKStagingTexture* staging_texture = new VKStagingTexture(&context, desc, cpu_access);
        if (!staging_texture->initialize(&_allocator))
        {
            LOG_ERROR("Call to DeviceInterface::create_staging_texture failed.");
            delete staging_texture;
            return nullptr;
        }
        return staging_texture;
    }
    
    BufferInterface* VKDevice::create_buffer(const BufferDesc& desc)
    {
        VKBuffer* buffer = new VKBuffer(&context, &_allocator, desc);
        if (!buffer->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_buffer failed.");
            delete buffer;
            return nullptr;
        }
        return buffer;
    }

    TextureInterface* VKDevice::create_texture_from_native(void* native_texture, const TextureDesc& desc)
    {
        VKTexture* texture = new VKTexture(&context, &_allocator, desc);
        texture->vk_image = *static_cast<vk::Image*>(native_texture);
        return texture;
    }

    BufferInterface* VKDevice::create_buffer_from_native(void* native_buffer, const BufferDesc& desc)
    {
        VKBuffer* buffer = new VKBuffer(&context, &_allocator, desc);
        buffer->vk_buffer = *static_cast<vk::Buffer*>(native_buffer);
        return buffer;
    }

    SamplerInterface* VKDevice::create_sampler(const SamplerDesc& desc)
    {
        VKSampler* sampler = new VKSampler(&context, desc);
        if (!sampler->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_sampler failed.");
            delete sampler;
            return nullptr;
        }
        return sampler;
    }

    InputLayoutInterface* VKDevice::create_input_layout(const VertexAttributeDesc* cpDesc, uint32_t attribute_count)
    {
        StackArray<VertexAttributeDesc, MAX_VERTEX_ATTRIBUTES> attributes(attribute_count);
        for (uint32_t ix = 0; ix < attribute_count; ++ix)
        {
            attributes[ix] = cpDesc[ix];
        }

        VKInputLayout* input_layout = new VKInputLayout();
        if (!input_layout->initialize(attributes))
        {
            LOG_ERROR("Call to DeviceInterface::create_input_layout failed.");
            delete input_layout;
            return nullptr;
        }
        return input_layout;
    }
    
    GraphicsAPI VKDevice::get_graphics_api() const
    {
        return GraphicsAPI::Vulkan;
    }

    void* VKDevice::get_native_object() const
    {
        return (void*)&desc.vk_device;
    }

    
    FrameBufferInterface* VKDevice::create_frame_buffer(const FrameBufferDesc& desc)
    {
        VKFrameBuffer* frame_buffer = new VKFrameBuffer(&context, desc);
        if (!frame_buffer->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_frame_buffer failed.");
            delete frame_buffer;
            return nullptr;
        }
        return frame_buffer;
    }

    GraphicsPipelineInterface* VKDevice::create_graphics_pipeline(const GraphicsPipelineDesc& desc, FrameBufferInterface* frame_buffer)
    {
        VKGraphicsPipeline* graphics_pipeline = new VKGraphicsPipeline(&context, desc);
        if (!graphics_pipeline->initialize(frame_buffer))
        {
            LOG_ERROR("Call to DeviceInterface::create_graphics_pipeline failed.");
            delete graphics_pipeline;
            return nullptr;
        }
        return graphics_pipeline;
    }

    ComputePipelineInterface* VKDevice::create_compute_pipeline(const ComputePipelineDesc& desc)
    {
        VKComputePipeline* compute_pipeline = new VKComputePipeline(&context, desc);
        if (!compute_pipeline->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_compute_pipeline failed.");
            delete compute_pipeline;
            return nullptr;
        }
        return compute_pipeline;
    }

    BindingLayoutInterface* VKDevice::create_binding_layout(const BindingLayoutDesc& desc)
    {
        VKBindingLayout* binding_layout = new VKBindingLayout(&context, desc);
        if (!binding_layout->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_binding_layout failed.");
            delete binding_layout;
            return nullptr;
        }
        return binding_layout;
    }

    BindingLayoutInterface* VKDevice::create_bindless_layout(const BindlessLayoutDesc& desc)
    {
        VKBindingLayout* bindless_layout = new VKBindingLayout(&context, desc);
        if (!bindless_layout->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_bindless_layout failed.");
            delete bindless_layout;
            return nullptr;
        }
        return bindless_layout;
    }

    BindingSetInterface* VKDevice::create_binding_set(const BindingSetDesc& desc, std::shared_ptr<BindingLayoutInterface> binding_layout)
    {
        VKBindingSet* binding_set = new VKBindingSet(&context, desc, binding_layout);
        if (!binding_set->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_binding_set failed.");
            delete binding_set;
            return nullptr;
        }
        return binding_set;
    }
    
    BindlessSetInterface* VKDevice::create_bindless_set(std::shared_ptr<BindingLayoutInterface> binding_layout)
    {
        VKBindlessSet* bindless_set = new VKBindlessSet(&context, binding_layout);
        if (!bindless_set->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::CreateDescriptorTable failed.");
            delete bindless_set;
            return nullptr;
        }
        return bindless_set;
    }

    CommandListInterface* VKDevice::create_command_list(const CommandListDesc& desc)
    {
        VKCommandList* cmdlist = new VKCommandList(&context, this, desc);
        if (!cmdlist->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_command_list failed.");
            delete cmdlist;
            return nullptr;
        }
        return cmdlist;
    }

    void VKDevice::update_texture_tile_mappings(
        TextureInterface* texture_, 
        const TextureTilesMapping* tile_mappings, 
        uint32_t tile_mapping_num, 
        CommandQueueType execution_queue_type
    )
    {
        VKTexture* texture = check_cast<VKTexture*>(texture_);

        std::vector<vk::SparseImageMemoryBind> vk_sparse_image_memory_binds;
        std::vector<vk::SparseMemoryBind> vk_sparse_memory_binds;

        for (size_t i = 0; i < tile_mapping_num; i++)
        {
            uint32_t region_num = static_cast<uint32_t>(tile_mappings[i].regions.size());
            VKHeap* heap = tile_mappings[i].heap ? check_cast<VKHeap*>(tile_mappings[i].heap) : nullptr;
            vk::DeviceMemory vk_device_memory = heap ? heap->vk_device_memory : VK_NULL_HANDLE;

            for (uint32_t j = 0; j < region_num; ++j)
            {
                const TextureTilesMapping::Region& region = tile_mappings[i].regions[j];

                if (region.tiles_num)
                {
                    vk::SparseMemoryBind vk_memory_bind{};
                    vk_memory_bind.resourceOffset = 0;
                    vk_memory_bind.size = region.tiles_num * texture_tile_byte_size;
                    vk_memory_bind.memory = vk_device_memory;
                    vk_memory_bind.memoryOffset = vk_device_memory ? tile_mappings[i].regions[j].byte_offset : 0;
                    vk_sparse_memory_binds.push_back(vk_memory_bind);
                }
                else
                {
                    vk::ImageSubresource vk_subresource{};
                    vk_subresource.arrayLayer = region.array_level;
                    vk_subresource.mipLevel = region.mip_level;

                    vk::Offset3D vk_offset;
                    vk_offset.x = region.x;
                    vk_offset.y = region.y;
                    vk_offset.z = region.z;

                    vk::Extent3D vk_extent;
                    vk_extent.width = region.width;
                    vk_extent.height = region.height;
                    vk_extent.depth = region.depth;

                    vk::SparseImageMemoryBind vk_memory_bind{};
                    vk_memory_bind.subresource = vk_subresource;
                    vk_memory_bind.offset = vk_offset;
                    vk_memory_bind.extent = vk_extent;
                    vk_memory_bind.memory = vk_device_memory;
                    vk_memory_bind.memoryOffset = vk_device_memory ? tile_mappings[i].regions[j].byte_offset : 0;
                    vk_sparse_image_memory_binds.push_back(vk_memory_bind);
                }
            }
        }

        vk::BindSparseInfo vk_bind_sparse_info = {};

        vk::SparseImageMemoryBindInfo vk_sparse_image_memory_bind_info;
        if (!vk_sparse_image_memory_binds.empty())
        {
            vk_sparse_image_memory_bind_info.image = texture->vk_image;
            vk_sparse_image_memory_bind_info.bindCount = static_cast<uint32_t>(vk_sparse_image_memory_binds.size());
            vk_sparse_image_memory_bind_info.pBinds = vk_sparse_image_memory_binds.data();

            vk_bind_sparse_info.imageBindCount = 1;
            vk_bind_sparse_info.pImageBinds = &vk_sparse_image_memory_bind_info;
        }

        vk::SparseImageOpaqueMemoryBindInfo vk_sparse_image_opaque_memory_bind_info;
        if (!vk_sparse_memory_binds.empty())
        {
            vk_sparse_image_opaque_memory_bind_info.image = texture->vk_image;
            vk_sparse_image_opaque_memory_bind_info.bindCount = static_cast<uint32_t>(vk_sparse_memory_binds.size());
            vk_sparse_image_opaque_memory_bind_info.pBinds = vk_sparse_memory_binds.data();

            vk_bind_sparse_info.imageOpaqueBindCount = 1;
            vk_bind_sparse_info.pImageOpaqueBinds = &vk_sparse_image_opaque_memory_bind_info;
        }

        get_queue(execution_queue_type)->vk_queue.bindSparse(vk_bind_sparse_info, vk::Fence());
    }

    uint64_t VKDevice::execute_command_lists(CommandListInterface* const* cmdlists, uint64_t cmd_count, CommandQueueType queue_type)
    {
        return get_queue(queue_type)->submit(cmdlists, cmd_count);
    }


    void VKDevice::queue_wait_for_cmdlist(CommandQueueType wait_queue_type, CommandQueueType execution_queue_type, uint64_t submit_id)
    {
        VKCommandQueue* wait_queue = get_queue(wait_queue_type);
        VKCommandQueue* execution_queue = get_queue(execution_queue_type);

        wait_queue->add_wait_semaphore(execution_queue->vk_tracking_semaphore, submit_id);
    }

	void VKDevice::wait_for_idle()
    {
        context.device.waitIdle();
    }

    void VKDevice::collect_garbage()
    {
        for (auto& queue : cmd_queues)
        {
            queue->retire_command_buffers();
        }
    }

    uint64_t VKDevice::queue_get_completed_id(CommandQueueType queue)
    {
        return context.device.getSemaphoreCounterValue(get_queue(queue)->vk_tracking_semaphore);
    }

    VKCommandQueue* VKDevice::get_queue(CommandQueueType queue) const 
    { 
        return cmd_queues[static_cast<uint32_t>(queue)].get(); 
    }


}