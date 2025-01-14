#ifndef RHI_DRAW_H
#define RHI_DRAW_H

#include "forward.h"
#include "pipeline.h"
#include "resource.h"
#include <memory>

namespace fantasy
{
    struct TimerQueryInterface 
    {
        virtual ~TimerQueryInterface() = default;
    };

    struct EventQueryInterface 
    {
        virtual ~EventQueryInterface() = default;
    };

    struct VertexBufferBinding
    {
        std::shared_ptr<BufferInterface> buffer;
        uint32_t slot = 0;
        uint64_t offset = 0;

        bool operator==(const VertexBufferBinding& binding) const
        {
            return  buffer == binding.buffer &&
                    offset == binding.offset &&
                    slot == binding.slot;
        }

        bool operator!=(const VertexBufferBinding& binding) const
        {
            return !((*this) == binding);
        }

    };

    using VertexBufferBindingArray = StackArray<VertexBufferBinding, MAX_VERTEX_ATTRIBUTES>;

    struct IndexBufferBinding
    {
        std::shared_ptr<BufferInterface> buffer;
        Format format = Format::R32_UINT;
        uint32_t offset = 0;

        bool operator==(const IndexBufferBinding& binding) const
        {
            return  buffer == binding.buffer &&
                    offset == binding.offset &&
                    format == binding.format;
        }

        bool operator!=(const IndexBufferBinding& binding) const
        {
            return !((*this) == binding);
        }

        bool is_valid() const { return buffer != nullptr; }
    };


    struct DrawArguments
    {
        uint32_t index_count = 0;    // 当使用 draw() 时为 VertexCount, 使用 draw_indexed() 时为 IndexCount.
        uint32_t instance_count = 1;
        uint32_t start_index_location = 0;
        uint32_t start_vertex_location = 0;
        uint32_t start_instance_location = 0;


        static DrawArguments full_screen_quad()
        {
            DrawArguments ret{};
            ret.index_count = 6;
            return ret;
        } 
    };

    struct DrawIndirectArguments
    {
        uint32_t vertex_count = 0;
        uint32_t instance_count = 1;
        uint32_t start_vertex_location = 0;
        uint32_t start_instance_location = 0;
    };

    struct DrawIndexedIndirectArguments
    {
        uint32_t index_count = 0;
        uint32_t instance_count = 1;
        uint32_t start_index_location = 0;
        uint32_t start_vertex_location = 0;
        uint32_t start_instance_location = 0;
    };
    
    using PipelineStateBindingSetArray = StackArray<BindingSetInterface*, MAX_BINDING_LAYOUTS>;

    struct GraphicsState
    {
        GraphicsPipelineInterface* pipeline = nullptr;
        PipelineStateBindingSetArray binding_sets;
        Color blend_constant_color;
        
        FrameBufferInterface* frame_buffer = nullptr;
        ViewportState viewport_state;
        
        VertexBufferBindingArray vertex_buffer_bindings;
        IndexBufferBinding index_buffer_binding;

        uint8_t dynamic_stencil_ref_value = 0;

        BufferInterface* indirect_buffer;
    };

    struct ComputeState
    {
        ComputePipelineInterface* pipeline = nullptr;
        PipelineStateBindingSetArray binding_sets;

        BufferInterface* indirect_buffer;
    };

    namespace ray_tracing 
    {
        struct PipelineState
        {
            ShaderTableInterface* shader_table = nullptr;
            PipelineStateBindingSetArray binding_sets;
        };

        struct DispatchRaysArguments
        {
            uint32_t width = 1;	
            uint32_t height = 1;
            uint32_t depth = 1;
        };
    }
}






































#endif