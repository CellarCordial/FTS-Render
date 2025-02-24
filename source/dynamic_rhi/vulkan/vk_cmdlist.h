#ifndef DYNAMIC_RHI_VULKAN_CMDLIST_H
#define DYNAMIC_RHI_VULKAN_CMDLIST_H

#include "vk_forward.h"
#include "../command_list.h"
#include "../state_track.h"
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>

namespace fantasy 
{
    struct VKCommandBuffer
    {
        vk::CommandBuffer vk_cmdbuffer;
        vk::CommandPool vk_cmdpool; 
        
        uint64_t submit_id = INVALID_SIZE_64;
        uint64_t recording_id = INVALID_SIZE_32;
        
        const VKContext* context;

        VKCommandBuffer(const VKContext* context, uint64_t recording_id);
        ~VKCommandBuffer();

        bool initialize(uint32_t queue_family_index);
    };

    class VKCommandQueue
    {
    public:
        VKCommandQueue(const VKContext* context, CommandQueueType queue_type, vk::Queue vk_queue, uint32_t queue_family_index);
        ~VKCommandQueue();

        std::shared_ptr<VKCommandBuffer> get_command_buffer();
        
        void add_wait_semaphore(vk::Semaphore semaphore, uint64_t value);
        void add_signal_semaphore(vk::Semaphore semaphore, uint64_t value);
        
        uint64_t submit(CommandListInterface* const* cmdlists, uint32_t cmd_count);
        
        uint64_t get_last_finished_id();
        void retire_command_buffers();
        
        std::shared_ptr<VKCommandBuffer> get_command_buffer_in_flight(uint64_t submit_id);
        
        bool wait_command_list(uint64_t cmdlist_id, uint64_t timeout);
        
    public:
        CommandQueueType queue_type;
        
        vk::Queue vk_queue;
        uint32_t queue_family_index = INVALID_SIZE_32;
        
        uint64_t last_submitted_id = 0;
        uint64_t last_recording_id;
        vk::Semaphore vk_tracking_semaphore;
        
    private:
        const VKContext* _context;
        
        std::mutex _mutex;

        std::vector<vk::Semaphore> _vk_wait_semaphores;
        std::vector<uint64_t> _wait_semaphore_values;
        std::vector<vk::Semaphore> _vk_signal_semaphores;
        std::vector<uint64_t> _signal_semaphore_values;

        std::list<std::shared_ptr<VKCommandBuffer>> _cmdbuffers_pool;
        std::list<std::shared_ptr<VKCommandBuffer>> _cmdbuffers_in_flight;
    };

    struct VKVolatileBufferVersion
    {
        int32_t latest_version = 0;
        int32_t min_version = 0;
        int32_t max_version = 0;
        bool initialized = false;
    };
    
    struct VKBufferChunk
    {
        std::shared_ptr<BufferInterface> buffer;
        uint64_t buffer_size = 0;
        uint64_t version = 0;
        uint64_t write_pointer = 0;
        void* mapped_memory = nullptr;

        static constexpr uint64_t align_size = 4096; // GPU page size

        VKBufferChunk(DeviceInterface* device, const BufferDesc& desc, bool map_memory);
        ~VKBufferChunk();
    };

    class VKUploadManager
    {
    public:
        VKUploadManager(DeviceInterface* device, uint64_t default_chunk_size, uint64_t max_memory, bool is_scratch_buffer);
        
        bool suballocate_buffer(
            uint64_t size, 
            BufferInterface** out_buffer, 
            uint64_t* out_offset, 
            void** out_cpu_address, 
            uint64_t current_version, 
            uint32_t alignment = 256
        );

        void submit_chunks(uint64_t current_version, uint64_t submitted_version);
        
        std::shared_ptr<VKBufferChunk> create_chunk(uint64_t size);

    private:
        DeviceInterface* _device;
        
        bool _is_scratch_buffer = false;
        uint64_t _scratch_max_memory = 0;
        
        uint64_t _allocated_memory = 0;
        uint64_t _default_chunk_size = 0;

        std::shared_ptr<VKBufferChunk> _current_chunk;
        std::list<std::shared_ptr<VKBufferChunk>> _chunk_pool;
    };


    class VKCommandList : public CommandListInterface
    {
    public:
        VKCommandList(const VKContext* context, DeviceInterface* device, const CommandListDesc& desc);
        ~VKCommandList() override = default;

        bool initialize();
        
        bool open() override;
        bool close() override;

        void clear_texture_float(
            TextureInterface* texture, 
            const TextureSubresourceSet& subresource, 
            const Color& clear_color
        ) override;
        
        void clear_texture_uint(
            TextureInterface* texture,
            const TextureSubresourceSet& subresource,
            uint32_t clear_color
        ) override;
        
        void clear_render_target_texture(
            TextureInterface* texture, 
            const TextureSubresourceSet& subresource_set, 
            const Color& clear_color
        ) override;

        void clear_depth_stencil_texture(
            TextureInterface* texture,
            const TextureSubresourceSet& subresource,
            bool clear_depth,
            float depth,
            bool clear_stencil,
            uint8_t stencil
        ) override;

        void copy_texture(
            TextureInterface* dst,
            const TextureSlice& dst_slice,
            TextureInterface* src,
            const TextureSlice& src_slice
        ) override;
        
        void copy_texture(
            StagingTextureInterface* dst,
            const TextureSlice& dst_slice,
            TextureInterface* src,
            const TextureSlice& src_slice
        ) override;
        
        void copy_texture(
            TextureInterface* dst,
            const TextureSlice& dst_slice,
            StagingTextureInterface* src,
            const TextureSlice& src_slice
        ) override;
        
        bool write_texture(
            TextureInterface* dst,
            uint32_t array_slice,
            uint32_t mip_level,
            const uint8_t* data,
            uint64_t data_size
        ) override;

        bool write_buffer(
            BufferInterface* buffer, 
            const void* data, 
            uint64_t data_size, 
            uint64_t dst_byte_offset = 0
        ) override;

        void clear_buffer_uint(
            BufferInterface* buffer, 
            BufferRange range, 
            uint32_t clear_value
        ) override;
        
        void copy_buffer(
            BufferInterface* dst,
            uint64_t dst_byte_offset,
            BufferInterface* src,
            uint64_t src_byte_offset,
            uint64_t data_byte_size
        ) override;

        bool draw(
            const GraphicsState& state, 
            const DrawArguments& arguments, 
            const void* push_constant = nullptr
        ) override;

        bool draw_indexed(
            const GraphicsState& state, 
            const DrawArguments& arguments, 
            const void* push_constant = nullptr
        ) override;

        bool dispatch(
            const ComputeState& state,
            uint32_t thread_group_num_x, 
            uint32_t thread_group_num_y = 1, 
            uint32_t thread_group_num_z = 1,
            const void* push_constant = nullptr
        ) override;

        bool draw_indirect(
            const GraphicsState& state, 
            uint32_t offset_bytes = 0, 
            uint32_t draw_count = 1, 
            const void* push_constant = nullptr
        ) override;

        bool draw_indexed_indirect(
            const GraphicsState& state, 
            uint32_t offset_bytes = 0, 
            uint32_t draw_count = 1,
            const void* push_constant = nullptr
        ) override;
        
        bool dispatch_indirect(
            const ComputeState& state, 
            uint32_t offset_bytes = 0,
            const void* push_constant = nullptr
        ) override;
        
        void set_enable_uav_barrier_for_texture(TextureInterface* texture, bool enable_barriers) override;
        void set_enable_uav_barrier_for_buffer(BufferInterface* buffer, bool enable_barriers) override;
        void set_texture_state(TextureInterface* texture, const TextureSubresourceSet& subresource_set, ResourceStates states) override;
        void set_buffer_state(BufferInterface* buffer, ResourceStates states) override;

        void commit_barriers() override;

        ResourceStates get_buffer_state(BufferInterface* buffer) override;
        ResourceStates get_texture_state(TextureInterface* texture, uint32_t array_slice, uint32_t mip_level) override;

        const CommandListDesc& get_desc() override;
        DeviceInterface* get_deivce() override;
        void* get_native_object() override;


        void executed(VKCommandQueue& queue, uint64_t submit_id);
        std::shared_ptr<VKCommandBuffer> get_current_command_buffer() const;

    private:
        void clear_Texture(TextureInterface* texture, TextureSubresourceSet subresource, const vk::ClearColorValue& clear_color);
        
        bool set_graphics_state(const GraphicsState& state);
        bool set_compute_state(const ComputeState& state);
        
        void set_binding_resource_state(BindingSetInterface* binding_set);
        void bind_binding_sets(
            const PipelineStateBindingSetArray& binding_sets,
            vk::PipelineBindPoint vk_bind_point, 
            vk::PipelineLayout vk_layout
        );

    private:
        const VKContext* _context;
        DeviceInterface* _device;

        inline static ResourceStateTracker _resource_state_tracker;
        
        CommandListDesc _desc;
        std::shared_ptr<VKCommandBuffer> _current_cmdbuffer;

        VKUploadManager _upload_manager;
        VKUploadManager _scratch_manager;

        GraphicsState _current_graphics_state;
        ComputeState _current_compute_state;
    };
}









#endif