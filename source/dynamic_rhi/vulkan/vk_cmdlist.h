#ifndef DYNAMIC_RHI_VULKAN_CMDLIST_H
#define DYNAMIC_RHI_VULKAN_CMDLIST_H

#include "vk_resource.h"
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
        const VKContext* context;
        vk::CommandBuffer cmd_buffer = vk::CommandBuffer();
        vk::CommandPool cmd_pool = vk::CommandPool();
        
        std::vector<std::shared_ptr<ResourceInterface>> ref_resources;
        std::vector<std::shared_ptr<BufferInterface>> ref_staging_buffers;

        uint64_t recording_id = 0;
        uint64_t submission_id = 0;

        explicit VKCommandBuffer(const VKContext* context_)
            : context(context_)
        {
        }

        ~VKCommandBuffer()
        {
            context->device.destroyCommandPool(cmd_pool, context->allocation_callbacks);
        }
    };

    class VKCommandQueue
    {
    public:
        VKCommandQueue(
            const VKContext* context, 
            CommandQueueType queue_type, 
            vk::Queue vulkan_queue, 
            uint32_t queue_family_index
        );
        ~VKCommandQueue();

        std::shared_ptr<VKCommandBuffer> create_command_buffer();
        std::shared_ptr<VKCommandBuffer> get_command_buffer();

        void add_wait_semaphore(vk::Semaphore semaphore, uint64_t value);
        void add_signal_semaphore(vk::Semaphore semaphore, uint64_t value);

        uint64_t submit(CommandListInterface* const* cmdlist, uint64_t cmd_count);

        void update_texture_tile_mappings(TextureInterface* texture, const TextureTilesMapping* tile_mappings, uint32_t tile_mappings_count);

        void retire_command_buffers();

        std::shared_ptr<VKCommandBuffer> get_command_buffer_in_flight(uint64_t submission_id);

        uint64_t update_last_finished_id();

        bool poll_command_list(uint64_t cmd_list_id);
        bool wait_command_list(uint64_t cmd_list_id, uint64_t timeout);

    public:
        vk::Semaphore tracking_semaphore;
        
        vk::Queue _vulkan_queue;
        CommandQueueType _queue_type;
        
        uint64_t _last_submitted_id = 0;
        uint64_t _last_finished_id = 0;

    private:
        const VKContext* _context;

        uint32_t _queue_family_index = INVALID_SIZE_32;

        std::mutex _mutex;
        std::vector<vk::Semaphore> _wait_semaphores;
        std::vector<uint64_t> _wait_semaphore_values;
        std::vector<vk::Semaphore> _signal_semaphores;
        std::vector<uint64_t> _signal_semaphore_values;

        uint64_t _last_recording_id = 0;

        std::list<std::shared_ptr<VKCommandBuffer>> _cmd_buffers_pool;
        std::list<std::shared_ptr<VKCommandBuffer>> _cmd_buffers_in_flight;
    };

    
    
    struct VKBufferChunk
    {
        std::shared_ptr<BufferInterface> buffer;
        uint64_t version = 0;
        uint64_t buffer_size = 0;
        uint64_t write_pointer = 0;
        void* mapped_memory = nullptr;

        static constexpr uint64_t size_alignment = 4096; // GPU page size
    };

    class UploadManager
    {
    public:
        UploadManager(DeviceInterface* device, uint64_t defaultChunkSize, uint64_t memoryLimit, bool isScratchBuffer) :
            _device(device), 
            _default_chunk_size(defaultChunkSize), 
            _memory_limit(memoryLimit), 
            _is_scratch_buffer(isScratchBuffer)
        {
        }

        std::shared_ptr<VKBufferChunk> create_chunk(uint64_t size);

        bool suballocate_buffer(
            uint64_t size, 
            BufferInterface* buffer, 
            uint64_t* offset, 
            void** cpu_address, 
            uint64_t current_version, 
            uint32_t alignment = 256
        );
        void submit_chunks(uint64_t current_version, uint64_t submitted_version);

    private:
        DeviceInterface* _device;
        uint64_t _default_chunk_size = 0;
        uint64_t _memory_limit = 0;
        uint64_t _allocated_memory = 0;
        bool _is_scratch_buffer = false;

        std::list<std::shared_ptr<VKBufferChunk>> _chunk_pool;
        std::shared_ptr<VKBufferChunk> _current_chunk;
    };


    class VKCommandList : public CommandListInterface
    {
    public:
        VKCommandList(DeviceInterface* device, const VKContext* context, const CommandListDesc& desc);
        ~VKCommandList() override;

        
        bool open() override;
        bool close() override;

        bool clear_state() override;

        bool clear_texture_float(
            TextureInterface* texture, 
            const TextureSubresourceSet& subresource_set, 
            const Color& clear_color
        ) override;
        
        bool clear_texture_uint(
            TextureInterface* texture,
            const TextureSubresourceSet& subresource_set,
            uint32_t dwClearColor
        ) override;
        
        bool clear_depth_stencil_texture(
            TextureInterface* texture,
            const TextureSubresourceSet& subresource_set,
            bool clear_depth,
            float depth,
            bool clear_stencil,
            uint8_t stencil
        ) override;

        bool copy_texture(
            TextureInterface* dst,
            const TextureSlice& dst_slice,
            TextureInterface* src,
            const TextureSlice& src_slice
        ) override;
        
        bool copy_texture(
            StagingTextureInterface* dst,
            const TextureSlice& dst_slice,
            TextureInterface* src,
            const TextureSlice& src_slice
        ) override;
        
        bool copy_texture(
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
            uint64_t row_pitch,
            uint64_t depth_pitch = 0
        ) override;
        
        bool resolve_texture(
            TextureInterface* dst,
            const TextureSubresourceSet& dst_subresource,
            TextureInterface* src,
            const TextureSubresourceSet& src_subresource
        ) override;

        bool write_buffer(
            BufferInterface* buffer, 
            const void* data, 
            uint64_t data_size, 
            uint64_t dst_byte_offset = 0
        ) override;
        
        bool clear_buffer_uint(BufferInterface* buffer, uint32_t clear_value) override;
        
        bool copy_buffer(
            BufferInterface* dst,
            uint64_t dst_byte_offset,
            BufferInterface* src,
            uint64_t src_byte_offset,
            uint64_t data_byte_size
        ) override;
        
		bool build_bottom_level_accel_struct(
            ray_tracing::AccelStructInterface* accel_struct,
            const ray_tracing::GeometryDesc* geometry_descs,
            uint32_t geometry_desc_count
        ) override;
		bool build_top_level_accel_struct(
            ray_tracing::AccelStructInterface* accel_struct, 
            const ray_tracing::InstanceDesc* instance_descs, 
            uint32_t instance_count
        ) override;

        bool set_push_constants(const void* data, uint64_t byte_size) override;
		bool set_accel_struct_state(ray_tracing::AccelStructInterface* accel_struct, ResourceStates state) override;
        bool set_graphics_state(const GraphicsState& state) override;
        bool set_compute_state(const ComputeState& state) override;
		bool set_ray_tracing_state(const ray_tracing::PipelineState& state) override;
        
        bool draw(const DrawArguments& arguments) override;
        bool draw_indexed(const DrawArguments& arguments) override;
        bool dispatch(
            uint32_t thread_group_num_x, 
            uint32_t thread_group_num_y = 1, 
            uint32_t thread_group_num_z = 1
        ) override;

        bool draw_indirect(uint32_t offset_bytes = 0, uint32_t draw_count = 1) override;
        bool draw_indexed_indirect(uint32_t offset_bytes = 0, uint32_t draw_count = 1) override;
        bool dispatch_indirect(uint32_t offset_bytes = 0) override;
		
        bool dispatch_rays(const ray_tracing::DispatchRaysArguments& arguments) override;
        
        bool begin_timer_query(TimerQueryInterface* query) override;
        bool end_timer_query(TimerQueryInterface* query) override;

        bool begin_marker(const char* cpcName) override;
        bool end_marker() override;

        void set_enable_uav_barrier_for_texture(TextureInterface* texture, bool enable_barriers) override;
        void set_enable_uav_barrier_for_buffer(BufferInterface* buffer, bool enable_barriers) override;
        bool set_texture_state(TextureInterface* texture, const TextureSubresourceSet& subresource_set, ResourceStates states) override;
        bool set_buffer_state(BufferInterface* buffer, ResourceStates states) override;

        void commit_barriers() override;

		bool bind_frame_buffer(FrameBufferInterface* frame_buffer) override;
		bool commit_descriptor_heaps() override;

        ResourceStates get_buffer_state(BufferInterface* buffer) override;
        ResourceStates get_texture_subresource_state(
            TextureInterface* texture,
            uint32_t array_slice,
            uint32_t mip_level
        ) override;

        DeviceInterface* get_deivce() override;
        CommandListDesc get_desc() override;
        void* get_native_object() override;


        void executed(VKCommandQueue& queue, uint64_t submissionID);
        std::shared_ptr<VKCommandBuffer> get_current_command_buffer() const;

    private:
        void clear_Texture(TextureInterface* texture, TextureSubresourceSet subresources, const vk::ClearColorValue& clearValue);

        void bind_binding_sets(
            vk::PipelineBindPoint bindPoint, 
            vk::PipelineLayout pipeline_layout, 
            const StackArray<BindingSetInterface*, MAX_BINDING_LAYOUTS>& bindings, 
            const StackArray<uint32_t, MAX_BINDING_LAYOUTS>& descriptor_set_index_to_binding_index
        );

        void end_render_pass();

        void track_resources(const GraphicsState& state);
        
        void write_volatile_buffer(BufferInterface* buffer, const void* data, size_t data_size);
        void flush_volatile_buffer_writes();
        void submit_volatile_buffers(uint64_t recording_id, uint64_t submitted_id);

        void update_graphics_volatile_buffers();
        void update_compute_volatile_buffers();
        void update_ray_tracing_volatile_buffers();

        void require_texture_state(TextureInterface* texture, TextureSubresourceSet subresources, ResourceStates state);
        void require_buffer_state(BufferInterface* buffer, ResourceStates state);
        bool any_barriers() const;

        void build_top_level_accel_struct_internal(
            ray_tracing::AccelStructInterface* accel_struct, 
            VkDeviceAddress instance_data, 
            size_t instance_count, 
            ray_tracing::AccelStructBuildFlags build_flags, 
            uint64_t current_version
        );

        void commit_barriers_internal();
        void commit_barriers_internal_synchronization2();

    private:
        DeviceInterface* m_Device;
        const VKContext* m_Context;

        CommandListDesc m_CommandListParameters;

        ResourceStateTracker m_StateTracker;
        bool m_EnableAutomaticBarriers = true;

        std::shared_ptr<VKCommandBuffer> m_CurrentCmdBuf = nullptr;

        vk::PipelineLayout m_CurrentPipelineLayout;
        vk::ShaderStageFlags m_CurrentPushConstantsVisibility;
        GraphicsState m_CurrentGraphicsState{};
        ComputeState m_CurrentComputeState{};
        ray_tracing::PipelineState m_CurrentRayTracingState;
        bool m_AnyVolatileBufferWrites = false;

        struct ShaderTableState
        {
            vk::StridedDeviceAddressRegionKHR rayGen;
            vk::StridedDeviceAddressRegionKHR miss;
            vk::StridedDeviceAddressRegionKHR hitGroups;
            vk::StridedDeviceAddressRegionKHR callable;
            uint32_t version = 0;
        } m_CurrentShaderTablePointers;

        std::unordered_map<BufferInterface*, VKVolatileBufferState> m_VolatileBufferStates;

        std::unique_ptr<UploadManager> m_UploadManager;
        std::unique_ptr<UploadManager> m_ScratchManager;
    };
}









#endif