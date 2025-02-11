#ifndef RHI_DEVICE_H
#define RHI_DEVICE_H

#include "binding.h"
#include "pipeline.h"
#include "resource.h"
#include "command_list.h"
#include "frame_buffer.h"
#include <memory>

namespace fantasy
{
    struct DeviceInterface
    {
        virtual HeapInterface* create_heap(const HeapDesc& desc) = 0;
        
        virtual BufferInterface* create_buffer(const BufferDesc& desc) = 0;
        virtual TextureInterface* create_texture(const TextureDesc& desc) = 0;
        virtual BufferInterface* create_buffer_from_native(void* native_buffer, const BufferDesc& desc) = 0;
        virtual TextureInterface* create_texture_from_native(void* native_texture, const TextureDesc& desc) = 0;
        
        virtual StagingTextureInterface* create_staging_texture(const TextureDesc& desc, CpuAccessMode cpu_access) = 0;

        virtual SamplerInterface* create_sampler(const SamplerDesc& desc) = 0;

        virtual InputLayoutInterface* create_input_layout(const VertexAttributeDesc* cpDesc, uint32_t attribute_count) = 0;
        
        virtual FrameBufferInterface* create_frame_buffer(const FrameBufferDesc& desc) = 0;
        
        virtual GraphicsPipelineInterface* create_graphics_pipeline(const GraphicsPipelineDesc& desc, FrameBufferInterface* frame_buffer) = 0;
        virtual ComputePipelineInterface* create_compute_pipeline(const ComputePipelineDesc& desc) = 0;

        virtual BindingLayoutInterface* create_binding_layout(const BindingLayoutDesc& desc) = 0;
        virtual BindingLayoutInterface* create_bindless_layout(const BindlessLayoutDesc& desc) = 0;
        virtual BindlessSetInterface* create_bindless_set(std::shared_ptr<BindingLayoutInterface> binding_layout) = 0;
        virtual BindingSetInterface* create_binding_set(const BindingSetDesc& desc, std::shared_ptr<BindingLayoutInterface> binding_layout) = 0;

        virtual CommandListInterface* create_command_list(const CommandListDesc& desc) = 0;
        virtual uint64_t execute_command_lists(
            CommandListInterface* const* cmdlists,
            uint64_t cmd_count = 1,
            CommandQueueType queue_type = CommandQueueType::Graphics
        ) = 0;

        virtual void queue_wait_for_cmdlist(CommandQueueType wait_queue_type, CommandQueueType execution_queue_type, uint64_t submit_id) = 0;

        virtual void wait_for_idle() = 0;
        virtual void collect_garbage() = 0;

        virtual GraphicsAPI get_graphics_api() const = 0;
        virtual void* get_native_object() const = 0;


		virtual ~DeviceInterface() = default;
    };
}






























#endif