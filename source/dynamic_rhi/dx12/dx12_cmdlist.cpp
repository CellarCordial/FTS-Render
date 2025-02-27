#include "dx12_cmdlist.h"
#include "../../core/tools/check_cast.h"
#include <cassert>
#include <combaseapi.h>
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <intsafe.h>
#include <memory>
#include <minwindef.h>
#include <pix_win.h>
#include <utility>
#include <winerror.h>

#include "dx12_binding.h"
#include "dx12_device.h"
#include "dx12_convert.h"
#include "dx12_forward.h"
#include "dx12_frame_buffer.h"
#include "dx12_pipeline.h"
#include "dx12_ray_tracing.h"
#include "dx12_resource.h"


namespace fantasy 
{
    DX12InternalCommandList::DX12InternalCommandList(const DX12Context* context_, uint64_t recording_id_) : 
        context(context_), recording_id(recording_id_)
    {
    }

    bool DX12InternalCommandList::initialize(CommandQueueType queue_type)
    {
        D3D12_COMMAND_LIST_TYPE cmdlist_type;
        switch (queue_type)
        {
        case CommandQueueType::Graphics: cmdlist_type = D3D12_COMMAND_LIST_TYPE_DIRECT; break;
        case CommandQueueType::Compute:  cmdlist_type = D3D12_COMMAND_LIST_TYPE_COMPUTE; break;
        default: 
            LOG_ERROR("invalid Enumeration value");
            return false;
        }

        ReturnIfFalse(SUCCEEDED(context->device->CreateCommandAllocator(cmdlist_type, IID_PPV_ARGS(d3d12_cmd_allocator.GetAddressOf()))));
        ReturnIfFalse(SUCCEEDED(context->device->CreateCommandList(
            0, 
            cmdlist_type, 
            d3d12_cmd_allocator.Get(), 
            nullptr, 
            IID_PPV_ARGS(d3d12_cmdlist.GetAddressOf())
        )));

        ReturnIfFalse(SUCCEEDED(d3d12_cmdlist->QueryInterface(IID_PPV_ARGS(d3d12_cmdlist4.GetAddressOf()))));
        ReturnIfFalse(SUCCEEDED(d3d12_cmdlist->QueryInterface(IID_PPV_ARGS(d3d12_cmdlist6.GetAddressOf()))));

        d3d12_cmdlist->Close();

        return true;
    }

    DX12CommandQueue::DX12CommandQueue(
        const DX12Context* context, 
        CommandQueueType queue_type_, 
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_cmdqueue_
    ) :
        _context(context), queue_type(queue_type_), d3d12_cmdqueue(d3d12_cmdqueue_)
    {
    }

    bool DX12CommandQueue::initialize()
    {
        return SUCCEEDED(_context->device->CreateFence(
            0, 
            D3D12_FENCE_FLAG_NONE, 
            IID_PPV_ARGS(d3d12_tracking_fence.GetAddressOf())
        ));
    }

    std::shared_ptr<DX12InternalCommandList> DX12CommandQueue::get_command_list()
    {
        std::lock_guard lock(_mutex);

        std::shared_ptr<DX12InternalCommandList> cmdlist;
        if (_cmdlists_pool.empty())
        {
            cmdlist = std::make_shared<DX12InternalCommandList>(_context, ++last_recording_id);
            if (!cmdlist->initialize(queue_type))
            {
                LOG_ERROR("Create command buffer failed.");
                return nullptr;
            }
        }
        else
        {
            cmdlist = _cmdlists_pool.front();
            _cmdlists_pool.pop_front();
        }
        
        return cmdlist;
    }
    
    void DX12CommandQueue::add_wait_fence(Microsoft::WRL::ComPtr<ID3D12Fence> fence, uint64_t value)
    {
        if (!fence) return;

        _d3d12_wait_fences.push_back(fence);
        _wait_fence_values.push_back(value);
    }

    void DX12CommandQueue::add_signal_fence(Microsoft::WRL::ComPtr<ID3D12Fence> fence, uint64_t value)
    {
        if (!fence) return;

        _d3d12_signal_fences.push_back(fence);
        _signal_fence_values.push_back(value);
    }
    
    uint64_t DX12CommandQueue::execute(CommandListInterface* const* cmdlists, uint32_t cmd_count)
    {
        if (cmd_count == 0) return last_submitted_id;

        last_submitted_id++;

        std::vector<ID3D12CommandList*> d3d12_cmdlists(cmd_count);
        for (uint64_t ix = 0; ix < cmd_count; ++ix)
        {
            auto cmdlist = check_cast<DX12CommandList*>(cmdlists[ix])->get_current_command_list();
            
            d3d12_cmdlists[ix] = cmdlist->d3d12_cmdlist.Get();
            
            _cmdlists_in_flight.push_back(cmdlist);
        }

        _d3d12_signal_fences.push_back(d3d12_tracking_fence);
        _signal_fence_values.push_back(last_submitted_id);

        for (uint32_t ix = 0; ix < _d3d12_wait_fences.size(); ++ix)
        {
            ReturnIfFalse(SUCCEEDED(d3d12_cmdqueue->Wait(_d3d12_wait_fences[ix].Get(), _wait_fence_values[ix])));
        }

        d3d12_cmdqueue->ExecuteCommandLists(static_cast<uint32_t>(d3d12_cmdlists.size()), d3d12_cmdlists.data());

        for (uint32_t ix = 0; ix < _d3d12_signal_fences.size(); ++ix)
        {
            ReturnIfFalse(SUCCEEDED(d3d12_cmdqueue->Signal(_d3d12_signal_fences[ix].Get(), _signal_fence_values[ix])));
        }

        if (FAILED(_context->device->GetDeviceRemovedReason()))
        {
            LOG_CRITICAL("Device removed.");
            return INVALID_SIZE_64;
        }

        for (size_t ix = 0; ix < cmd_count; ix++)
        {
            check_cast<DX12CommandList*>(cmdlists[ix])->executed(last_submitted_id);
        }
        
        _d3d12_wait_fences.clear();
        _wait_fence_values.clear();
        _d3d12_signal_fences.clear();
        _signal_fence_values.clear();
        
        return last_submitted_id;
    }
    
    uint64_t DX12CommandQueue::get_last_finished_id()
    {
        return d3d12_tracking_fence->GetCompletedValue();
    }

    void DX12CommandQueue::retire_command_lists()
    {
        std::list<std::shared_ptr<DX12InternalCommandList>> flight_cmdlists = std::move(_cmdlists_in_flight);

        for (const auto& cmdlist : flight_cmdlists)
        {
            if (cmdlist->submit_id <= get_last_finished_id())
            {
                cmdlist->submit_id = INVALID_SIZE_64;

                _cmdlists_pool.push_back(cmdlist);
            }
            else
            {
                _cmdlists_in_flight.push_back(cmdlist);
            }
        }
    }
    
    std::shared_ptr<DX12InternalCommandList> DX12CommandQueue::get_command_list_in_flight(uint64_t submit_id)
    {
        for (const auto& cmdlist : _cmdlists_in_flight)
        {
            if (cmdlist->submit_id == submit_id) return cmdlist;
        }

        return nullptr;
    }
    
    bool DX12CommandQueue::wait_command_list(uint64_t cmdlist_id, uint64_t timeout)
    {
        if (cmdlist_id == INVALID_SIZE_32) return false;

        return wait_for_fence(d3d12_tracking_fence.Get(), cmdlist_id);
    }

    void DX12CommandQueue::wait_for_idle()
    {
        wait_for_fence(d3d12_tracking_fence.Get(), last_submitted_id);
    }


    DX12BufferChunk::DX12BufferChunk(DeviceInterface* device, const BufferDesc& desc, bool map_memory)
    {
        buffer = std::shared_ptr<BufferInterface>(device->create_buffer(desc));
        mapped_memory = map_memory ? buffer->map(CpuAccessMode::Write) : nullptr;
        buffer_size = desc.byte_size;
    }

    DX12BufferChunk::~DX12BufferChunk()
    {
        if (mapped_memory != nullptr) buffer->unmap();
    }

    
    DX12UploadManager::DX12UploadManager(DeviceInterface* device, uint64_t default_chunk_size, uint64_t max_memory, bool is_scratch_buffer) :
        _device(device), 
        _default_chunk_size(default_chunk_size), 
        _scratch_max_memory(max_memory), 
        _is_scratch_buffer(is_scratch_buffer)
    {
    }

    bool DX12UploadManager::suballocate_buffer(
        uint64_t size, 
        BufferInterface** out_buffer, 
        uint64_t* out_offset, 
        void** out_cpu_address, 
        uint64_t current_version, 
        uint32_t alignment,
        CommandListInterface* cmdlist
    )
    {
        ReturnIfFalse(out_cpu_address);

        std::shared_ptr<DX12BufferChunk> retire_chunk;

        if (_current_chunk)
        {
            uint64_t offset = align(_current_chunk->write_pointer, static_cast<uint64_t>(alignment));
            uint64_t data_end = offset + size;

            if (data_end <= _current_chunk->buffer_size)
            {
                _current_chunk->write_pointer = data_end;

                *out_buffer = _current_chunk->buffer.get();
                *out_offset = offset;
                if (_current_chunk->mapped_memory)
                {
                    *out_cpu_address = (char*)_current_chunk->mapped_memory + offset;
                }

                return true;
            }

            retire_chunk = _current_chunk;
            _current_chunk.reset();
        }

        if (retire_chunk) _chunk_pool.push_back(retire_chunk);

        
        uint64_t completed_id = check_cast<DX12Device*>(_device)->queue_get_completed_id(get_version_queue_type(current_version));

        for (auto iter = _chunk_pool.begin(); iter != _chunk_pool.end(); ++iter)
        {
            std::shared_ptr<DX12BufferChunk> chunk = *iter;

            if (is_version_submitted(chunk->version) && get_version_id(chunk->version) <= completed_id)
            {
                chunk->version = 0;
            }

            if (chunk->version == 0 && chunk->buffer_size >= size)
            {
                _chunk_pool.erase(iter);
                _current_chunk = chunk;
                break;
            }
        }

        if (!_current_chunk)
        {
            uint64_t alloc_size = align(std::max(size, _default_chunk_size), DX12BufferChunk::align_size);

            if (_scratch_max_memory > 0 && _allocated_memory + alloc_size > _scratch_max_memory)
            {
                if (_is_scratch_buffer)
                {
                    ReturnIfFalse(cmdlist != nullptr);

                    std::shared_ptr<DX12BufferChunk> suitable_chunk;

                    for (const auto& chunk : _chunk_pool)
                    {
                        if (chunk->buffer_size >= alloc_size)
                        {
                            if (!suitable_chunk)
                            {
                                suitable_chunk = chunk;
                                continue;
                            }

                            bool chunk_submitted = is_version_submitted(chunk->version);
                            bool suitable_chunk_submitted = is_version_submitted(suitable_chunk->version);
                            uint64_t chunk_id = get_version_id(chunk->version);
                            uint64_t suitable_chunk_id = get_version_id(suitable_chunk->version);

                            if (
                                chunk_submitted && !suitable_chunk_submitted ||
                                chunk_submitted == suitable_chunk_submitted && 
                                chunk_id < suitable_chunk_id ||
                                chunk_submitted == suitable_chunk_submitted && 
                                chunk_id == suitable_chunk_id && 
                                chunk->buffer_size > suitable_chunk->buffer_size
                            )
                            {
                                suitable_chunk = chunk;
                            }
                        }
                    }
                    ReturnIfFalse(suitable_chunk != nullptr);

                    _chunk_pool.erase(std::find(_chunk_pool.begin(), _chunk_pool.end(), suitable_chunk));
                    _current_chunk = suitable_chunk;

                    D3D12_RESOURCE_BARRIER d3d12_uav_barrier = {};
                    d3d12_uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                    d3d12_uav_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                    d3d12_uav_barrier.UAV.pResource = reinterpret_cast<ID3D12Resource*>(suitable_chunk->buffer->get_native_object());
                    reinterpret_cast<ID3D12GraphicsCommandList*>(cmdlist->get_native_object())->ResourceBarrier(1, &d3d12_uav_barrier);
                }
                else
                {
                    return false;
                }
            }
            else 
            {
                _current_chunk = create_chunk(alloc_size);
            }

        }

        _current_chunk->version = current_version;
        _current_chunk->write_pointer = size;

        *out_buffer = _current_chunk->buffer.get();
        *out_offset = 0;
        *out_cpu_address = _current_chunk->mapped_memory;
        return true;
    }

    void DX12UploadManager::submit_chunks(uint64_t current_version, uint64_t submitted_version)
    {
        if (_current_chunk != nullptr)
        {
            _chunk_pool.push_back(_current_chunk);
            _current_chunk.reset();
        }

        for (const auto& chunk : _chunk_pool)
        {
            if (chunk->version == current_version)
            {
                chunk->version = submitted_version;
            }
        }
    }

    std::shared_ptr<DX12BufferChunk> DX12UploadManager::create_chunk(uint64_t size) const
    {
        std::shared_ptr<DX12BufferChunk> chunk;

        if (_is_scratch_buffer)
        {
            BufferDesc desc{};
            desc.name = "scratch buffer chunk";
            desc.byte_size = size;
            desc.cpu_access = CpuAccessMode::None;
            desc.allow_unordered_access = true;
            chunk = std::make_shared<DX12BufferChunk>(_device, desc, false);
        }
        else
        {
            BufferDesc desc;
            desc.name = "upload buffer chunk";
            desc.byte_size = size;
            desc.cpu_access = CpuAccessMode::Write;

            chunk = std::make_shared<DX12BufferChunk>(_device, desc, true);
        }

        return chunk;
    }


    DX12CommandList::DX12CommandList(
        const DX12Context* context,
        DX12DescriptorManager* descriptor_heaps,
        DeviceInterface* device,
        const CommandListDesc& desc
    ) :
        _context(context), 
        _descriptor_manager(descriptor_heaps),
        _device(device),
        _desc(desc),
        _upload_manager(device, desc.upload_chunk_size, 0, false),
        _scratch_manager(device, desc.scratch_chunk_size, desc.scratch_max_mamory, true)
    {
    }

    bool DX12CommandList::initialize()
    {
        return true;
    }

    bool DX12CommandList::open()
    {
        _current_cmdlist = check_cast<DX12Device*>(_device)->get_queue(_desc.queue_type)->get_command_list();

        _current_cmdlist->d3d12_cmdlist->SetName(std::wstring(_desc.name.begin(), _desc.name.end()).c_str());
        
        _current_cmdlist->d3d12_cmd_allocator->Reset();
        _current_cmdlist->d3d12_cmdlist->Reset(_current_cmdlist->d3d12_cmd_allocator.Get(), nullptr);

        return true;
    }
    
    bool DX12CommandList::close()
    {
        if (_desc.queue_type == CommandQueueType::Graphics)
        {
            // DX12 不允许在 compute cmdlist 中存在有资源处于 GraphicsShaderResource 或 RenderTarget state, 而我们不知道下一条
            // 处理相同资源的 cmdlist 是否是 compute 的, 所以需要在此处将 GraphicsShaderResource 资源转换为 Common state.
            for (const auto& pair : _resource_state_tracker.texture_states)
            {
                if ((pair.second->state & ResourceStates::GraphicsShaderResource) != 0 ||
                    (pair.second->state & ResourceStates::RenderTarget) != 0)
                {
                    set_texture_state(
                        pair.first, 
                        TextureSubresourceSet{
                            .base_mip_level = 0,
                            .mip_level_count = pair.first->get_desc().mip_levels,
                            .base_array_slice = 0,
                            .array_slice_count = pair.first->get_desc().array_size
                        }, 
                        ResourceStates::Common
                    );
                }
            }
            for (const auto& pair : _resource_state_tracker.buffer_states)
            {
                if (pair.second->state == ResourceStates::GraphicsShaderResource)
                {
                    set_buffer_state(pair.first, ResourceStates::Common);
                }
            }
        }

        commit_barriers();
        
        _current_cmdlist->d3d12_cmdlist->Close();
        return true;
    }


    void DX12CommandList::clear_texture_float(TextureInterface* texture_, const TextureSubresourceSet& subresource, const Color& clear_color)
    {
        DX12Texture* texture = check_cast<DX12Texture*>(texture_);

        set_texture_state(texture_, subresource, ResourceStates::UnorderedAccess);
        commit_barriers();
        commit_descriptor_heaps();

        for (uint32_t mip = subresource.base_mip_level; mip < subresource.base_mip_level + subresource.mip_level_count; ++mip)
        {
            TextureSubresourceSet subresource_mip{
                .base_mip_level = mip,
                .mip_level_count = 1,
                .base_array_slice = subresource.base_array_slice,
                .array_slice_count = subresource.array_slice_count
            };
            

            uint32_t view_index = texture->get_view_index(ResourceViewType::Texture_UAV, subresource_mip);
            _descriptor_manager->shader_resource_heap.copy_to_shader_visible_heap(view_index);

            _current_cmdlist->d3d12_cmdlist->ClearUnorderedAccessViewFloat(
                _descriptor_manager->shader_resource_heap.get_gpu_handle(view_index), 
                _descriptor_manager->shader_resource_heap.get_cpu_handle(view_index), 
                reinterpret_cast<ID3D12Resource*>(texture->get_native_object()), 
                &clear_color.r, 
                0, 
                nullptr
            );
        }
    }

    void DX12CommandList::commit_descriptor_heaps()
    {
        ID3D12DescriptorHeap* d3d12_srv_etc_heap = _descriptor_manager->shader_resource_heap.get_shader_visible_heap();
        ID3D12DescriptorHeap* d3d12_sampler_heap = _descriptor_manager->sampler_heap.get_shader_visible_heap();

        ID3D12DescriptorHeap* d3d12_heaps[2] = { d3d12_srv_etc_heap, d3d12_sampler_heap };
        _current_cmdlist->d3d12_cmdlist->SetDescriptorHeaps(2, d3d12_heaps);
    }

    void DX12CommandList::clear_texture_uint(TextureInterface* texture, const TextureSubresourceSet& subresource, uint32_t clear_color)
    {
        DX12Texture* dx12_texture = check_cast<DX12Texture*>(texture);
        
        uint32_t clear_colors[4] = { clear_color, clear_color, clear_color, clear_color };

        set_texture_state(texture, subresource, ResourceStates::UnorderedAccess);
        commit_barriers();
        commit_descriptor_heaps();

        for (uint32_t mip = subresource.base_mip_level; mip < subresource.base_mip_level + subresource.mip_level_count; ++mip)
        {
            TextureSubresourceSet subresource_mip{
                .base_mip_level = mip,
                .mip_level_count = 1,
                .base_array_slice = subresource.base_array_slice,
                .array_slice_count = subresource.array_slice_count
            };
            

            uint32_t view_index = dx12_texture->get_view_index(ResourceViewType::Texture_UAV, subresource_mip);
            _descriptor_manager->shader_resource_heap.copy_to_shader_visible_heap(view_index);

            _current_cmdlist->d3d12_cmdlist->ClearUnorderedAccessViewUint(
                _descriptor_manager->shader_resource_heap.get_gpu_handle(view_index), 
                _descriptor_manager->shader_resource_heap.get_cpu_handle(view_index), 
                reinterpret_cast<ID3D12Resource*>(dx12_texture->get_native_object()), 
                clear_colors, 
                0, 
                nullptr
            );
        }
    }

    void DX12CommandList::clear_render_target_texture(
        TextureInterface* texture_, 
        const TextureSubresourceSet& subresource, 
        const Color& clear_color
    )
    {
        DX12Texture* texture = check_cast<DX12Texture*>(texture_);

        set_texture_state(texture_, subresource, ResourceStates::RenderTarget);
        commit_barriers();

        for (uint32_t mip = subresource.base_mip_level; mip < subresource.base_mip_level + subresource.mip_level_count; ++mip)
        {
            TextureSubresourceSet subresource_mip{
                .base_mip_level = mip,
                .mip_level_count = 1,
                .base_array_slice = subresource.base_array_slice,
                .array_slice_count = subresource.array_slice_count
            };
            
            uint32_t view_index = texture->get_view_index(ResourceViewType::Texture_RTV, subresource_mip);

            _current_cmdlist->d3d12_cmdlist->ClearRenderTargetView(
                _descriptor_manager->render_target_heap.get_cpu_handle(view_index), 
                &clear_color.r, 
                0, 
                nullptr
            );
        }
    }

    void DX12CommandList::clear_depth_stencil_texture(
        TextureInterface* texture, 
        const TextureSubresourceSet& subresource, 
        bool clear_depth, 
        float depth, 
        bool clear_stencil, 
        uint8_t stencil
    )
    {
        assert(clear_depth || clear_stencil);
        
        DX12Texture* dx12_texture = check_cast<DX12Texture*>(texture);
        const auto& texture_desc = dx12_texture->get_desc();

		set_texture_state(texture, subresource, ResourceStates::DepthWrite);
        commit_barriers();

        D3D12_CLEAR_FLAGS d3d12_clear_flags = D3D12_CLEAR_FLAGS(0);
        if (clear_depth) d3d12_clear_flags |= D3D12_CLEAR_FLAG_DEPTH;
        if (clear_stencil) d3d12_clear_flags |= D3D12_CLEAR_FLAG_STENCIL;

        for (uint32_t mip = subresource.base_mip_level; mip < subresource.base_mip_level + subresource.mip_level_count; ++mip)
        {
            TextureSubresourceSet subresource_mip{
                .base_mip_level = mip,
                .mip_level_count = 1,
                .base_array_slice = subresource.base_array_slice,
                .array_slice_count = subresource.array_slice_count
            };

            uint32_t view_index = dx12_texture->get_view_index(ResourceViewType::Texture_DSV, subresource_mip);

            _current_cmdlist->d3d12_cmdlist->ClearDepthStencilView(
                _descriptor_manager->depth_stencil_heap.get_cpu_handle(view_index), 
                d3d12_clear_flags, 
                depth, 
                stencil, 
                0, 
                nullptr
            );
        }
    }
    
    void DX12CommandList::copy_texture(TextureInterface* dst, const TextureSlice& dst_slice, TextureInterface* src, const TextureSlice& src_slice)
    {
        uint32_t dst_subresource_index = calculate_texture_subresource(
            dst_slice.mip_level, 
            dst_slice.array_slice, 
            dst->get_desc().mip_levels 
        );

        uint32_t src_subresource_index = calculate_texture_subresource(
            src_slice.mip_level, 
            src_slice.array_slice, 
            src->get_desc().mip_levels 
        );

        D3D12_TEXTURE_COPY_LOCATION d3d12_dst_texture_location;
        d3d12_dst_texture_location.pResource        = reinterpret_cast<ID3D12Resource*>(dst->get_native_object());
        d3d12_dst_texture_location.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        d3d12_dst_texture_location.SubresourceIndex = dst_subresource_index;

        D3D12_TEXTURE_COPY_LOCATION d3d12_src_texture_location;
        d3d12_src_texture_location.pResource        = reinterpret_cast<ID3D12Resource*>(src->get_native_object());
        d3d12_src_texture_location.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        d3d12_src_texture_location.SubresourceIndex = src_subresource_index;

		set_texture_state(dst, TextureSubresourceSet{ dst_slice.mip_level, 1, dst_slice.array_slice, 1 }, ResourceStates::CopyDst);
		set_texture_state(src, TextureSubresourceSet{ src_slice.mip_level, 1, src_slice.array_slice, 1 }, ResourceStates::CopySrc);
        commit_barriers();
        
        if (
            dst_slice.width == 0 || src_slice.width == 0 ||
            dst_slice.height == 0 || src_slice.height == 0 ||
            dst_slice.depth == 0 || src_slice.depth == 0
        )
        {
            _current_cmdlist->d3d12_cmdlist->CopyTextureRegion(
                &d3d12_dst_texture_location,
                0, 0, 0,
                &d3d12_src_texture_location,
                nullptr
            );
            return;
        }

        D3D12_BOX d3d12_src_box;
        d3d12_src_box.left   = src_slice.x;
        d3d12_src_box.top    = src_slice.y;
        d3d12_src_box.front  = src_slice.z;
        d3d12_src_box.right  = src_slice.x + src_slice.width;
        d3d12_src_box.bottom = src_slice.y + src_slice.height;
        d3d12_src_box.back   = src_slice.z + src_slice.depth;

        _current_cmdlist->d3d12_cmdlist->CopyTextureRegion(
            &d3d12_dst_texture_location,
            dst_slice.x,
            dst_slice.y,
            dst_slice.z,
            &d3d12_src_texture_location,
            &d3d12_src_box
        );
    }

    void DX12CommandList::copy_texture(StagingTextureInterface* dst, const TextureSlice& dst_slice, TextureInterface* src, const TextureSlice& src_slice)
    {
        uint32_t src_subresource_index = calculate_texture_subresource(
            src_slice.mip_level, 
            src_slice.array_slice, 
            src->get_desc().mip_levels
        );

        D3D12_TEXTURE_COPY_LOCATION d3d12_dst_texture_location;
        d3d12_dst_texture_location.pResource        = reinterpret_cast<ID3D12Resource*>(dst->get_native_object());
        d3d12_dst_texture_location.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        d3d12_dst_texture_location.PlacedFootprint = 
            check_cast<DX12StagingTexture*>(dst)->get_slice_region(dst_slice.mip_level, dst_slice.array_slice).d3d12_foot_print;

        D3D12_TEXTURE_COPY_LOCATION d3d12_src_texture_location;
        d3d12_src_texture_location.pResource        = reinterpret_cast<ID3D12Resource*>(src->get_native_object());
        d3d12_src_texture_location.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        d3d12_src_texture_location.SubresourceIndex = src_subresource_index;

		set_buffer_state(check_cast<DX12StagingTexture*>(dst)->get_buffer(), ResourceStates::CopyDst);
		set_texture_state(src, TextureSubresourceSet{ src_slice.mip_level, 1, src_slice.array_slice, 1 }, ResourceStates::CopySrc);
        commit_barriers();

        if (
            dst_slice.width == 0 || src_slice.width == 0 ||
            dst_slice.height == 0 || src_slice.height == 0 ||
            dst_slice.depth == 0 || src_slice.depth == 0
        )
        {
            _current_cmdlist->d3d12_cmdlist->CopyTextureRegion(
                &d3d12_dst_texture_location,
                0, 0, 0,
                &d3d12_src_texture_location,
                nullptr
            );
            return;
        }

        D3D12_BOX d3d12_src_box;
        d3d12_src_box.left   = src_slice.x;
        d3d12_src_box.top    = src_slice.y;
        d3d12_src_box.front  = src_slice.z;
        d3d12_src_box.right  = src_slice.x + src_slice.width;
        d3d12_src_box.bottom = src_slice.y + src_slice.height;
        d3d12_src_box.back   = src_slice.z + src_slice.depth;

        _current_cmdlist->d3d12_cmdlist->CopyTextureRegion(
            &d3d12_dst_texture_location,
            dst_slice.x,
            dst_slice.y,
            dst_slice.z,
            &d3d12_src_texture_location,
            &d3d12_src_box
        );
    }

    void DX12CommandList::copy_texture(TextureInterface* dst, const TextureSlice& dst_slice, StagingTextureInterface* src, const TextureSlice& src_slice)
    { 
        uint32_t dst_subresource_index = calculate_texture_subresource(
            dst_slice.mip_level, 
            dst_slice.array_slice, 
            dst->get_desc().mip_levels
        );

        D3D12_TEXTURE_COPY_LOCATION d3d12_dst_texture_location;
        d3d12_dst_texture_location.pResource        = reinterpret_cast<ID3D12Resource*>(dst->get_native_object());
        d3d12_dst_texture_location.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        d3d12_dst_texture_location.SubresourceIndex = dst_subresource_index;

        D3D12_TEXTURE_COPY_LOCATION d3d12_src_texture_location;
        d3d12_src_texture_location.pResource        = reinterpret_cast<ID3D12Resource*>(src->get_native_object());
        d3d12_src_texture_location.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        d3d12_src_texture_location.PlacedFootprint = 
            check_cast<DX12StagingTexture*>(src)->get_slice_region(src_slice.mip_level, src_slice.array_slice).d3d12_foot_print;

		set_texture_state(dst, TextureSubresourceSet{ dst_slice.mip_level, 1, dst_slice.array_slice, 1 }, ResourceStates::CopyDst);
		set_buffer_state(check_cast<DX12StagingTexture*>(src)->get_buffer(), ResourceStates::CopySrc);
        commit_barriers();

        if (
            dst_slice.width == 0 || src_slice.width == 0 ||
            dst_slice.height == 0 || src_slice.height == 0 ||
            dst_slice.depth == 0 || src_slice.depth == 0
        )
        {
            _current_cmdlist->d3d12_cmdlist->CopyTextureRegion(
                &d3d12_dst_texture_location,
                0, 0, 0,
                &d3d12_src_texture_location,
                nullptr
            );
            return;
        }

        D3D12_BOX d3d12_src_box;
        d3d12_src_box.left   = src_slice.x;
        d3d12_src_box.top    = src_slice.y;
        d3d12_src_box.front  = src_slice.z;
        d3d12_src_box.right  = src_slice.x + src_slice.width;
        d3d12_src_box.bottom = src_slice.y + src_slice.height;
        d3d12_src_box.back   = src_slice.z + src_slice.depth;

        _current_cmdlist->d3d12_cmdlist->CopyTextureRegion(
            &d3d12_dst_texture_location,
            dst_slice.x,
            dst_slice.y,
            dst_slice.z,
            &d3d12_src_texture_location,
            &d3d12_src_box
        );
    }
    
    bool DX12CommandList::write_texture(
        TextureInterface* dst, 
        uint32_t array_slice, 
        uint32_t mip_level, 
        const uint8_t* data,
        uint64_t data_size
    )
    { 
        if (data_size == 0) return true;

		set_texture_state(dst, TextureSubresourceSet{ mip_level, 1, array_slice, 1 }, ResourceStates::CopyDst);
        commit_barriers();

        uint32_t subresource_index = calculate_texture_subresource(
            mip_level,
            array_slice,
            dst->get_desc().mip_levels
        );

        ID3D12Resource* d3d12_resource = reinterpret_cast<ID3D12Resource*>(dst->get_native_object());

        D3D12_RESOURCE_DESC d3d12_resource_desc = d3d12_resource->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT d3d12_foot_print;
        uint32_t row_num;
        uint64_t row_size_in_byte;
        uint64_t total_bytes;
        
        _context->device->GetCopyableFootprints(&d3d12_resource_desc, subresource_index, 1, 0, &d3d12_foot_print, &row_num, &row_size_in_byte, &total_bytes);
        ReturnIfFalse(total_bytes == data_size);
        
        BufferInterface* upload_buffer;
        void* mapped_address;
        ReturnIfFalse(_upload_manager.suballocate_buffer(
            total_bytes, 
            &upload_buffer, 
            &d3d12_foot_print.Offset, 
            &mapped_address, 
            make_version(_current_cmdlist->recording_id, _desc.queue_type, false), 
            D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
        ));

        uint32_t row_pitch = d3d12_foot_print.Footprint.RowPitch;
        uint32_t height = d3d12_foot_print.Footprint.Height;
        uint32_t depth = d3d12_foot_print.Footprint.Depth;
        uint32_t depth_pitch = row_pitch * height;
        
        uint8_t* dst_address = reinterpret_cast<uint8_t*>(mapped_address);
        for (uint32_t z = 0; z < depth; z++)
        {
            const uint8_t* src_address = reinterpret_cast<const uint8_t*>(data) + depth_pitch * z;
            for (uint32_t row = 0; row < height; row++)
            {
                memcpy(dst_address, src_address, row_pitch);
                dst_address += row_pitch;
                src_address += row_pitch;
            }
        }


        D3D12_TEXTURE_COPY_LOCATION dst_location;
        dst_location.pResource = reinterpret_cast<ID3D12Resource*>(dst->get_native_object());
        dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst_location.SubresourceIndex = subresource_index;

        D3D12_TEXTURE_COPY_LOCATION src_location;
        src_location.pResource = reinterpret_cast<ID3D12Resource*>(upload_buffer->get_native_object());
        src_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src_location.PlacedFootprint = d3d12_foot_print;

        _current_cmdlist->d3d12_cmdlist->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

        return true; 
    }

    bool DX12CommandList::write_buffer(BufferInterface* buffer, const void* data, uint64_t data_size, uint64_t dst_byte_offset)
    {
        if (data_size == 0) return true;

        BufferInterface* upload_buffer;
        uint64_t upload_offset;
        void* mapped_address;
        ReturnIfFalse(_upload_manager.suballocate_buffer(
            data_size, 
            &upload_buffer, 
            &upload_offset, 
            &mapped_address,
            make_version(_current_cmdlist->recording_id, _desc.queue_type, false),
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
        ));

        memcpy(mapped_address, data, data_size);

        ID3D12Resource* d3d12_upload_resource = reinterpret_cast<ID3D12Resource*>(upload_buffer->get_native_object());

        set_buffer_state(buffer, ResourceStates::CopyDst);
        commit_barriers();

        _current_cmdlist->d3d12_cmdlist->CopyBufferRegion(
            reinterpret_cast<ID3D12Resource*>(buffer->get_native_object()), 
            dst_byte_offset, 
            d3d12_upload_resource, 
            upload_offset, 
            data_size
        );

        return true;
    }

    void DX12CommandList::clear_buffer_uint(BufferInterface* buffer, BufferRange range, uint32_t clear_value)
    {
		set_buffer_state(buffer, ResourceStates::UnorderedAccess);
        commit_barriers();
        commit_descriptor_heaps();

        uint32_t view_index = check_cast<DX12Buffer*>(buffer)->get_view_index(ResourceViewType::RawBuffer_UAV, range);
        _descriptor_manager->shader_resource_heap.copy_to_shader_visible_heap(view_index);

        const uint32_t clear_values[4] = { clear_value, clear_value, clear_value, clear_value };
        _current_cmdlist->d3d12_cmdlist->ClearUnorderedAccessViewUint(
            _descriptor_manager->shader_resource_heap.get_gpu_handle(view_index), 
            _descriptor_manager->shader_resource_heap.get_cpu_handle(view_index), 
            reinterpret_cast<ID3D12Resource*>(buffer->get_native_object()), 
            clear_values, 
            0, 
            nullptr
        );
    }
    
    void DX12CommandList::copy_buffer(BufferInterface* dst, uint64_t dst_byte_offset, BufferInterface* src, uint64_t src_byte_offset, uint64_t data_byte_size)
    {
		set_buffer_state(dst, ResourceStates::CopyDst);
		set_buffer_state(src, ResourceStates::CopySrc);
        commit_barriers();

        const auto& dst_desc = dst->get_desc();
        const auto& src_desc = src->get_desc();

        _current_cmdlist->d3d12_cmdlist->CopyBufferRegion(
            reinterpret_cast<ID3D12Resource*>(dst->get_native_object()),
            dst_byte_offset,
            reinterpret_cast<ID3D12Resource*>(src->get_native_object()),
            src_byte_offset,
            data_byte_size
        );
    }

    bool DX12CommandList::set_graphics_state(const GraphicsState& state)
    {
        _current_graphics_state = state;

        DX12GraphicsPipeline* pipeline = check_cast<DX12GraphicsPipeline*>(_current_graphics_state.pipeline);

        for (uint32_t ix = 0; ix < _current_graphics_state.binding_sets.size(); ++ix)
        {
            ReturnIfFalse(_current_graphics_state.binding_sets[ix]->get_layout() == pipeline->desc.binding_layouts[ix].get());

            set_binding_resource_state(_current_graphics_state.binding_sets[ix]);
        }

        if (_current_graphics_state.index_buffer_binding.is_valid())
        {
			set_buffer_state(_current_graphics_state.index_buffer_binding.buffer.get(), ResourceStates::IndexBuffer); 
        }

        for (const auto& binding : _current_graphics_state.vertex_buffer_bindings)
        {
            set_buffer_state(binding.buffer.get(), ResourceStates::VertexBuffer);
        }

        if (_current_graphics_state.indirect_buffer)
        {
            set_buffer_state(_current_graphics_state.indirect_buffer, ResourceStates::IndirectArgument);
        }

        const FrameBufferDesc& frame_buffer_desc = _current_graphics_state.frame_buffer->get_desc();
        for (uint32_t ix = 0; ix < frame_buffer_desc.color_attachments.size(); ++ix)
        {
            const auto& attachment = frame_buffer_desc.color_attachments[ix];
            set_texture_state(attachment.texture.get(), attachment.subresource, ResourceStates::RenderTarget);
        }
        if (frame_buffer_desc.depth_stencil_attachment.is_valid())
        {
            set_texture_state(
                frame_buffer_desc.depth_stencil_attachment.texture.get(), 
                frame_buffer_desc.depth_stencil_attachment.subresource, 
                ResourceStates::DepthWrite
            );
        }

        commit_barriers();
        commit_descriptor_heaps();


        const GraphicsPipelineDesc& pipeline_desc = pipeline->get_desc();
        
        _current_cmdlist->d3d12_cmdlist->SetGraphicsRootSignature(pipeline->d3d12_root_signature.Get());
        _current_cmdlist->d3d12_cmdlist->SetPipelineState(reinterpret_cast<ID3D12PipelineState*>(pipeline->get_native_object()));
        _current_cmdlist->d3d12_cmdlist->IASetPrimitiveTopology(convert_primitive_type(pipeline_desc.primitive_type, pipeline_desc.patch_control_points));

        
        uint8_t stencil_ref = 
            pipeline_desc.render_state.depth_stencil_state.dynamic_stencil_ref ? 
            _current_graphics_state.dynamic_stencil_ref_value : 
            pipeline_desc.render_state.depth_stencil_state.stencil_ref_value;

        if (pipeline_desc.render_state.depth_stencil_state.enable_stencil)
        {
            _current_cmdlist->d3d12_cmdlist->OMSetStencilRef(stencil_ref);
        }


        if (pipeline->use_blend_constant)
        {
            _current_cmdlist->d3d12_cmdlist->OMSetBlendFactor(&_current_graphics_state.blend_constant_color.r);
        }
    

        DX12FrameBuffer* frame_buffer = check_cast<DX12FrameBuffer*>(_current_graphics_state.frame_buffer);

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
        for (uint32_t view_index : frame_buffer->rtv_indices)
        {
            rtvs.emplace_back(_descriptor_manager->render_target_heap.get_cpu_handle(view_index));
        }

        D3D12_CPU_DESCRIPTOR_HANDLE dsv;
        bool has_depth_stencil = frame_buffer->dsv_index != INVALID_SIZE_32;
        if (has_depth_stencil) dsv = _descriptor_manager->depth_stencil_heap.get_cpu_handle(frame_buffer->dsv_index);

        _current_cmdlist->d3d12_cmdlist->OMSetRenderTargets(
            static_cast<uint32_t>(rtvs.size()),
            rtvs.data(),
            false,
            has_depth_stencil ? &dsv : nullptr
        );


        bind_graphics_binding_sets(_current_graphics_state.binding_sets, state.pipeline);


        if (_current_graphics_state.index_buffer_binding.is_valid())
        {
            
            auto& index_buffer = _current_graphics_state.index_buffer_binding.buffer;
            
            D3D12_INDEX_BUFFER_VIEW d3d12_index_buffer_view{};
            d3d12_index_buffer_view.Format = get_dxgi_format_mapping(_current_graphics_state.index_buffer_binding.format).srv_format;
            d3d12_index_buffer_view.BufferLocation = 
                reinterpret_cast<ID3D12Resource*>(index_buffer->get_native_object())->GetGPUVirtualAddress() + _current_graphics_state.index_buffer_binding.offset;
            d3d12_index_buffer_view.SizeInBytes = 
                static_cast<uint32_t>(index_buffer->get_desc().byte_size - _current_graphics_state.index_buffer_binding.offset);
            
            _current_cmdlist->d3d12_cmdlist->IASetIndexBuffer(&d3d12_index_buffer_view);
        }

        if (!_current_graphics_state.vertex_buffer_bindings.empty())
        {
            D3D12_VERTEX_BUFFER_VIEW d3d12_vertex_buffer_views[MAX_VERTEX_ATTRIBUTES];
            uint32_t max_vertex_buffer_index = 0;

            auto input_layout = check_cast<DX12InputLayout>(pipeline_desc.input_layout);

            for (const auto& binding : _current_graphics_state.vertex_buffer_bindings)
            {
                ReturnIfFalse(binding.slot <= MAX_VERTEX_ATTRIBUTES);
                
                d3d12_vertex_buffer_views[binding.slot].StrideInBytes = input_layout->slot_strides[binding.slot];
                d3d12_vertex_buffer_views[binding.slot].BufferLocation = 
                    reinterpret_cast<ID3D12Resource*>(binding.buffer->get_native_object())->GetGPUVirtualAddress() + binding.offset;
                d3d12_vertex_buffer_views[binding.slot].SizeInBytes = 
                    static_cast<uint32_t>(binding.buffer->get_desc().byte_size - binding.offset);

                max_vertex_buffer_index = std::max(max_vertex_buffer_index, binding.slot);
            }

            _current_cmdlist->d3d12_cmdlist->IASetVertexBuffers(0, max_vertex_buffer_index + 1, d3d12_vertex_buffer_views);
        }
        

        const FrameBufferInfo& frame_buffer_info = _current_graphics_state.frame_buffer->get_info();

        DX12ViewportState viewport_state = convert_viewport_state(pipeline_desc.render_state.raster_state, frame_buffer_info, _current_graphics_state.viewport_state);

        if (!viewport_state.viewports.empty())
        {
            _current_cmdlist->d3d12_cmdlist->RSSetViewports(viewport_state.viewports.size(), viewport_state.viewports.data());
        }

        if (!viewport_state.scissor_rects.empty())
        {
            _current_cmdlist->d3d12_cmdlist->RSSetScissorRects(viewport_state.scissor_rects.size(), viewport_state.scissor_rects.data());
        }

        return true;
    }

    void DX12CommandList::bind_compute_binding_sets(
        const PipelineStateBindingSetArray& binding_sets, 
        ComputePipelineInterface* pipeline
    )
    {
        for (uint32_t ix = 0; ix < binding_sets.size(); ++ix)
        {
            if (!binding_sets[ix]->is_bindless())
            {
                DX12BindingSet* binding_set = check_cast<DX12BindingSet*>(binding_sets[ix]);
                DX12BindingLayout* binding_layout = check_cast<DX12BindingLayout*>(pipeline->get_desc().binding_layouts[ix].get());

                for (uint32_t ix = 0; ix < binding_set->root_param_index_constant_buffer_map.size(); ++ix)
                {
                    uint32_t root_param_index = binding_set->root_param_index_constant_buffer_map[ix].first;
                    BufferInterface* constant_buffer = binding_set->root_param_index_constant_buffer_map[ix].second;

                    _current_cmdlist->d3d12_cmdlist->SetComputeRootConstantBufferView(
                        root_param_index, 
                        reinterpret_cast<ID3D12Resource*>(constant_buffer->get_native_object())->GetGPUVirtualAddress()
                    );
                }

                if (binding_set->is_descriptor_table_sampler_valid)
                {    
                    _current_cmdlist->d3d12_cmdlist->SetComputeRootDescriptorTable(
                        binding_layout->sampler_root_param_start_index,
                        _descriptor_manager->sampler_heap.get_gpu_handle(binding_set->sampler_view_start_index)
                    );
                }

                if (binding_set->is_descriptor_table_srv_etc_valid)
                {
                    _current_cmdlist->d3d12_cmdlist->SetComputeRootDescriptorTable(
                        binding_layout->srv_root_param_start_index,
                        _descriptor_manager->shader_resource_heap.get_gpu_handle(binding_set->srv_start_index)
                    );
                }
            }
            else
            {
                DX12BindlessSet* bindless_set = check_cast<DX12BindlessSet*>(binding_sets[ix]);
                DX12BindlessLayout* bindless_layout = check_cast<DX12BindlessLayout*>(pipeline->get_desc().binding_layouts[ix].get());

                _current_cmdlist->d3d12_cmdlist->SetComputeRootDescriptorTable(
                    bindless_layout->root_param_index, 
                    _descriptor_manager->shader_resource_heap.get_gpu_handle(bindless_set->first_descriptor_index)
                );
            }
        }
    }

    void DX12CommandList::bind_graphics_binding_sets(
        const PipelineStateBindingSetArray& binding_sets, 
        GraphicsPipelineInterface* pipeline
    )
    {
        for (uint32_t ix = 0; ix < binding_sets.size(); ++ix)
        {
            if (!binding_sets[ix]->is_bindless())
            {
                DX12BindingSet* binding_set = check_cast<DX12BindingSet*>(binding_sets[ix]);
                DX12BindingLayout* binding_layout = check_cast<DX12BindingLayout*>(pipeline->get_desc().binding_layouts[ix].get());

                for (uint32_t ix = 0; ix < binding_set->root_param_index_constant_buffer_map.size(); ++ix)
                {
                    uint32_t root_param_index = binding_set->root_param_index_constant_buffer_map[ix].first;
                    BufferInterface* constant_buffer = binding_set->root_param_index_constant_buffer_map[ix].second;

                    _current_cmdlist->d3d12_cmdlist->SetGraphicsRootConstantBufferView(
                        root_param_index, 
                        reinterpret_cast<ID3D12Resource*>(constant_buffer->get_native_object())->GetGPUVirtualAddress()
                    );
                }

                if (binding_set->is_descriptor_table_sampler_valid)
                {
                    _current_cmdlist->d3d12_cmdlist->SetGraphicsRootDescriptorTable(
                        binding_layout->sampler_root_param_start_index,
                        _descriptor_manager->sampler_heap.get_gpu_handle(binding_set->sampler_view_start_index)
                    );
                }

                if (binding_set->is_descriptor_table_srv_etc_valid)
                {
                    _current_cmdlist->d3d12_cmdlist->SetGraphicsRootDescriptorTable(
                        binding_layout->srv_root_param_start_index,
                        _descriptor_manager->shader_resource_heap.get_gpu_handle(binding_set->srv_start_index)
                    );
                }
            }
            else
            {
                DX12BindlessSet* bindless_set = check_cast<DX12BindlessSet*>(binding_sets[ix]);
                DX12BindlessLayout* bindless_layout = check_cast<DX12BindlessLayout*>(pipeline->get_desc().binding_layouts[ix].get());

                _current_cmdlist->d3d12_cmdlist->SetGraphicsRootDescriptorTable(
                    bindless_layout->root_param_index, 
                    _descriptor_manager->shader_resource_heap.get_gpu_handle(bindless_set->first_descriptor_index)
                );
            }
        }
    }

    void DX12CommandList::set_binding_resource_state(BindingSetInterface* binding_set_)
    {
        std::vector<BindingSetItem> binding_items;

        if (binding_set_->is_bindless())
        {
            const auto& bindings = check_cast<DX12BindlessSet*>(binding_set_)->binding_items;
            binding_items.insert(binding_items.end(), bindings.begin(), bindings.end());
        }
        else 
        {
            const auto& bindings = binding_set_->get_desc().binding_items;
            binding_items.insert(binding_items.end(), bindings.begin(), bindings.end());
        }

        for (const auto& binding : binding_items)
        {
            switch(binding.type)
            {
                case ResourceViewType::Texture_SRV:
                    switch (_desc.queue_type)
                    {
                    case CommandQueueType::Graphics:
                        set_texture_state(check_cast<TextureInterface>(binding.resource).get(), binding.subresource, ResourceStates::GraphicsShaderResource);
                        break;
                    case CommandQueueType::Compute:
                        set_texture_state(check_cast<TextureInterface>(binding.resource).get(), binding.subresource, ResourceStates::ComputeShaderResource);
                        break;
                    default: assert(!"Invalid enum");
                    }
                    break;

                case ResourceViewType::Texture_UAV:
                    set_texture_state(check_cast<TextureInterface>(binding.resource).get(), binding.subresource, ResourceStates::UnorderedAccess);
                    break;

                case ResourceViewType::TypedBuffer_SRV:
                case ResourceViewType::StructuredBuffer_SRV:
                case ResourceViewType::RawBuffer_SRV:
                    switch (_desc.queue_type)
                    {
                    case CommandQueueType::Graphics:
                        set_buffer_state(check_cast<BufferInterface>(binding.resource).get(), ResourceStates::GraphicsShaderResource);
                        break;
                    case CommandQueueType::Compute:
                        set_buffer_state(check_cast<BufferInterface>(binding.resource).get(), ResourceStates::ComputeShaderResource);
                        break;
                    default: assert(!"Invalid enum");
                    }
                    break;

                case ResourceViewType::TypedBuffer_UAV:
                case ResourceViewType::StructuredBuffer_UAV:
                case ResourceViewType::RawBuffer_UAV:
                    set_buffer_state(check_cast<BufferInterface>(binding.resource).get(), ResourceStates::UnorderedAccess);
                    break;
                default:
                    continue;
            }
        }
    }


    bool DX12CommandList::set_compute_state(const ComputeState& state)
    {
        _current_compute_state = state;

        DX12ComputePipeline* pipeline = check_cast<DX12ComputePipeline*>(_current_compute_state.pipeline);

        for (uint32_t ix = 0; ix < _current_compute_state.binding_sets.size(); ++ix)
        {
            ReturnIfFalse(_current_compute_state.binding_sets[ix]->get_layout() == pipeline->desc.binding_layouts[ix].get());

            set_binding_resource_state(_current_compute_state.binding_sets[ix]);
        }

        if (_current_compute_state.indirect_buffer)
        {
            set_buffer_state(_current_compute_state.indirect_buffer, ResourceStates::IndirectArgument);
        }

        commit_descriptor_heaps();
        commit_barriers();


        _current_cmdlist->d3d12_cmdlist->SetComputeRootSignature(pipeline->d3d12_root_signature.Get());
        _current_cmdlist->d3d12_cmdlist->SetPipelineState(reinterpret_cast<ID3D12PipelineState*>(_current_compute_state.pipeline->get_native_object()));

        bind_compute_binding_sets(_current_compute_state.binding_sets, state.pipeline);
        return true;
    }
    

    bool DX12CommandList::draw(
        const GraphicsState& state, 
        const DrawArguments& arguments, 
        const void* push_constant
    )
    {
        ReturnIfFalse(set_graphics_state(state));

        if (push_constant)
        {
            DX12GraphicsPipeline* pipeline = check_cast<DX12GraphicsPipeline*>(_current_graphics_state.pipeline);
            _current_cmdlist->d3d12_cmdlist->SetGraphicsRoot32BitConstants(
                pipeline->push_constant_root_param_index, 
                static_cast<uint32_t>(pipeline->push_constant_size / 4), 
                push_constant, 
                0
            );
        }

        _current_cmdlist->d3d12_cmdlist->DrawInstanced(
            arguments.index_count, 
            arguments.instance_count, 
            arguments.start_vertex_location, 
            arguments.start_instance_location
        );

        return true;
    }

    bool DX12CommandList::draw_indexed(
        const GraphicsState& state, 
        const DrawArguments& arguments, 
        const void* push_constant
    )
    {
        ReturnIfFalse(set_graphics_state(state));

        if (push_constant)
        {
            DX12GraphicsPipeline* pipeline = check_cast<DX12GraphicsPipeline*>(_current_graphics_state.pipeline);
            _current_cmdlist->d3d12_cmdlist->SetGraphicsRoot32BitConstants(
                pipeline->push_constant_root_param_index, 
                static_cast<uint32_t>(pipeline->push_constant_size / 4), 
                push_constant, 
                0
            );
        }

        _current_cmdlist->d3d12_cmdlist->DrawIndexedInstanced(
            arguments.index_count, 
            arguments.instance_count, 
            arguments.start_index_location, 
            arguments.start_vertex_location, 
            arguments.start_instance_location
        );

        return true;
    }

    bool DX12CommandList::dispatch(
        const ComputeState& state,
        uint32_t thread_group_num_x, 
        uint32_t thread_group_num_y,
        uint32_t thread_group_num_z,
        const void* push_constant
    )
    {
        ReturnIfFalse(set_compute_state(state));
        
        if (push_constant)
        {
            DX12ComputePipeline* pipeline = check_cast<DX12ComputePipeline*>(_current_compute_state.pipeline);
            _current_cmdlist->d3d12_cmdlist->SetComputeRoot32BitConstants(
                pipeline->push_constant_root_param_index, 
                static_cast<uint32_t>(pipeline->push_constant_size / 4), 
                push_constant, 
                0
            );
        }

        _current_cmdlist->d3d12_cmdlist->Dispatch(thread_group_num_x, thread_group_num_y, thread_group_num_z);
        return true;
    }

    bool DX12CommandList::draw_indirect(
        const GraphicsState& state, 
        uint32_t offset_bytes,
        uint32_t draw_count,
        const void* push_constant
    )
    {
        ReturnIfFalse(set_graphics_state(state));

        if (push_constant)
        {
            DX12GraphicsPipeline* pipeline = check_cast<DX12GraphicsPipeline*>(_current_graphics_state.pipeline);
            _current_cmdlist->d3d12_cmdlist->SetGraphicsRoot32BitConstants(
                pipeline->push_constant_root_param_index, 
                static_cast<uint32_t>(pipeline->push_constant_size / 4), 
                push_constant, 
                0
            );
        }

        _current_cmdlist->d3d12_cmdlist->ExecuteIndirect(
            _context->draw_indirect_signature.Get(), 
            draw_count, 
            reinterpret_cast<ID3D12Resource*>(state.indirect_buffer->get_native_object()), 
            offset_bytes, 
            nullptr, 
            0
        );
        return true;
    }

    bool DX12CommandList::draw_indexed_indirect(
        const GraphicsState& state, 
        uint32_t offset_bytes,
        uint32_t draw_count,
        const void* push_constant
    )
    {
        ReturnIfFalse(set_graphics_state(state));

        if (push_constant)
        {
            DX12GraphicsPipeline* pipeline = check_cast<DX12GraphicsPipeline*>(_current_graphics_state.pipeline);
            _current_cmdlist->d3d12_cmdlist->SetGraphicsRoot32BitConstants(
                pipeline->push_constant_root_param_index, 
                static_cast<uint32_t>(pipeline->push_constant_size / 4), 
                push_constant, 
                0
            );
        }

        _current_cmdlist->d3d12_cmdlist->ExecuteIndirect(
            _context->draw_indexed_indirect_signature.Get(), 
            draw_count, 
            reinterpret_cast<ID3D12Resource*>(state.indirect_buffer->get_native_object()), 
            offset_bytes, 
            nullptr, 
            0
        );
        return true;
    }
    
    bool DX12CommandList::dispatch_indirect(
        const ComputeState& state, 
        uint32_t offset_bytes,
        const void* push_constant
    )
    {
        ReturnIfFalse(set_compute_state(state));
        
        if (push_constant)
        {
            DX12ComputePipeline* pipeline = check_cast<DX12ComputePipeline*>(_current_compute_state.pipeline);
            _current_cmdlist->d3d12_cmdlist->SetComputeRoot32BitConstants(
                pipeline->push_constant_root_param_index, 
                static_cast<uint32_t>(pipeline->push_constant_size / 4), 
                push_constant, 
                0
            );
        }

        _current_cmdlist->d3d12_cmdlist->ExecuteIndirect(
            _context->draw_indexed_indirect_signature.Get(), 
            1, 
            reinterpret_cast<ID3D12Resource*>(state.indirect_buffer->get_native_object()), 
            offset_bytes, 
            nullptr, 
            0
        );
        return true;
    }

    void DX12CommandList::set_enable_uav_barrier_for_texture(TextureInterface* texture, bool enable_barriers)
    {
        _resource_state_tracker.set_texture_enable_uav_barriers(texture, enable_barriers);
    }

    void DX12CommandList::set_enable_uav_barrier_for_buffer(BufferInterface* buffer, bool enable_barriers)
    {
        _resource_state_tracker.set_buffer_enable_uav_barriers(buffer, enable_barriers);
    }

    void DX12CommandList::set_texture_state(TextureInterface* texture, const TextureSubresourceSet& subresource, ResourceStates states)
    {
        _resource_state_tracker.set_texture_state(texture, subresource, states);
    }

    void DX12CommandList::set_buffer_state(BufferInterface* buffer, ResourceStates states)
    {
        _resource_state_tracker.set_buffer_state(buffer, states);
    }

    void DX12CommandList::commit_barriers()
    {
        const auto& texture_barriers = _resource_state_tracker.get_texture_barriers();
        const auto& buffer_barriers = _resource_state_tracker.get_buffer_barriers();

        const uint64_t total_barrier_count = texture_barriers.size() + buffer_barriers.size();
        if (total_barrier_count == 0) return;

        for (const auto& barrier : texture_barriers)
        {
            const D3D12_RESOURCE_STATES d3d12_before_state = convert_resource_states(barrier.state_before);
            const D3D12_RESOURCE_STATES d3d12_after_state = convert_resource_states(barrier.state_after);
            
            ID3D12Resource* d3d12_resource = reinterpret_cast<ID3D12Resource*>(barrier.texture->get_native_object());

            D3D12_RESOURCE_BARRIER d3d12_barrier{};
            d3d12_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            if (d3d12_before_state != d3d12_after_state)
            {
                d3d12_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                d3d12_barrier.Transition.StateBefore = d3d12_before_state;
                d3d12_barrier.Transition.StateAfter = d3d12_after_state;
                d3d12_barrier.Transition.pResource = d3d12_resource;

                if (barrier.is_entire_texture)
                {
                    d3d12_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                    _d3d12_barriers.emplace_back(d3d12_barrier);
                }
                else 
                {
                    const auto& texture_desc = barrier.texture->get_desc();

                    d3d12_barrier.Transition.Subresource = calculate_texture_subresource(
                        barrier.mip_level, 
                        barrier.array_slice, 
                        texture_desc.mip_levels
                    );

                    _d3d12_barriers.emplace_back(d3d12_barrier);
                }
            }
            else if ((d3d12_after_state & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0)
            {
                d3d12_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                d3d12_barrier.UAV.pResource = d3d12_resource;

                _d3d12_barriers.emplace_back(d3d12_barrier);
            }
        }

        for (const auto& barrier : buffer_barriers)
        {
            const D3D12_RESOURCE_STATES d3d12_before_state = convert_resource_states(barrier.state_before);
            const D3D12_RESOURCE_STATES d3d12_after_state = convert_resource_states(barrier.state_after);
            
            D3D12_RESOURCE_BARRIER d3d12_barrier{};
            d3d12_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            ID3D12Resource* d3d12_resource = reinterpret_cast<ID3D12Resource*>(barrier.buffer->get_native_object());
            if (d3d12_before_state != d3d12_after_state)
            {
                d3d12_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                d3d12_barrier.Transition.StateBefore = d3d12_before_state;
                d3d12_barrier.Transition.StateAfter = d3d12_after_state;
                d3d12_barrier.Transition.pResource = d3d12_resource;
                d3d12_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                _d3d12_barriers.emplace_back(d3d12_barrier);
            }
            else if ((d3d12_after_state & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0)
            {
                d3d12_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                d3d12_barrier.UAV.pResource = d3d12_resource;

                _d3d12_barriers.emplace_back(d3d12_barrier);
            }
        }

        if (!_d3d12_barriers.empty())
        {
            _current_cmdlist->d3d12_cmdlist->ResourceBarrier(static_cast<uint32_t>(_d3d12_barriers.size()), _d3d12_barriers.data());
        }

        _resource_state_tracker.clear_barriers();
        _d3d12_barriers.clear();
    }

    ResourceStates DX12CommandList::get_buffer_state(BufferInterface* buffer)
    {
        return _resource_state_tracker.get_buffer_state(buffer);
    }
    ResourceStates DX12CommandList::get_texture_state(TextureInterface* texture, uint32_t array_slice, uint32_t mip_level)
    {
        return _resource_state_tracker.get_texture_state(texture, array_slice, mip_level);
    }

    const CommandListDesc& DX12CommandList::get_desc()
    {
        return _desc;
    }

    DeviceInterface* DX12CommandList::get_deivce()
    {
        return _device;
    }

    void* DX12CommandList::get_native_object()
    {
        return _current_cmdlist->d3d12_cmdlist.Get();
    }

    void DX12CommandList::executed(uint64_t submit_id)
    {
        _current_cmdlist->submit_id = submit_id;

        const uint64_t recording_id = _current_cmdlist->recording_id;

        _current_cmdlist = nullptr;
        
        _upload_manager.submit_chunks(
            make_version(recording_id, _desc.queue_type, false),
            make_version(submit_id, _desc.queue_type, true)
        );
        
        _scratch_manager.submit_chunks(
            make_version(recording_id, _desc.queue_type, false),
            make_version(submit_id, _desc.queue_type, true)
        );
    }

    std::shared_ptr<DX12InternalCommandList> DX12CommandList::get_current_command_list()
    {
        return _current_cmdlist;
    }


    // ray_tracing::DX12ShaderTableState* DX12CommandList::get_shader_tabel_state(ray_tracing::ShaderTableInterface* shader_table)
    // {
    //     auto iter = _shader_table_states.find(shader_table);
    //     if (iter != _shader_table_states.end())
    //     {
    //         return iter->second.get();
    //     }

    //     std::unique_ptr<ray_tracing::DX12ShaderTableState> shader_table_state = std::make_unique<ray_tracing::DX12ShaderTableState>();
    //     ray_tracing::DX12ShaderTableState* ret = shader_table_state.get();

    //     _shader_table_states.insert(std::make_pair(shader_table, std::move(shader_table_state)));
    //     return ret;
    // }
    
    // bool DX12CommandList::build_bottom_level_accel_struct(
    //     ray_tracing::AccelStructInterface* accel_struct,
    //     const ray_tracing::GeometryDesc* geometry_descs,
    //     uint32_t geometry_desc_count
    // )
    // {
    //     auto& desc = check_cast<ray_tracing::DX12AccelStruct*>(accel_struct)->_desc;
    //     ReturnIfFalse(!desc.is_top_level);

    //     bool preform_update = (desc.flags & ray_tracing::AccelStructBuildFlags::PerformUpdate) != 0;

    //     desc.bottom_level_geometry_descs.clear();
    //     desc.bottom_level_geometry_descs.reserve(geometry_desc_count);
    //     for (uint32_t ix = 0; ix < geometry_desc_count; ++ix)
    //     {
    //         const auto& geometry_desc = geometry_descs[ix];
    //         desc.bottom_level_geometry_descs[ix] = geometry_desc;

    //         if (geometry_desc.type == ray_tracing::GeometryType::Triangle)
    //         {
    //             ReturnIfFalse(set_buffer_state(geometry_desc.triangles.vertex_buffer.get(), ResourceStates::AccelStructBuildInput));
    //             ReturnIfFalse(set_buffer_state(geometry_desc.triangles.index_buffer.get(), ResourceStates::AccelStructBuildInput));

    //             _cmdlist_ref_instances->ref_resources.push_back(geometry_desc.triangles.vertex_buffer.get());
    //             _cmdlist_ref_instances->ref_resources.push_back(geometry_desc.triangles.index_buffer.get());
    //         }
    //         else 
    //         {
    //             ReturnIfFalse(set_buffer_state(geometry_desc.aabbs.buffer.get(), ResourceStates::AccelStructBuildInput));
    //             _cmdlist_ref_instances->ref_resources.push_back(geometry_desc.aabbs.buffer.get());
    //         }

    //     }

    //     commit_barriers();

    //     ray_tracing::DX12AccelStructBuildInputs dx12_build_inputs;
    //     dx12_build_inputs.type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    //     dx12_build_inputs.flags = ray_tracing::convert_accel_struct_build_flags(desc.flags);
    //     dx12_build_inputs.desc_num = static_cast<uint32_t>(desc.bottom_level_geometry_descs.size());
    //     dx12_build_inputs.descs_layout = D3D12_ELEMENTS_LAYOUT_ARRAY_OF_POINTERS;
    //     dx12_build_inputs.geometry_descs.resize(desc.bottom_level_geometry_descs.size());
    //     dx12_build_inputs.geometry_desc_ptrs.resize(desc.bottom_level_geometry_descs.size());
    //     for (uint32_t ix = 0; ix < static_cast<uint32_t>(dx12_build_inputs.geometry_descs.size()); ++ix)
    //     {
    //         dx12_build_inputs.geometry_desc_ptrs[ix] = dx12_build_inputs.geometry_descs.data() + ix;
    //     }
    //     dx12_build_inputs.cpcpGeometryDesc = dx12_build_inputs.geometry_desc_ptrs.data();


    //     dx12_build_inputs.geometry_descs.resize(geometry_desc_count);
    //     for (uint32_t ix = 0; ix < static_cast<uint32_t>(desc.bottom_level_geometry_descs.size()); ++ix)
    //     {
    //         const auto& geometry_desc = desc.bottom_level_geometry_descs[ix];
    //         D3D12_GPU_VIRTUAL_ADDRESS gpu_address = 0;
    //         if (geometry_desc.use_transform)
    //         {
    //             uint8_t* cpu_address = nullptr;
    //             ReturnIfFalse(!_upload_manager.suballocate_buffer(
    //                 sizeof(float3x4), 
    //                 nullptr, 
    //                 nullptr, 
    //                 &cpu_address, 
    //                 &gpu_address, 
    //                 _recording_version, 
    //                 D3D12_RAYTRACING_TRANSFORM3X4_BYTE_ALIGNMENT
    //             ));

    //             memcpy(cpu_address, &geometry_desc.affine_matrix, sizeof(float3x4));
    //         }

    //         dx12_build_inputs.geometry_descs[ix] = ray_tracing::DX12AccelStruct::convert_geometry_desc(geometry_desc, gpu_address);
    //     }

    //     ray_tracing::DX12AccelStruct* dx12_accel_struct = check_cast<ray_tracing::DX12AccelStruct*>(accel_struct);
    //     D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO accel_struct_prebuild_info = dx12_accel_struct->get_accel_struct_prebuild_info();

    //     ReturnIfFalse(accel_struct_prebuild_info.ResultDataMaxSizeInBytes <= dx12_accel_struct->get_buffer()->get_desc().byte_size);

    //     uint64_t scratch_size = preform_update ? 
    //         accel_struct_prebuild_info.UpdateScratchDataSizeInBytes :
    //         accel_struct_prebuild_info.ScratchDataSizeInBytes;

    //     D3D12_GPU_VIRTUAL_ADDRESS scratch_gpu_address{};
    //     ReturnIfFalse(_dx12_scratch_manager.suballocate_buffer(
    //         scratch_size, 
    //         nullptr, 
    //         nullptr, 
    //         nullptr, 
    //         &scratch_gpu_address, 
    //         _recording_version, 
    //         D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT,
    //         active_cmdlist->d3d12_cmdlist.Get()
    //     ));

    //     set_buffer_state(dx12_accel_struct->get_buffer(), ResourceStates::AccelStructWrite);
    //     commit_barriers();

    //     D3D12_GPU_VIRTUAL_ADDRESS accel_struct_data_address = 
    //         reinterpret_cast<ID3D12Resource*>(dx12_accel_struct->get_buffer()->get_native_object())->GetGPUVirtualAddress();

    //     D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC accel_struct_build_desc = {};
    //     accel_struct_build_desc.Inputs = dx12_build_inputs.Convert();
    //     accel_struct_build_desc.ScratchAccelerationStructureData = scratch_gpu_address;
    //     accel_struct_build_desc.DestAccelerationStructureData = accel_struct_data_address;
    //     accel_struct_build_desc.SourceAccelerationStructureData = preform_update ? accel_struct_data_address : 0;
    //     active_cmdlist->d3d12_cmdlist4->BuildRaytracingAccelerationStructure(&accel_struct_build_desc, 0, nullptr);
    //     _cmdlist_ref_instances->ref_resources.push_back(accel_struct);

    //     return true;
    // }

    // bool DX12CommandList::build_top_level_accel_struct(
    //     ray_tracing::AccelStructInterface* accel_struct, 
    //     const ray_tracing::InstanceDesc* instance_descs, 
    //     uint32_t instance_count
    // )
    // {
    //     ray_tracing::DX12AccelStruct* dx12_accel_struct = check_cast<ray_tracing::DX12AccelStruct*>(accel_struct);
    //     const auto& desc = dx12_accel_struct->get_desc();
    //     ReturnIfFalse(desc.is_top_level);

    //     BufferInterface* accel_struct_buffer = dx12_accel_struct->get_buffer();

    //     dx12_accel_struct->d3d12_ray_tracing_instance_descs.resize(instance_count);
    //     dx12_accel_struct->bottom_level_accel_structs.clear();
        
    //     for (uint32_t ix = 0; ix < instance_count; ++ix)
    //     {
    //         const auto& instance_desc = instance_descs[ix];
    //         auto& d3d12_instance_desc = dx12_accel_struct->d3d12_ray_tracing_instance_descs[ix];

    //         if (instance_desc.bottom_level_accel_struct)
    //         {
    //             dx12_accel_struct->bottom_level_accel_structs.emplace_back(instance_desc.bottom_level_accel_struct);
    //             ray_tracing::DX12AccelStruct* dx12_blas = check_cast<ray_tracing::DX12AccelStruct*>(instance_desc.bottom_level_accel_struct);
    //             BufferInterface* blas_buffer = dx12_blas->get_buffer();

    //             d3d12_instance_desc = ray_tracing::convert_instance_desc(instance_desc);
    //             d3d12_instance_desc.AccelerationStructure = 
    //                 reinterpret_cast<ID3D12Resource*>(blas_buffer->get_native_object())->GetGPUVirtualAddress();
    //             ReturnIfFalse(set_buffer_state(blas_buffer, ResourceStates::AccelStructBuildBlas));
    //         }
    //         else 
    //         {
    //             d3d12_instance_desc.AccelerationStructure = D3D12_GPU_VIRTUAL_ADDRESS{0};
    //         }
    //     }

    //     uint64_t upload_size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * dx12_accel_struct->d3d12_ray_tracing_instance_descs.size();
    //     D3D12_GPU_VIRTUAL_ADDRESS gpu_address{};
    //     uint8_t* cpu_address = nullptr;
    //     ReturnIfFalse(_upload_manager.suballocate_buffer(
    //         upload_size, 
    //         nullptr, 
    //         nullptr, 
    //         &cpu_address, 
    //         &gpu_address, 
    //         _recording_version, 
    //         D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
    //     ));

    //     memcpy(cpu_address, dx12_accel_struct->d3d12_ray_tracing_instance_descs.data(), upload_size);

    //     ReturnIfFalse(set_buffer_state(accel_struct_buffer, ResourceStates::AccelStructWrite));
    //     commit_barriers();

    //     bool perform_update = (desc.flags & ray_tracing::AccelStructBuildFlags::AllowUpdate) != 0;

    //     D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS d3d12_accel_struct_inputs;
    //     d3d12_accel_struct_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    //     d3d12_accel_struct_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    //     d3d12_accel_struct_inputs.InstanceDescs = gpu_address;
    //     d3d12_accel_struct_inputs.NumDescs = instance_count;
    //     d3d12_accel_struct_inputs.Flags = ray_tracing::convert_accel_struct_build_flags(desc.flags);
    //     if (perform_update) d3d12_accel_struct_inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

    //     D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
    //     _context->device5->GetRaytracingAccelerationStructurePrebuildInfo(&d3d12_accel_struct_inputs, &ASPreBuildInfo);

    //     ReturnIfFalse(ASPreBuildInfo.ResultDataMaxSizeInBytes <= accel_struct_buffer->get_desc().byte_size);

    //     uint64_t stScratchSize = perform_update ? 
    //         ASPreBuildInfo.UpdateScratchDataSizeInBytes :
    //         ASPreBuildInfo.ScratchDataSizeInBytes;

    //     D3D12_GPU_VIRTUAL_ADDRESS d3d12_scratch_gpu_address{};
    //     ReturnIfFalse(_dx12_scratch_manager.suballocate_buffer(
    //         stScratchSize, 
    //         nullptr, 
    //         nullptr, 
    //         nullptr, 
    //         &d3d12_scratch_gpu_address, 
    //         _recording_version, 
    //         D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT,
    //         active_cmdlist->d3d12_cmdlist.Get()
    //     ));


    //     D3D12_GPU_VIRTUAL_ADDRESS d3d12_accel_struct_data_address = 
    //         reinterpret_cast<ID3D12Resource*>(accel_struct_buffer->get_native_object())->GetGPUVirtualAddress();

    //     D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC d3d12_build_as_desc = {};
    //     d3d12_build_as_desc.Inputs = d3d12_accel_struct_inputs;
    //     d3d12_build_as_desc.ScratchAccelerationStructureData = d3d12_scratch_gpu_address;
    //     d3d12_build_as_desc.DestAccelerationStructureData = d3d12_accel_struct_data_address;
    //     d3d12_build_as_desc.SourceAccelerationStructureData = perform_update ? d3d12_accel_struct_data_address : 0;

    //     active_cmdlist->d3d12_cmdlist4->BuildRaytracingAccelerationStructure(&d3d12_build_as_desc, 0, nullptr);
    //     _cmdlist_ref_instances->ref_resources.push_back(accel_struct);

    //     return true;
    // }

    // bool DX12CommandList::set_accel_struct_state(ray_tracing::AccelStructInterface* accel_struct, ResourceStates state)
    // {
    //     ReturnIfFalse(_resource_state_tracker.set_buffer_state(check_cast<ray_tracing::DX12AccelStruct*>(accel_struct)->get_buffer(), state));
        
    //     if (_cmdlist_ref_instances != nullptr)
    //     {
    //         _cmdlist_ref_instances->ref_resources.emplace_back(accel_struct);
    //     }
    //     return true;
    // }

    // bool DX12CommandList::set_ray_tracing_state(const ray_tracing::PipelineState& state)
    // {
    //     ray_tracing::DX12ShaderTable* shader_table = check_cast<ray_tracing::DX12ShaderTable*>(state.shader_table);
    //     ray_tracing::DX12Pipeline* pipeline = check_cast<ray_tracing::DX12Pipeline*>(shader_table->get_pipeline());
    //     ray_tracing::DX12ShaderTableState* shader_table_state = get_shaderTableState(shader_table);

    //     bool rebuild_shader_table = shader_table_state->committed_version != shader_table->_version ||
    //         shader_table_state->d3d12_descriptor_heap_srv != _descriptor_manager->shader_resource_heap.get_shader_visible_heap() ||
    //         shader_table_state->d3d12_descriptor_heap_samplers != _descriptor_manager->sampler_heap.get_shader_visible_heap();

    //     if (rebuild_shader_table)
    //     {
    //         uint32_t entry_size = pipeline->get_shaderTableEntrySize();
    //         uint8_t* cpu_address;
    //         D3D12_GPU_VIRTUAL_ADDRESS gpu_address;
    //         ReturnIfFalse(_upload_manager.suballocate_buffer(
    //             entry_size * shader_table->get_entry_count(), 
    //             nullptr, 
    //             nullptr, 
    //             &cpu_address, 
    //             &gpu_address, 
    //             _recording_version, 
    //             D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT
    //         ));

    //         uint32_t entry_index = 0;

    //         auto WriteEntry = [&](const ray_tracing::DX12ShaderTable::ShaderEntry& entry) 
    //         {
    //             memcpy(cpu_address, entry.shader_identifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    //             if (entry.binding_set)
    //             {
    //                 DX12BindingSet* bindingSet = check_cast<DX12BindingSet*>(entry.binding_set);
    //                 DX12BindingLayout* layout = check_cast<DX12BindingLayout*>(bindingSet->get_layout());

    //                 if (layout->descriptor_table_sampler_size > 0)
    //                 {
    //                     auto table = reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(
    //                         cpu_address + 
    //                         D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 
    //                         layout->sampler_root_param_start_index * sizeof(D3D12_GPU_DESCRIPTOR_HANDLE)
    //                     );
    //                     *table = _descriptor_manager->sampler_heap.get_gpu_handle(bindingSet->sampler_view_start_index);
    //                 }

    //                 if (layout->descriptor_table_srv_etc_size > 0)
    //                 {
    //                     auto table = reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(
    //                         cpu_address + 
    //                         D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 
    //                         layout->srv_root_param_start_index * sizeof(D3D12_GPU_DESCRIPTOR_HANDLE)
    //                     );
    //                     *table = _descriptor_manager->shader_resource_heap.get_gpu_handle(bindingSet->srv_start_index);
    //                 }

    //                 ReturnIfFalse(layout->root_param_index_volatile_cb_descriptor_map.empty());
    //             }

    //             cpu_address += entry_size;
    //             gpu_address += entry_size;
    //             entry_index += 1;

    //             return true;
    //         };

    //         D3D12_DISPATCH_RAYS_DESC& d3d12_dispatch_rays_desc = shader_table_state->d3d12_dispatch_rays_desc;
    //         memset(&d3d12_dispatch_rays_desc, 0, sizeof(D3D12_DISPATCH_RAYS_DESC));

    //         d3d12_dispatch_rays_desc.RayGenerationShaderRecord.StartAddress = gpu_address;
    //         d3d12_dispatch_rays_desc.RayGenerationShaderRecord.SizeInBytes = entry_size;
    //         WriteEntry(shader_table->_raygen_shader);

    //         if (!shader_table->_miss_shaders.empty())
    //         {
    //             d3d12_dispatch_rays_desc.MissShaderTable.StartAddress = gpu_address;
    //             d3d12_dispatch_rays_desc.MissShaderTable.SizeInBytes = entry_size * static_cast<uint32_t>(shader_table->_miss_shaders.size());
    //             d3d12_dispatch_rays_desc.MissShaderTable.StrideInBytes = shader_table->_miss_shaders.size() == 1 ? 0 : entry_size;
    //             for (const auto& entry : shader_table->_miss_shaders) WriteEntry(entry);
    //         }
    //         if (!shader_table->_hit_groups.empty())
    //         {
    //             d3d12_dispatch_rays_desc.HitGroupTable.StartAddress = gpu_address;
    //             d3d12_dispatch_rays_desc.HitGroupTable.SizeInBytes = entry_size * static_cast<uint32_t>(shader_table->_hit_groups.size());
    //             d3d12_dispatch_rays_desc.HitGroupTable.StrideInBytes = shader_table->_hit_groups.size() == 1 ? 0 : entry_size;
    //             for (const auto& entry : shader_table->_miss_shaders) WriteEntry(entry);
    //         }
    //         if (!shader_table->_callable_shaders.empty())
    //         {
    //             d3d12_dispatch_rays_desc.CallableShaderTable.StartAddress = gpu_address;
    //             d3d12_dispatch_rays_desc.CallableShaderTable.SizeInBytes = entry_size * static_cast<uint32_t>(shader_table->_callable_shaders.size());
    //             d3d12_dispatch_rays_desc.CallableShaderTable.StrideInBytes = shader_table->_callable_shaders.size() == 1 ? 0 : entry_size;
    //             for (const auto& entry : shader_table->_miss_shaders) WriteEntry(entry);
    //         }

    //         shader_table_state->committed_version = shader_table->_version;
    //         shader_table_state->d3d12_descriptor_heap_srv = _descriptor_manager->shader_resource_heap.get_shader_visible_heap();
    //         shader_table_state->d3d12_descriptor_heap_samplers = _descriptor_manager->sampler_heap.get_shader_visible_heap();
            
    //         _cmdlist_ref_instances->ref_resources.push_back(shader_table);
    //     }

        
    //     ray_tracing::DX12Pipeline* current_pipeline = 
    //         _current_ray_tracing_state.shader_table ?
    //         check_cast<ray_tracing::DX12Pipeline*>(_current_ray_tracing_state.shader_table->get_pipeline()) : nullptr;
            
    //     const bool update_root_signature = 
    //         !_current_ray_tracing_state_valid || 
    //         _current_ray_tracing_state.shader_table == nullptr ||
    //         current_pipeline->_global_root_signature != pipeline->_global_root_signature;

        
    //     uint32_t binding_update_mask = 0;     // 按位判断 bindingset 数组中哪一个 bindingset 需要更新绑定

    //     if (!update_root_signature) binding_update_mask = ~0u;

    //     if (commit_descriptor_heaps()) binding_update_mask = ~0u;

    //     if (binding_update_mask == 0)
    //     {
    //         binding_update_mask = find_array_different_bits(
    //             _current_graphics_state.binding_sets, 
    //             _current_graphics_state.binding_sets.size(), 
    //             state.binding_sets, 
    //             state.binding_sets.size()
    //         );
    //     } 

    //     if (update_root_signature) 
    //     {
    //         active_cmdlist->d3d12_cmdlist4->SetComputeRootSignature(pipeline->_global_root_signature->d3d12_root_signature.Get());
    //     }

    //     bool update_pipeline = !_current_ray_tracing_state_valid || current_pipeline != pipeline;

    //     if (update_pipeline)
    //     {
    //         active_cmdlist->d3d12_cmdlist4->SetPipelineState1(reinterpret_cast<ID3D12StateObject*>(pipeline->get_native_object()));
    //         _cmdlist_ref_instances->ref_resources.push_back(pipeline);
    //     }

    //     set_compute_bindings(state.binding_sets, binding_update_mask, nullptr, false, pipeline->_global_root_signature.get());

    //     _current_graphics_state_valid = false;
    //     _current_compute_state_valid = false;
    //     _current_ray_tracing_state_valid = true;
    //     _current_ray_tracing_state = state;
    //     return true;
    // }


    // bool DX12CommandList::dispatch_rays(const ray_tracing::DispatchRaysArguments& arguments)
    // {
    //     ReturnIfFalse(_current_ray_tracing_state_valid && update_compute_volatile_buffers());

    //     D3D12_DISPATCH_RAYS_DESC desc = get_shaderTableState(_current_ray_tracing_state.shader_table)->d3d12_dispatch_rays_desc;
    //     desc.Width = arguments.width;
    //     desc.Height = arguments.height;
    //     desc.Depth = arguments.depth;

    //     active_cmdlist->d3d12_cmdlist4->DispatchRays(&desc);
    //     return true;
    // }

    
}
