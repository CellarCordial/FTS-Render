#ifndef RHI_DX12_COMMANDLIST_H
#define RHI_DX12_COMMANDLIST_H

#include "dx12_forward.h"
#include "dx12_descriptor.h"
#include "../command_list.h"
#include "../state_track.h"
#include "dx12_ray_tracing.h"
#include <combaseapi.h>
#include <d3d12.h>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>
#include <wrl/client.h>

namespace fantasy 
{
    struct DX12InternalCommandList
    {
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> d3d12_cmdlist;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> d3d12_cmdlist4;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> d3d12_cmdlist6;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> d3d12_cmd_allocator;
        
        uint64_t submit_id = INVALID_SIZE_64;
        uint64_t recording_id = INVALID_SIZE_64;

        std::vector<std::shared_ptr<BufferInterface>> ref_staging_buffers;

        const DX12Context* context;

        explicit DX12InternalCommandList(const DX12Context* context, uint64_t recording_id);

        bool initialize(CommandQueueType queue_type);
    };

    class DX12CommandQueue
    {
    public:
        DX12CommandQueue(
            const DX12Context* context, 
            CommandQueueType queue_type, 
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_cmdqueue
        );

        bool initialize();

        std::shared_ptr<DX12InternalCommandList> get_command_list();
        
        void add_wait_fence(Microsoft::WRL::ComPtr<ID3D12Fence> fence, uint64_t value);
        void add_signal_fence(Microsoft::WRL::ComPtr<ID3D12Fence> fence, uint64_t value);
        
        uint64_t execute(CommandListInterface* const* cmdlists, uint32_t cmd_count);
        
        uint64_t get_last_finished_id();
        void retire_command_lists();
        
        std::shared_ptr<DX12InternalCommandList> get_command_list_in_flight(uint64_t submit_id);
        
        bool wait_command_list(uint64_t cmdlist_id, uint64_t timeout);
        void wait_for_idle();

    public:
        CommandQueueType queue_type;
        
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_cmdqueue;

        uint64_t last_submitted_id = 0;
        uint32_t last_recording_id = 0;
        Microsoft::WRL::ComPtr<ID3D12Fence> d3d12_recording_fence;
        
    private:      
        const DX12Context* _context;

        std::mutex _mutex;

        std::vector<Microsoft::WRL::ComPtr<ID3D12Fence>> _d3d12_wait_fences;
        std::vector<uint64_t> _wait_fence_values;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Fence>> _d3d12_signal_fences;
        std::vector<uint64_t> _signal_fence_values;
        
        std::list<std::shared_ptr<DX12InternalCommandList>> _cmdlists_pool;
        std::list<std::shared_ptr<DX12InternalCommandList>> _cmdlists_in_flight;
    };

    struct DX12BufferChunk
    {
        std::shared_ptr<BufferInterface> buffer;
        uint64_t buffer_size = 0;
        uint64_t version = 0;
        uint64_t write_pointer = 0;
        void* mapped_memory = nullptr;

        static constexpr uint64_t align_size = 4096; // GPU page size

        DX12BufferChunk(DeviceInterface* device, const BufferDesc& desc, bool map_memory);
        ~DX12BufferChunk();
    };

    class DX12UploadManager
    {
    public:
        DX12UploadManager(DeviceInterface* device, uint64_t default_chunk_size, uint64_t max_memory, bool is_scratch_buffer);

        bool suballocate_buffer(
            uint64_t size, 
            BufferInterface** out_buffer, 
            uint64_t* out_offset, 
            void** out_cpu_address, 
            uint64_t current_version, 
            uint32_t alignment = 256,
            CommandListInterface* cmdlist = nullptr
        );

        void submit_chunks(uint64_t current_version, uint64_t submitted_version);

        std::shared_ptr<DX12BufferChunk> create_chunk(uint64_t size) const;

    private:
        DeviceInterface* _device;

        bool _is_scratch_buffer = false;
        uint64_t _scratch_max_memory = 0;
        
        uint64_t _allocated_memory = 0;
        uint64_t _default_chunk_size = 0;

        std::shared_ptr<DX12BufferChunk> _current_chunk;
        std::list<std::shared_ptr<DX12BufferChunk>> _chunk_pool;
    };


    class DX12CommandList : public CommandListInterface
    {
    public:
        DX12CommandList(
            const DX12Context* context,
            DX12DescriptorManager* descriptor_heaps,
            DeviceInterface* device,
            const CommandListDesc& desc
        );

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
        void set_texture_state(TextureInterface* texture, const TextureSubresourceSet& subresource, ResourceStates states) override;
        void set_buffer_state(BufferInterface* buffer, ResourceStates states) override;

        void commit_barriers() override;

        ResourceStates get_buffer_state(BufferInterface* buffer) override;
        ResourceStates get_texture_state(TextureInterface* texture, uint32_t array_slice, uint32_t mip_level) override;

        const CommandListDesc& get_desc() override;
        DeviceInterface* get_deivce() override;
        void* get_native_object() override;


        void executed(DX12CommandQueue& queue, uint64_t submit_id);
        std::shared_ptr<DX12InternalCommandList> get_current_command_list();

    private:
        void commit_descriptor_heaps();
		void set_binding_resource_state(BindingSetInterface* binding_set);
        
        bool set_graphics_state(const GraphicsState& state);
        bool set_compute_state(const ComputeState& state);
        void bind_graphics_binding_sets(
            const PipelineStateBindingSetArray& binding_sets, 
            GraphicsPipelineInterface* graphics_pipeline
        );
        void bind_compute_binding_sets(
            const PipelineStateBindingSetArray& binding_sets, 
            ComputePipelineInterface* graphics_pipeline
        );
        
    private:
        const DX12Context* _context;
        DX12DescriptorManager* _descriptor_manager;
        
        DeviceInterface* _device;
        inline static ResourceStateTracker _resource_state_tracker;
        
        CommandListDesc _desc;
        std::shared_ptr<DX12InternalCommandList> _current_cmdlist;
        
        std::unordered_map<BufferInterface*, D3D12_GPU_VIRTUAL_ADDRESS> _volatile_constant_buffer_address_cache;

        DX12UploadManager _upload_manager;
        DX12UploadManager _scratch_manager;

        uint64_t _recording_version = 0;

        GraphicsState _current_graphics_state;
        ComputeState _current_compute_state;
        std::vector<D3D12_RESOURCE_BARRIER> _d3d12_barriers;
    };

}







 #endif