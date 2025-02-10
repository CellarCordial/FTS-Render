#ifndef DYNAMIC_RHI_VULKAN_DEVICE_H
#define DYNAMIC_RHI_VULKAN_DEVICE_H

#include "../dynamic_rhi.h"
#include "vk_memory.h"
#include "vk_cmdlist.h"

namespace fantasy
{
    class VKDevice : public DeviceInterface
    {
    public:
        VKDevice(const VKDeviceDesc& desc);
        ~VKDevice() override;

        bool initialize();

        HeapInterface* create_heap(const HeapDesc& desc) override;
        
        BufferInterface* create_buffer(const BufferDesc& desc) override;
        TextureInterface* create_texture(const TextureDesc& desc) override;
        BufferInterface* create_buffer_from_native(void* native_buffer, const BufferDesc& desc) override;
        TextureInterface* create_texture_from_native(void* native_texture, const TextureDesc& desc) override;
        
        StagingTextureInterface* create_staging_texture(const TextureDesc& desc, CpuAccessMode cpu_access) override;

        SamplerInterface* create_sampler(const SamplerDesc& desc) override;

        InputLayoutInterface* create_input_layout(const VertexAttributeDesc* cpDesc, uint32_t attribute_count) override;
        
        FrameBufferInterface* create_frame_buffer(const FrameBufferDesc& desc) override;
        
        GraphicsPipelineInterface* create_graphics_pipeline(const GraphicsPipelineDesc& desc, FrameBufferInterface* frame_buffer) override;
        ComputePipelineInterface* create_compute_pipeline(const ComputePipelineDesc& desc) override;

        BindingLayoutInterface* create_binding_layout(const BindingLayoutDesc& desc) override;
        BindingLayoutInterface* create_bindless_layout(const BindlessLayoutDesc& desc) override;
        BindlessSetInterface* create_bindless_set(std::shared_ptr<BindingLayoutInterface> binding_layout) override;
        BindingSetInterface* create_binding_set(const BindingSetDesc& desc, std::shared_ptr<BindingLayoutInterface> binding_layout) override;

        CommandListInterface* create_command_list(const CommandListDesc& desc) override;
        
        uint64_t execute_command_lists(
            CommandListInterface* const* cmdlists,
            uint64_t cmd_count = 1,
            CommandQueueType queue_type = CommandQueueType::Graphics
        ) override;

        void queue_wait_for_cmdlist(CommandQueueType WaitQueueType, CommandQueueType queue_type, uint64_t submit_id) override;

        void wait_for_idle() override;
        void run_garbage_collection() override;

        GraphicsAPI get_graphics_api() const override;
        void* get_native_object() const override;

        uint64_t queue_get_completed_id(CommandQueueType queue);
        VKCommandQueue* get_queue(CommandQueueType queue) const;

    public:
        VKDeviceDesc desc;

        VKContext context;
        
        std::array<std::unique_ptr<VKCommandQueue>, uint32_t(CommandQueueType::Count)> cmd_queues;
        
    private:
        VKMemoryAllocator _allocator;
        std::mutex _mutex;
    };
}






#endif