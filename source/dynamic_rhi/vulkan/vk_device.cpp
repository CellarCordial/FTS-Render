#include "vk_device.h"
#include "vk_forward.h"
#include "vk_binding.h"
#include "vk_resource.h"
#include "vk_pipeline.h"
#include "vk_frame_buffer.h"


namespace fantasy 
{
    VKDevice::VKDevice(const VKDeviceDesc& desc_) : _allocator(&context), desc(desc_)
    {        
        context.device = desc.vk_device;
        context.vk_instance = desc.vk_instance;
        context.vk_physical_device = desc.vk_physical_device;
        context.allocation_callbacks = desc.vk_allocation_callbacks;

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

    void VKDevice::run_garbage_collection()
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