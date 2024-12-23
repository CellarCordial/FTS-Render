#ifndef RHI_DX12_DEVICE_H
#define RHI_DX12_DEVICE_H


#include "../dynamic_rhi.h"
#include "../draw.h"
#include "dx12_cmdlist.h"
#include <d3d12.h>
#include <mutex>

namespace fantasy 
{
    class DX12EventQuery : public EventQueryInterface
    {
    public:
        bool initialize() { return true; }

        bool start(ID3D12Fence* d3d12_fence, uint64_t fence_counter);
        bool poll();
        void wait(HANDLE wait_event);
        void reset();

    private:

        Microsoft::WRL::ComPtr<ID3D12Fence> _d3d12_fence;
        uint64_t _fence_counter = 0;
        bool _started = false;
        bool _resolved = false;
    };


    class DX12TimerQuery : public TimerQueryInterface
    {
    public:
        DX12TimerQuery(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps, uint32_t query_index);

        bool initialize() { return true; }

        // TimerQueryInterface
        bool poll();
        float get_time(HANDLE wait_event, uint64_t stFrequency);
        void reset();

    public:
        uint32_t begin_query_index = 0;
        uint32_t end_query_index = 0;
        bool _started = false;
        bool _resolved = false;
        
        Microsoft::WRL::ComPtr<ID3D12Fence> _d3d12_fence;
        uint64_t _fence_counter = 0;
        
    private:
        const DX12Context* _context = nullptr; 

        float _time = 0.0f;
        DX12DescriptorHeaps* _descriptor_heaps = nullptr;
    };



    class DX12Device : public DeviceInterface
    {
    public:
        explicit DX12Device(const DX12DeviceDesc& desc);
        ~DX12Device() noexcept;

        bool initialize();

        // DeviceInterface
        HeapInterface* create_heap(const HeapDesc& desc) override;
        TextureInterface* create_texture(const TextureDesc& desc) override;
        StagingTextureInterface* create_staging_texture(const TextureDesc& desc, CpuAccessMode cpu_access) override;
        BufferInterface* create_buffer(const BufferDesc& desc) override;

        TextureInterface* create_texture_from_native(void* pNativeTexture, const TextureDesc& desc) override;
        BufferInterface* create_buffer_from_native(void* pNativeBuffer, const BufferDesc& desc) override;

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
        
        GraphicsAPI get_graphics_api() const override;
        void* get_native_descriptor_heap(DescriptorHeapType type) const override;
        void* get_native_object() const override;
        
        FrameBufferInterface* create_frame_buffer(const FrameBufferDesc& desc) override;
        GraphicsPipelineInterface* create_graphics_pipeline(const GraphicsPipelineDesc& desc, FrameBufferInterface* frame_buffer) override;
        ComputePipelineInterface* create_compute_pipeline(const ComputePipelineDesc& desc) override;
        
        BindingLayoutInterface* create_binding_layout(const BindingLayoutDesc& desc) override;
        BindingLayoutInterface* create_bindless_layout(const BindlessLayoutDesc& desc) override;
        
        BindingSetInterface* create_binding_set(const BindingSetDesc& desc, BindingLayoutInterface* pLayout) override;
        BindlessSetInterface* create_bindless_set(BindingLayoutInterface* pLayout) override;
        
        CommandListInterface* create_command_list(const CommandListDesc& desc) override;
        uint64_t execute_command_lists(CommandListInterface* const* cmdlists, uint64_t cmd_count = 1, CommandQueueType queue_type = CommandQueueType::Graphics) override;
        bool queue_wait_for_cmdlist(CommandQueueType WaitQueueType, CommandQueueType queue_type, uint64_t stInstance) override;

		ray_tracing::PipelineInterface* create_ray_tracing_pipline(const ray_tracing::PipelineDesc& desc) override;
		ray_tracing::AccelStructInterface* create_accel_struct(const ray_tracing::AccelStructDesc& desc) override;
        
        void wait_for_idle() override;
        void collect_garbage() override;

        DX12RootSignature* build_root_signature(const BindingLayoutInterfaceArray& binding_layouts, bool allow_input_layout) const;
        
    private:
        DX12CommandQueue* GetQueue(CommandQueueType type) const;

    private:
        DX12Context _context;
        std::unique_ptr<DX12DescriptorHeaps> _descriptor_heaps;

        DX12DeviceDesc _desc;

        std::array<std::unique_ptr<DX12CommandQueue>, static_cast<uint8_t>(CommandQueueType::Count)> _cmd_queues;
        HANDLE _fence_event{};

        std::mutex _mutex;

        std::vector<ID3D12CommandList*> _d3d12_cmdlists_to_execute;
    };

}


#endif