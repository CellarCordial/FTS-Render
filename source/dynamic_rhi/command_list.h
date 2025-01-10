#ifndef RHI_COMMAND_LIST_H
#define RHI_COMMAND_LIST_H

#include "draw.h"
namespace fantasy
{
    enum class CommandQueueType : uint8_t
    {
        Graphics = 0,
        Compute,
        Copy,

        Count
    };

    struct CommandListDesc
    {
        uint64_t upload_chunk_size = 64 * 1024;
        CommandQueueType queue_type = CommandQueueType::Graphics;

        uint64_t scratch_chunk_size = 64 * 1024;
        uint64_t scratch_max_mamory = 1024 * 1024 * 1024;
    };

    struct CommandListInterface
    {
    public:
        virtual bool open() = 0;
        virtual bool close() = 0;

        virtual bool clear_state() = 0;

        virtual bool clear_texture_float(
            TextureInterface* texture, 
            const TextureSubresourceSet& subresource_set, 
            const Color& clear_color
        ) = 0;
        
        virtual bool clear_texture_uint(
            TextureInterface* texture,
            const TextureSubresourceSet& subresource_set,
            uint32_t dwClearColor
        ) = 0;
        
        virtual bool clear_depth_stencil_texture(
            TextureInterface* texture,
            const TextureSubresourceSet& subresource_set,
            bool clear_depth,
            float depth,
            bool clear_stencil,
            uint8_t stencil
        ) = 0;

        virtual bool copy_texture(
            TextureInterface* dst,
            const TextureSlice& dst_slice,
            TextureInterface* src,
            const TextureSlice& src_slice
        ) = 0;
        
        virtual bool copy_texture(
            StagingTextureInterface* dst,
            const TextureSlice& dst_slice,
            TextureInterface* src,
            const TextureSlice& src_slice
        ) = 0;
        
        virtual bool copy_texture(
            TextureInterface* dst,
            const TextureSlice& dst_slice,
            StagingTextureInterface* src,
            const TextureSlice& src_slice
        ) = 0;
        
        virtual bool write_texture(
            TextureInterface* dst,
            uint32_t array_slice,
            uint32_t mip_level,
            const uint8_t* data,
            uint64_t row_pitch,
            uint64_t depth_pitch = 0
        ) = 0;
        
        virtual bool resolve_texture(
            TextureInterface* dst,
            const TextureSubresourceSet& dst_subresource,
            TextureInterface* src,
            const TextureSubresourceSet& src_subresource
        ) = 0;

        virtual bool write_buffer(
            BufferInterface* buffer, 
            const void* data, 
            uint64_t data_size, 
            uint64_t dst_byte_offset = 0
        ) = 0;
        
        virtual bool clear_buffer_uint(BufferInterface* buffer, uint32_t clear_value) = 0;
        
        virtual bool copy_buffer(
            BufferInterface* dst,
            uint64_t dst_byte_offset,
            BufferInterface* src,
            uint64_t src_byte_offset,
            uint64_t data_byte_size
        ) = 0;
        
		virtual bool build_bottom_level_accel_struct(
            ray_tracing::AccelStructInterface* accel_struct,
            const ray_tracing::GeometryDesc* geometry_descs,
            uint32_t geometry_desc_count
        ) = 0;
		virtual bool build_top_level_accel_struct(
            ray_tracing::AccelStructInterface* accel_struct, 
            const ray_tracing::InstanceDesc* instance_descs, 
            uint32_t instance_count
        ) = 0;

        virtual bool set_push_constants(const void* data, uint64_t byte_size) = 0;
		virtual bool set_accel_struct_state(ray_tracing::AccelStructInterface* accel_struct, ResourceStates state) = 0;
        virtual bool set_graphics_state(const GraphicsState& state) = 0;
        virtual bool set_compute_state(const ComputeState& state) = 0;
		virtual bool set_ray_tracing_state(const ray_tracing::PipelineState& state) = 0;
        
        virtual bool draw(const DrawArguments& arguments) = 0;
        virtual bool draw_indexed(const DrawArguments& arguments) = 0;
        virtual bool dispatch(
            uint32_t thread_group_num_x, 
            uint32_t thread_group_num_y = 1, 
            uint32_t thread_group_num_z = 1
        ) = 0;
		virtual bool dispatch_rays(const ray_tracing::DispatchRaysArguments& arguments) = 0;
        
        virtual bool begin_timer_query(TimerQueryInterface* query) = 0;
        virtual bool end_timer_query(TimerQueryInterface* query) = 0;

        virtual bool begin_marker(const char* cpcName) = 0;
        virtual bool end_marker() = 0;

        virtual void set_enable_uav_barrier_for_texture(TextureInterface* texture, bool enable_barriers) = 0;
        virtual void set_enable_uav_barrier_for_buffer(BufferInterface* buffer, bool enable_barriers) = 0;
        virtual bool set_texture_state(TextureInterface* texture, const TextureSubresourceSet& subresource_set, ResourceStates states) = 0;
        virtual bool set_buffer_state(BufferInterface* buffer, ResourceStates states) = 0;

        virtual void commit_barriers() = 0;

		virtual bool bind_frame_buffer(FrameBufferInterface* frame_buffer) = 0;
		virtual bool commit_descriptor_heaps() = 0;

        virtual ResourceStates get_buffer_state(BufferInterface* buffer) = 0;
        virtual ResourceStates get_texture_subresource_state(
            TextureInterface* texture,
            uint32_t array_slice,
            uint32_t mip_level
        ) = 0;

        virtual DeviceInterface* get_deivce() = 0;
        virtual CommandListDesc get_desc() = 0;
        virtual void* get_native_object() = 0;

        virtual ~CommandListInterface() = default;
    };


    
    inline bool clear_color_attachment(CommandListInterface* cmdlist, FrameBufferInterface* frame_buffer, uint32_t attachment_index)
    {
        const auto& attachment = frame_buffer->get_desc().color_attachments[attachment_index];
        if (attachment.is_valid() && cmdlist->clear_texture_float(attachment.texture.get(), attachment.subresource, attachment.texture->get_desc().clear_value))
        {
            return true;
        }
        return false;
    }

    inline bool clear_depth_stencil_attachment(CommandListInterface* cmdlist, FrameBufferInterface* frame_buffer)
    {
        const auto& attachment = frame_buffer->get_desc().depth_stencil_attachment;
        Color clear_value = attachment.texture->get_desc().clear_value;
        if (attachment.is_valid() && cmdlist->clear_depth_stencil_texture(attachment.texture.get(), attachment.subresource, true, clear_value.r, true, static_cast<uint8_t>(clear_value.g)))
        {
            return true;
        }
        return false;
    }
}


























#endif