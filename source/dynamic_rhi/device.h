#ifndef RHI_DEVICE_H
#define RHI_DEVICE_H

#include "descriptor.h"
#include "draw.h"
#include "pipeline.h"
#include "resource.h"
#include "shader.h"
#include "command_list.h"
#include "frame_buffer.h"

namespace fantasy
{
    struct DeviceInterface
    {
        virtual HeapInterface* create_heap(const HeapDesc& desc) = 0;
        
        virtual BufferInterface* create_buffer(const BufferDesc& desc) = 0;
        virtual TextureInterface* create_texture(const TextureDesc& desc) = 0;
        virtual BufferInterface* create_buffer_from_native(void* pNativeBuffer, const BufferDesc& desc) = 0;
        virtual TextureInterface* create_texture_from_native(void* pNativeTexture, const TextureDesc& desc) = 0;
        
        virtual StagingTextureInterface* create_staging_texture(const TextureDesc& desc, CpuAccessMode cpu_access) = 0;

        virtual SamplerInterface* create_sampler(const SamplerDesc& desc) = 0;

        virtual InputLayoutInterface* create_input_layout(const VertexAttributeDesc* cpDesc, uint32_t dwAttributeNum, Shader* pVertexShader) = 0;
        

        virtual EventQueryInterface* create_event_query() = 0;
        virtual bool set_event_query(EventQueryInterface* query, CommandQueueType queue_type) = 0;
        virtual bool poll_event_query(EventQueryInterface* query)= 0;
        virtual bool wait_event_query(EventQueryInterface* query) = 0;
        virtual bool reset_event_query(EventQueryInterface* query) = 0;

        virtual TimerQueryInterface* create_timer_query() = 0;
        virtual bool poll_timer_query(TimerQueryInterface* query) = 0;
        virtual bool reset_timer_query(TimerQueryInterface* query) = 0;
        virtual float get_timer_query_time(TimerQueryInterface* query) = 0;


        virtual void* get_native_descriptor_heap(DescriptorHeapType type) const = 0;

        virtual FrameBufferInterface* create_frame_buffer(const FrameBufferDesc& desc) = 0;
        
        virtual GraphicsPipelineInterface* create_graphics_pipeline(const GraphicsPipelineDesc& desc, FrameBufferInterface* frame_buffer) = 0;
        virtual ComputePipelineInterface* create_compute_pipeline(const ComputePipelineDesc& desc) = 0;

        virtual BindingLayoutInterface* create_binding_layout(const BindingLayoutDesc& desc) = 0;
        virtual BindingLayoutInterface* create_bindless_layout(const BindlessLayoutDesc& desc) = 0;
        virtual BindlessSetInterface* create_bindless_set(BindingLayoutInterface* pLayout) = 0;
        virtual BindingSetInterface* create_binding_set(const BindingSetDesc& desc, BindingLayoutInterface* pLayout) = 0;

        virtual CommandListInterface* create_command_list(const CommandListDesc& desc) = 0;
        virtual uint64_t execute_command_lists(
            CommandListInterface* const* cmdlists,
            uint64_t cmd_count = 1,
            CommandQueueType queue_type = CommandQueueType::Graphics
        ) = 0;

#if RAY_TRACING
		virtual ray_tracing::PipelineInterface* create_ray_tracing_pipline(const ray_tracing::PipelineDesc& desc) = 0;
		virtual ray_tracing::AccelStructInterface* create_accel_struct(const ray_tracing::AccelStructDesc& desc) = 0;
#endif
        
        virtual bool queue_wait_for_cmdlist(CommandQueueType WaitQueueType, CommandQueueType queue_type, uint64_t stInstance) = 0;

        virtual void wait_for_idle() = 0;
        virtual void collect_garbage() = 0;

        virtual GraphicsAPI get_graphics_api() const = 0;
        virtual void* get_native_object() const = 0;


		virtual ~DeviceInterface() = default;
    };
}






























#endif