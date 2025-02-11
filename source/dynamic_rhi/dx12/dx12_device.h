#ifndef RHI_DX12_DEVICE_H
#define RHI_DX12_DEVICE_H


#include "../dynamic_rhi.h"
#include "dx12_cmdlist.h"
#include <d3d12.h>
#include <mutex>

namespace fantasy 
{
    class DX12Device : public DeviceInterface
    {
    public:
        explicit DX12Device(const DX12DeviceDesc& desc);
        ~DX12Device() noexcept;

        bool initialize();

        HeapInterface* create_heap(const HeapDesc& desc) override;
        TextureInterface* create_texture(const TextureDesc& desc) override;
        StagingTextureInterface* create_staging_texture(const TextureDesc& desc, CpuAccessMode cpu_access) override;
        BufferInterface* create_buffer(const BufferDesc& desc) override;

        TextureInterface* create_texture_from_native(void* native_texture, const TextureDesc& desc) override;
        BufferInterface* create_buffer_from_native(void* native_buffer, const BufferDesc& desc) override;

        SamplerInterface* create_sampler(const SamplerDesc& desc) override;
        InputLayoutInterface* create_input_layout(const VertexAttributeDesc* cpDesc, uint32_t attribute_count) override;
        
        GraphicsAPI get_graphics_api() const override;
        void* get_native_object() const override;
        
        FrameBufferInterface* create_frame_buffer(const FrameBufferDesc& desc) override;
        GraphicsPipelineInterface* create_graphics_pipeline(const GraphicsPipelineDesc& desc, FrameBufferInterface* frame_buffer) override;
        ComputePipelineInterface* create_compute_pipeline(const ComputePipelineDesc& desc) override;
        
        BindingLayoutInterface* create_binding_layout(const BindingLayoutDesc& desc) override;
        BindingLayoutInterface* create_bindless_layout(const BindlessLayoutDesc& desc) override;
        
        BindingSetInterface* create_binding_set(const BindingSetDesc& desc, std::shared_ptr<BindingLayoutInterface> binding_layout) override;
        BindlessSetInterface* create_bindless_set(std::shared_ptr<BindingLayoutInterface> binding_layout) override;
        
        CommandListInterface* create_command_list(const CommandListDesc& desc) override;

        uint64_t execute_command_lists(CommandListInterface* const* cmdlists, uint64_t cmd_count = 1, CommandQueueType queue_type = CommandQueueType::Graphics) override;
        void queue_wait_for_cmdlist(CommandQueueType wait_queue_type, CommandQueueType execution_queue_type, uint64_t submit_id) override;

        void wait_for_idle() override;
        void collect_garbage() override;

        uint64_t queue_get_completed_id(CommandQueueType type);
        DX12CommandQueue* get_queue(CommandQueueType type) const;

    public:
        DX12DeviceDesc desc;
        
        DX12Context context;
        DX12DescriptorManager descriptor_manager;
        
        std::array<std::unique_ptr<DX12CommandQueue>, static_cast<uint8_t>(CommandQueueType::Count)> cmdqueues;

    private:
        std::mutex _mutex;
        std::vector<ID3D12CommandList*> _d3d12_cmdlists_to_execute;
    };

}


#endif