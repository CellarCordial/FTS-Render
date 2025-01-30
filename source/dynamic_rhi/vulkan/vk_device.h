#ifndef DYNAMIC_RHI_VULKAN_DEVICE_H
#define DYNAMIC_RHI_VULKAN_DEVICE_H

#include "../dynamic_rhi.h"
#include "vk_allocator.h"
#include "vk_cmdlist.h"
#include "../../core/tools/bit_allocator.h"

namespace fantasy
{
    class VKEventQuery : public EventQueryInterface
    {
    public:
        ~VKEventQuery() override = default;

        CommandQueueType queue = CommandQueueType::Graphics;
        uint64_t cmdlist_id = 0;
    };
    
    class TimerQuery : public TimerQueryInterface
    {
    public:
        explicit TimerQuery(BitSetAllocator& allocator)  : _query_allocator(allocator) {}
        ~TimerQuery() override;

    public:
        uint32_t begin_query_index = INVALID_SIZE_32;
        uint32_t end_query_index = INVALID_SIZE_32;

        bool started = false;
        bool resolved = false;
        float time = 0.f;

    private:
        BitSetAllocator& _query_allocator;
    };


    class Device : public DeviceInterface
    {
    public:
        Device(const VKDeviceDesc& desc);
        ~Device() override;

        VKCommandQueue* getQueue(CommandQueueType queue) const { return _cmd_queues[static_cast<uint32_t>(queue)].get(); }
        vk::QueryPool getTimerQueryPool() const { return _timer_query_pool; }

        // DeivceInterface.
        HeapInterface* create_heap(const HeapDesc& desc) override;
        
        BufferInterface* create_buffer(const BufferDesc& desc) override;
        TextureInterface* create_texture(const TextureDesc& desc) override;
        BufferInterface* create_buffer_from_native(void* pNativeBuffer, const BufferDesc& desc) override;
        TextureInterface* create_texture_from_native(void* pNativeTexture, const TextureDesc& desc) override;
        
        StagingTextureInterface* create_staging_texture(const TextureDesc& desc, CpuAccessMode cpu_access) override;

        SamplerInterface* create_sampler(const SamplerDesc& desc) override;

        InputLayoutInterface* create_input_layout(const VertexAttributeDesc* cpDesc, uint32_t dwAttributeNum, Shader* pVertexShader) override;
        

        EventQueryInterface* create_event_query() override;
        bool set_event_query(EventQueryInterface* query, CommandQueueType queue_type) override;
        bool poll_event_query(EventQueryInterface* query) override;
        bool wait_event_query(EventQueryInterface* query) override;
        bool reset_event_query(EventQueryInterface* query) override;

        TimerQueryInterface* create_timer_query() override;
        bool poll_timer_query(TimerQueryInterface* query) override;
        bool reset_timer_query(TimerQueryInterface* query) override;
        float get_timer_query_time(TimerQueryInterface* query) override;


        void* get_native_descriptor_heap(DescriptorHeapType type) const override;

        FrameBufferInterface* create_frame_buffer(const FrameBufferDesc& desc) override;
        
        GraphicsPipelineInterface* create_graphics_pipeline(const GraphicsPipelineDesc& desc, FrameBufferInterface* frame_buffer) override;
        ComputePipelineInterface* create_compute_pipeline(const ComputePipelineDesc& desc) override;

        BindingLayoutInterface* create_binding_layout(const BindingLayoutDesc& desc) override;
        BindingLayoutInterface* create_bindless_layout(const BindlessLayoutDesc& desc) override;
        BindlessSetInterface* create_bindless_set(BindingLayoutInterface* pLayout) override;
        BindingSetInterface* create_binding_set(const BindingSetDesc& desc, BindingLayoutInterface* pLayout) override;

        CommandListInterface* create_command_list(const CommandListDesc& desc) override;
        uint64_t execute_command_lists(
            CommandListInterface* const* cmdlists,
            uint64_t cmd_count = 1,
            CommandQueueType queue_type = CommandQueueType::Graphics
        ) override;

		ray_tracing::PipelineInterface* create_ray_tracing_pipline(const ray_tracing::PipelineDesc& desc) override;
		ray_tracing::AccelStructInterface* create_accel_struct(const ray_tracing::AccelStructDesc& desc) override;
        
        bool queue_wait_for_cmdlist(CommandQueueType WaitQueueType, CommandQueueType queue_type, uint64_t stInstance) override;

        void wait_for_idle() override;
        void collect_garbage() override;

        GraphicsAPI get_graphics_api() const override;
        void* get_native_object() const override;



        VkSemaphore get_queue_semaphore(CommandQueueType queue);
        void queue_signal_semaphore(CommandQueueType executionQueue, VkSemaphore semaphore, uint64_t value);
        void queue_waitFor_semaphore(CommandQueueType waitQueue, VkSemaphore semaphore, uint64_t value);
        uint64_t queue_get_completed_instance(CommandQueueType queue);
        FrameBufferInterface* create_framebuffer(
            VkRenderPass renderPass, 
            VkFramebuffer framebuffer,
            const FrameBufferDesc& desc, 
            bool transferOwnership
        );

        void* map_buffer(BufferInterface* buffer, CpuAccessMode flags, uint64_t offset, size_t size) const;

    private:
        VKContext _context;
        VKMemoryAllocator _allocator;
        
        vk::QueryPool _timer_query_pool = nullptr;
        BitSetAllocator _timer_query_allocator;
        std::mutex _mutex;

        std::array<std::unique_ptr<VKCommandQueue>, uint32_t(CommandQueueType::Count)> _cmd_queues;
    };
}






#endif