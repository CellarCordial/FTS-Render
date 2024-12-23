/**
 * *****************************************************************************
 * @file        DX12CommandList.h
 * @brief       
 * @author      CellarCordial (591885295@qq.com)
 * @date        2024-06-03
 * @copyright Copyright (c) 2024
 * *****************************************************************************
 */

 #ifndef RHI_DX12_COMMANDLIST_H
 #define RHI_DX12_COMMANDLIST_H

#include "dx12_forward.h"
#include "dx12_descriptor.h"
#include "../command_list.h"
#include "../state_track.h"
#include "dx12_ray_tracing.h"
#include <atomic>
#include <combaseapi.h>
#include <d3d12.h>
#include <deque>
#include <list>
#include <memory>
#include <vector>

namespace fantasy 
{
    struct DX12InternalCommandList
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> d3d12_cmd_allocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> d3d12_cmdlist;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> d3d12_cmdlist4;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> d3d12_cmdlist6;
        uint64_t last_submitted_value = 0;
    };

    struct DX12CommandListInstance
    {
        Microsoft::WRL::ComPtr<ID3D12CommandList> d3d12_cmdlist;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> d3d12_cmd_allocator;
        CommandQueueType cmd_queue_type = CommandQueueType::Graphics;
        
        Microsoft::WRL::ComPtr<ID3D12Fence> fence;
        uint64_t submitted_value = 0;
        
        std::vector<ResourceInterface*> ref_resources;
        std::vector<StagingTextureInterface*> ref_staging_textures;
        std::vector<BufferInterface*> ref_staging_buffers;
        std::vector<TimerQueryInterface*> ref_timer_queries;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Object>> ref_native_resources;
    };

    struct DX12CommandQueue
    {
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_cmd_queue;
        Microsoft::WRL::ComPtr<ID3D12Fence> d3d12_fence;

        uint64_t last_submitted_value = 0;
        uint64_t last_compeleted_value = 0;

        std::atomic<uint64_t> redording_version = 1;
        std::deque<std::shared_ptr<DX12CommandListInstance>> dx12_cmdlists_in_flight;
        

        explicit DX12CommandQueue(const DX12Context& context, ID3D12CommandQueue* d3d12_queue) : d3d12_cmd_queue(d3d12_queue)
        {
            if (FAILED(context.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(d3d12_fence.GetAddressOf()))))
            {
                LOG_ERROR("Call to DX12CommandQueue constructor failed because create ID3D12Fence failed.");
                assert(d3d12_queue != nullptr);
            }
        }

        uint64_t update_last_completed_value()
        {
            if (last_compeleted_value < last_submitted_value)
            {
                last_compeleted_value = d3d12_fence->GetCompletedValue();
            }
            return last_compeleted_value;
        }
    };

    struct DX12BufferChunk
    {
        static const uint64_t size_alignment = 4096;     /**< GPU page size. */

        Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_buffer;
        uint64_t version = 0;
        uint64_t buffer_size = 0;
        uint64_t write_end_position = 0;
        void* cpu_address = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpu_address = 0;
        uint32_t index_in_pool = 0;

        ~DX12BufferChunk()
        {
            if (d3d12_buffer != nullptr && cpu_address != nullptr)
            {
                d3d12_buffer->Unmap(0, nullptr);
                cpu_address = nullptr;
            }
        }
    };

    class DX12UploadManager
    {
    public:
        DX12UploadManager(
            const DX12Context* context, 
            DX12CommandQueue* cmd_queue, 
            uint64_t default_chunk_size, 
            uint64_t memory_limit,
            bool dxr_scratch = false
        );

        bool suballocate_buffer(
            uint64_t size, 
            ID3D12Resource** d3d12_buffer, 
            uint64_t* offest, 
            uint8_t** cpu_address, 
            D3D12_GPU_VIRTUAL_ADDRESS* gpu_address, 
            uint64_t current_version, 
            uint32_t aligment = 256,
            ID3D12GraphicsCommandList* d3d12_cmdlist = nullptr
        );

        void submit_chunks(uint64_t current_version, uint64_t submitted_version);

    private:
        std::shared_ptr<DX12BufferChunk> create_bufferChunk(uint64_t size) const;

    private:
        const DX12Context* _context;
        DX12CommandQueue* _cmd_queue;

        uint64_t _default_chunk_size;
        uint64_t _max_memory_size;
        uint64_t _allocated_memory_size;

        std::list<std::shared_ptr<DX12BufferChunk>> _chunk_pool;
        std::shared_ptr<DX12BufferChunk> _current_chunk;

        bool _dxr_scratch;
    };


    class DX12CommandList : public CommandListInterface
    {
    public:
        DX12CommandList(
            const DX12Context* context,
            DX12DescriptorHeaps* descriptor_heaps,
            DeviceInterface* device,
            DX12CommandQueue* dx12_cmd_queue,
            const CommandListDesc& desc
        );

        bool initialize();

        // CommandListInterface
        bool open() override;
        bool close() override;

        bool clear_state() override;

        bool clear_texture_float(TextureInterface* texture, const TextureSubresourceSet& subresource_set, const Color& clear_color) override;
        bool clear_texture_uint(TextureInterface* texture, const TextureSubresourceSet& subresource_set, uint32_t dwClearColor) override;
        bool clear_depth_stencil_texture(TextureInterface* texture, const TextureSubresourceSet& subresource_set, bool clear_depth, float depth, bool clear_stencil, uint8_t stencil) override;
        
        bool write_texture(TextureInterface* dst, uint32_t array_slice, uint32_t mip_level, const uint8_t* data, uint64_t row_pitch, uint64_t depth_pitch) override;
        bool resolve_texture(TextureInterface* dst, const TextureSubresourceSet& crDstSubresourceSet, TextureInterface* src, const TextureSubresourceSet& crSrcSubresourceSet) override;
        bool copy_texture(TextureInterface* dst, const TextureSlice& dst_slice, TextureInterface* src, const TextureSlice& src_slice) override;
        bool copy_texture(StagingTextureInterface* dst, const TextureSlice& dst_slice, TextureInterface* src, const TextureSlice& src_slice) override;
        bool copy_texture(TextureInterface* dst, const TextureSlice& dst_slice, StagingTextureInterface* src, const TextureSlice& src_slice) override;
        
        bool write_buffer(BufferInterface* buffer, const void* data, uint64_t data_size, uint64_t dst_byte_offset) override;
        bool clear_buffer_uint(BufferInterface* buffer, uint32_t clear_value) override;
        bool copy_buffer(BufferInterface* dst, uint64_t dst_byte_offset, BufferInterface* src, uint64_t src_byte_offset, uint64_t data_byte_size) override;
        
        bool set_push_constants(const void* data, uint64_t byte_size) override;
        bool set_graphics_state(const GraphicsState& state) override;
        bool set_compute_state(const ComputeState& state) override;
        
        bool draw(const DrawArguments& arguments) override;
        bool draw_indexed(const DrawArguments& arguments) override;
        bool dispatch(uint32_t thread_group_num_x, uint32_t thread_group_num_y = 1, uint32_t thread_group_num_z = 1) override;
        
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

        ResourceStates get_texture_subresource_state(TextureInterface* texture, uint32_t array_slice, uint32_t mip_level) override;
        ResourceStates get_buffer_state(BufferInterface* buffer) override;

		DeviceInterface* get_deivce() override;
		CommandListDesc get_desc() override;
        void* get_native_object() override;

        bool set_ray_tracing_state(const ray_tracing::PipelineState& state) override;
		bool dispatch_rays(const ray_tracing::DispatchRaysArguments& arguments) override;

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

		bool set_accel_struct_state(ray_tracing::AccelStructInterface* accel_struct, ResourceStates state) override;


        ray_tracing::DX12ShaderTableState* get_shaderTableState(ray_tracing::ShaderTableInterface* shader_table);

        bool allocate_upload_buffer(uint64_t size, uint8_t** cpu_address, D3D12_GPU_VIRTUAL_ADDRESS* gpu_address);


        bool get_buffer_gpu_address(BufferInterface* buffer, D3D12_GPU_VIRTUAL_ADDRESS* gpu_address);
        bool update_graphics_volatile_buffers();
        bool update_compute_volatile_buffers();
        std::shared_ptr<DX12CommandListInstance> excuted(ID3D12Fence* d3d12_fence, uint64_t last_submitted_value);


		bool set_resource_state_from_binding_set(BindingSetInterface* binding_set);
		bool set_resource_state_from_frame_buffer(FrameBufferInterface* frame_buffer);
        bool set_staging_texture_state(StagingTextureInterface* staging_texture, ResourceStates state);

        bool set_graphics_bindings(
            const PipelineStateBindingSetArray& binding_sets, 
            uint32_t binding_update_mask, 
            DX12RootSignature* root_signature
        );
        bool set_compute_bindings(
            const PipelineStateBindingSetArray& binding_sets, 
            uint32_t binding_update_mask, 
            DX12RootSignature* root_signature
        );
        
    private:
        void clear_state_cache();
        bool bind_graphics_pipeline(GraphicsPipelineInterface* graphics_pipeline, bool update_root_signature) const;
        std::shared_ptr<DX12InternalCommandList> create_internal_cmdlist() const;
        ray_tracing::DX12ShaderTableState* get_shader_tabel_state(ray_tracing::ShaderTableInterface* shader_table);

        struct VolatileConstantBufferBinding
        {
            uint32_t binding_point;
            BufferInterface* buffer;
            D3D12_GPU_VIRTUAL_ADDRESS gpu_address;
        };

        
    public:
        std::shared_ptr<DX12InternalCommandList> active_cmdlist;

    private:
        const DX12Context* _context;
        DX12DescriptorHeaps* _descriptor_heaps;

        DeviceInterface* _device;
        DX12CommandQueue* _cmd_queue;
        DX12UploadManager _upload_manager;

        inline static ResourceStateTracker _resource_state_tracker;

        CommandListDesc _desc;

        std::list<std::shared_ptr<DX12InternalCommandList>> _cmdlist_pool;    // 容纳提交给 CmdQueue 的 CmdList.

        std::shared_ptr<DX12CommandListInstance> _cmdlist_ref_instances;
        uint64_t _recording_version = 0;

    
        // Cache

        GraphicsState _current_graphics_state;
        ComputeState _current_compute_state;
        bool _current_graphics_state_valid = false;
        bool _current_compute_state_valid = false;

        // Ray tracing
        DX12UploadManager _dx12_scratch_manager;
        ray_tracing::PipelineState _current_ray_tracing_state;
        bool _current_ray_tracing_state_valid = false;
        std::unordered_map<ray_tracing::ShaderTableInterface*, std::unique_ptr<ray_tracing::DX12ShaderTableState>> _shader_table_states;


        ID3D12DescriptorHeap* _current_srv_etc_heap = nullptr;
        ID3D12DescriptorHeap* _current_sampler_heap = nullptr;

        ID3D12Resource* _current_upload_buffer = nullptr;

        std::unordered_map<BufferInterface*, D3D12_GPU_VIRTUAL_ADDRESS> _volatile_constant_buffer_addresses;
        bool _any_volatile_constant_buffer_writes = false;

        std::vector<D3D12_RESOURCE_BARRIER> _d3d12_barriers;

        std::vector<VolatileConstantBufferBinding> _current_graphics_volatile_constant_buffers;
        std::vector<VolatileConstantBufferBinding> _current_compute_volatile_constant_buffers;
    };

}







 #endif