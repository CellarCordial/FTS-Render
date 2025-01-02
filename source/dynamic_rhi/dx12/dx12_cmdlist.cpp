#include "dx12_cmdlist.h"
#include "../../core/tools/check_cast.h"
#include <climits>
#include <combaseapi.h>
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <intsafe.h>
#include <memory>
#include <minwindef.h>
#include <pix_win.h>
#include <utility>

#include "dx12_converts.h"
#include "dx12_device.h"
#include "dx12_frameBuffer.h"
#include "dx12_pipeline.h"
#include "dx12_ray_tracing.h"
#include "dx12_resource.h"

namespace fantasy 
{
    constexpr uint64_t version_submitted_flag = 0x8000000000000000;
    constexpr uint32_t version_queue_shift = 60;
    constexpr uint64_t version_id_mask = 0x0FFFFFFFFFFFFFFF;
    constexpr uint64_t make_version(uint64_t id, CommandQueueType queue_type, bool submitted)
    {
        uint64_t result = (id & version_id_mask) | (uint64_t(queue_type) << version_queue_shift);
        if (submitted) result |= version_submitted_flag;
        return result;
    }
    constexpr uint64_t version_get_instance(uint64_t version)
    {
        return version & version_id_mask;
    }

    constexpr bool version_get_submitted(uint64_t version)
    {
        return (version & version_submitted_flag) != 0;
    }

    template<typename T, typename U> 
    inline uint32_t find_array_different_bits(const T& array0, uint32_t size0, const U& array1, uint32_t size1)
    {
        assert(size0 <= 32);
        assert(size1 <= 32);

        if (size0 != size1) return ~0u;

        uint32_t mask = 0;
        for (uint32_t i = 0; i < size0; i++)
        {
            if (array0[i] != array1[i]) mask |= (1 << i);
        }

        return mask;
    }



    template <class T>
    concept StackArrayType =requires(T t) { t.size(); t[0]; };

    template <class StackArrayType>
    inline bool is_same_arrays(const StackArrayType& array0, const StackArrayType& array1)
    {
        if (array0.size() != array1.size()) return false;
        
        for (uint32_t ix = 0; ix < array0.size(); ++ix)
        {
            if (array0[ix] != (array1[ix])) return false;
        }
        
        return true;
    }

    

    DX12UploadManager::DX12UploadManager(
        const DX12Context* context, 
        DX12CommandQueue* cmd_queue, 
        uint64_t default_chunk_size, 
        uint64_t memory_limit,
        bool dxr_scratch
    ) :
        _context(context), 
        _cmd_queue(cmd_queue), 
        _default_chunk_size(default_chunk_size), 
        _max_memory_size(memory_limit),
        _allocated_memory_size(0),
        _dxr_scratch(dxr_scratch)
    {
        if (!_cmd_queue)
        {
            LOG_ERROR("UploadManager initialize failed for nullptr cmdqueue.");
            assert(_cmd_queue != nullptr);
        }
    }
    

    bool DX12UploadManager::suballocate_buffer(
        uint64_t size, 
        ID3D12Resource** d3d12_buffer, 
        uint64_t* offest, 
        uint8_t** cpu_address, 
        D3D12_GPU_VIRTUAL_ADDRESS* gpu_address, 
        uint64_t current_version, 
        uint32_t aligment,
        ID3D12GraphicsCommandList* d3d12_cmdlist
    )
    {
        // DxrScratch upload manager need d3d12 cmdlist to set uav barrier.
        ReturnIfFalse(!_dxr_scratch || d3d12_cmdlist);

        std::shared_ptr<DX12BufferChunk> buffer_chunk_to_retire;

        // Try to allocate from the current chunk first.
        if (_current_chunk != nullptr)
        {
            uint64_t aligned_offset = align(_current_chunk->write_end_position, static_cast<uint64_t>(aligment));
            uint64_t data_end_pos = aligned_offset + size;

            if (data_end_pos <= _current_chunk->buffer_size)
            {
                // The buffer can fit into the current chunk. 
                _current_chunk->write_end_position = data_end_pos;

                if (d3d12_buffer != nullptr) *d3d12_buffer = _current_chunk->d3d12_buffer.Get();
                if (offest != nullptr) *offest = aligned_offset;
                if (cpu_address != nullptr) *cpu_address = static_cast<uint8_t*>(_current_chunk->cpu_address) + aligned_offset;
                if (gpu_address != nullptr) *gpu_address = _current_chunk->d3d12_buffer->GetGPUVirtualAddress() + aligned_offset;

                return true;
            }

            buffer_chunk_to_retire = _current_chunk;
            _current_chunk.reset();
        }

        uint64_t last_compelete_value = _cmd_queue->last_compeleted_value;


        for (auto it = _chunk_pool.begin(); it != _chunk_pool.end(); ++it)
        {
            std::shared_ptr<DX12BufferChunk> chunk = *it;
            
            if (version_get_submitted(chunk->version) && version_get_instance(chunk->version) <= last_compelete_value)
            {
                chunk->version = 0;
            }

            // If this chunk has submitted and size-fit. 
            if (chunk->version == 0 && chunk->buffer_size >= size)
            {
                _current_chunk = chunk;
                _chunk_pool.erase(it);
                break;
            }
        }

        if (buffer_chunk_to_retire)
        {
            _chunk_pool.push_back(buffer_chunk_to_retire);
        }

        if (!_current_chunk)
        {
            uint64_t size_to_allocate = align(std::max(size, _default_chunk_size), DX12BufferChunk::size_alignment);
            if (_max_memory_size > 0 && _allocated_memory_size + size_to_allocate <= _max_memory_size)
            {
                if (_dxr_scratch)
                {
                    std::shared_ptr<DX12BufferChunk> best_chunk;
                    for (const auto& crChunk : _chunk_pool)
                    {
                        if (crChunk->buffer_size >= size_to_allocate)
                        {
                            if (!best_chunk)
                            {
                                best_chunk = crChunk;
                                continue;
                            }

                            bool chunk_submitted = version_get_submitted(crChunk->version);
                            bool best_chunk_submitted = version_get_submitted(best_chunk->version);
                            uint64_t chunk_instance = version_get_instance(crChunk->version);
                            uint64_t best_chunk_instance = version_get_instance(best_chunk->version);

                            if (
                                chunk_submitted && !best_chunk_submitted ||
                                chunk_submitted == best_chunk_submitted && 
                                chunk_instance < best_chunk_instance ||
                                chunk_submitted == best_chunk_submitted && 
                                chunk_instance == best_chunk_instance && 
                                crChunk->buffer_size > best_chunk->buffer_size
                            )
                            {
                                best_chunk = crChunk;
                            }
                        }
                    }
                    ReturnIfFalse(best_chunk != nullptr);

                    _chunk_pool.erase(std::find(_chunk_pool.begin(), _chunk_pool.end(), best_chunk));
                    _current_chunk = best_chunk;

                    D3D12_RESOURCE_BARRIER uav_barrier = {};
                    uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                    uav_barrier.UAV.pResource = best_chunk->d3d12_buffer.Get();
                    d3d12_cmdlist->ResourceBarrier(1, &uav_barrier);
                }
                else 
                {
                    LOG_ERROR("No memory limit to upload resource.");
                    return false;
                }
            }
            else 
            {
                _current_chunk = create_bufferChunk(size_to_allocate);
            }
        }

        // 从 ChunkPool 中掏出来的 chunk 相当于重置了, 和新创建的一样 
        _current_chunk->version = current_version;
        _current_chunk->write_end_position = size;

        if (d3d12_buffer != nullptr) *d3d12_buffer = _current_chunk->d3d12_buffer.Get();
        if (offest != nullptr)       *offest       = 0;
        if (cpu_address != nullptr)  *cpu_address  = static_cast<uint8_t*>(_current_chunk->cpu_address);
        if (gpu_address != nullptr)  *gpu_address  = _current_chunk->d3d12_buffer->GetGPUVirtualAddress();

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

    std::shared_ptr<DX12BufferChunk> DX12UploadManager::create_bufferChunk(uint64_t size) const
    {
        std::shared_ptr<DX12BufferChunk> ret = std::make_shared<DX12BufferChunk>();
        size = align(size, DX12BufferChunk::size_alignment);

        D3D12_HEAP_PROPERTIES d3d12_heap_properties{};
        d3d12_heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT res = _context->device->CreateCommittedResource(
            &d3d12_heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(ret->d3d12_buffer.GetAddressOf())
        );
        if (FAILED(res)) return nullptr;

        res = ret->d3d12_buffer->Map(0, nullptr, &ret->cpu_address);
        if (FAILED(res)) return nullptr;

        ret->buffer_size = size;
        ret->gpu_address = ret->d3d12_buffer->GetGPUVirtualAddress();
        ret->index_in_pool = static_cast<uint32_t>(_chunk_pool.size());

        return ret;
    }


    DX12CommandList::DX12CommandList(
        const DX12Context* context,
        DX12DescriptorHeaps* descriptor_heaps,
        DeviceInterface* device,
        DX12CommandQueue* dx12_cmd_queue,
        const CommandListDesc& desc
    ) :
        _context(context), 
        _descriptor_heaps(descriptor_heaps),
        _device(device),
        _cmd_queue(dx12_cmd_queue),
        _desc(desc),
        _upload_manager(_context, _cmd_queue, desc.upload_chunk_size, 0),
        _dx12_scratch_manager(_context, _cmd_queue, desc.scratch_chunk_size, desc.scratch_max_mamory, true)
    {
    }

    bool DX12CommandList::initialize()
    {
        return true;
    }

    bool DX12CommandList::open()
    {
        uint64_t stCompletedValue = _cmd_queue->update_last_completed_value();

        std::shared_ptr<DX12InternalCommandList> cmdlist;

        if (!_cmdlist_pool.empty())
        {
            cmdlist = _cmdlist_pool.front();

            if (cmdlist->last_submitted_value <= stCompletedValue)
            {
                cmdlist->d3d12_cmd_allocator->Reset();
                cmdlist->d3d12_cmdlist->Reset(cmdlist->d3d12_cmd_allocator.Get(), nullptr);
                _cmdlist_pool.pop_front();
            }
            else 
            {
                cmdlist = nullptr;
            }
        }
        
        if (!cmdlist) cmdlist = create_internal_cmdlist();

        active_cmdlist = cmdlist;

        _cmdlist_ref_instances = std::make_shared<DX12CommandListInstance>();
        _cmdlist_ref_instances->d3d12_cmdlist = active_cmdlist->d3d12_cmdlist;
        _cmdlist_ref_instances->d3d12_cmd_allocator = active_cmdlist->d3d12_cmd_allocator;
        _cmdlist_ref_instances->cmd_queue_type = _desc.queue_type;

        _recording_version = make_version(_cmd_queue->redording_version++, _desc.queue_type, false);

        return true;
    }
    
    bool DX12CommandList::close()
    {
        commit_barriers();

        active_cmdlist->d3d12_cmdlist->Close();

        clear_state_cache();

        _current_upload_buffer = nullptr;
        _volatile_constant_buffer_addresses.clear();
        _shader_table_states.clear();

        return true;
    }

    bool DX12CommandList::clear_state()
    {
        active_cmdlist->d3d12_cmdlist->ClearState(nullptr);
        clear_state_cache();
        ReturnIfFalse(commit_descriptor_heaps());
        return true; 
    }

    bool DX12CommandList::clear_texture_float(TextureInterface* texture, const TextureSubresourceSet& subresource_set, const Color& clear_color)
    {
        DX12Texture* dx12_texture = check_cast<DX12Texture*>(texture);
        const auto& texture_desc = dx12_texture->get_desc();

        const FormatInfo& crFormatInfo = get_format_info(texture_desc.format);
        if (
            crFormatInfo.has_stencil || crFormatInfo.has_depth || 
            (!texture_desc.is_render_target && !texture_desc.is_uav)
        )
        {
            LOG_ERROR("This function can't use on the depth stenicl texture. require the texture is render target or unordered access. ");
            return false;
        }

        TextureSubresourceSet subresource = subresource_set.resolve(texture_desc, false);
        
        _cmdlist_ref_instances->ref_resources.emplace_back(texture);

        if (texture_desc.is_render_target)
        {
            ReturnIfFalse(set_texture_state(texture, subresource, ResourceStates::RenderTarget));

            commit_barriers();

            for (uint32_t ix = subresource.base_mip_level; ix < subresource.base_mip_level + subresource.mip_level_count; ++ix)
            {
                uint32_t view_index = dx12_texture->get_view_index(
                    ViewType::DX12_RenderTargetView,
                    subresource,
                    false
                );

				D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle{ _descriptor_heaps->render_target_heap.get_cpu_handle(view_index) };

                active_cmdlist->d3d12_cmdlist->ClearRenderTargetView(rtv_handle, &clear_color.r, 0, nullptr);
            }
        }
        else 
        {
			ReturnIfFalse(set_texture_state(texture, subresource, ResourceStates::UnorderedAccess));

            commit_barriers();
            ReturnIfFalse(commit_descriptor_heaps());

            for (uint32_t ix = subresource.base_mip_level; ix < subresource.base_mip_level + subresource.mip_level_count; ++ix)
            {
                uint32_t descriptor_index = dx12_texture->GetClearMipLevelUAVIndex(ix);

                ID3D12Resource* d3d12_resource = reinterpret_cast<ID3D12Resource*>(dx12_texture->get_native_object());
                
                active_cmdlist->d3d12_cmdlist->ClearUnorderedAccessViewFloat(
                    _descriptor_heaps->shader_resource_heap.get_gpu_handle(descriptor_index), 
                    _descriptor_heaps->shader_resource_heap.get_cpu_handle(descriptor_index), 
                    d3d12_resource, 
                    &clear_color.r, 
                    0, 
                    nullptr
                );
            }
        }
        return true; 
    }

    bool DX12CommandList::clear_texture_uint(TextureInterface* texture, const TextureSubresourceSet& subresource_set, uint32_t dwClearColor)
    {
        DX12Texture* dx12_texture = check_cast<DX12Texture*>(texture);
        const auto& texture_desc = dx12_texture->get_desc();

        const FormatInfo& crFormatInfo = get_format_info(texture_desc.format);
        if (crFormatInfo.has_stencil || crFormatInfo.has_depth || (!texture_desc.is_render_target && !texture_desc.is_uav))
        {
            LOG_ERROR("This function can't use on the depth stenicl texture. require the texture is render target or unordered access. ");
            return false;
        }

        TextureSubresourceSet subresource = subresource_set.resolve(texture_desc, false);
        _cmdlist_ref_instances->ref_resources.emplace_back(texture);

        if (texture_desc.is_render_target)
        {
			ReturnIfFalse(set_texture_state(texture, subresource, ResourceStates::RenderTarget));

            commit_barriers();

            uint32_t max_mip_level = subresource.base_mip_level + subresource.mip_level_count;
            for (uint32_t ix = subresource.base_mip_level; ix < max_mip_level; ++ix)
            {
                uint32_t view_index = dx12_texture->get_view_index(
                    ViewType::DX12_RenderTargetView,
                    subresource,
                    false
                );

				D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle{ _descriptor_heaps->render_target_heap.get_cpu_handle(view_index) };

                float pfClearValues[4] = { 
                    static_cast<float>(dwClearColor), 
                    static_cast<float>(dwClearColor), 
                    static_cast<float>(dwClearColor), 
                    static_cast<float>(dwClearColor) 
                };

                active_cmdlist->d3d12_cmdlist->ClearRenderTargetView(rtv_handle, pfClearValues, 0, nullptr);
            }
        }
        else 
        {
			ReturnIfFalse(set_texture_state(texture, subresource, ResourceStates::UnorderedAccess));

            commit_barriers();
            ReturnIfFalse(commit_descriptor_heaps());

            uint32_t max_mip_level = subresource.base_mip_level + subresource.mip_level_count;
            for (uint32_t ix = subresource.base_mip_level; ix < max_mip_level; ++ix)
            {
                uint32_t descriptor_index = dx12_texture->GetClearMipLevelUAVIndex(ix);
                
                uint32_t pdwClearValues[4] = { dwClearColor, dwClearColor, dwClearColor, dwClearColor };

                ID3D12Resource* d3d12_resource = reinterpret_cast<ID3D12Resource*>(dx12_texture->get_native_object());

                active_cmdlist->d3d12_cmdlist->ClearUnorderedAccessViewUint(
                    _descriptor_heaps->shader_resource_heap.get_gpu_handle(descriptor_index), 
                    _descriptor_heaps->shader_resource_heap.get_cpu_handle(descriptor_index), 
                    d3d12_resource, 
                    pdwClearValues, 
                    0, 
                    nullptr
                );
            }
        }
        return true; 
    }
    bool DX12CommandList::clear_depth_stencil_texture(
        TextureInterface* texture, 
        const TextureSubresourceSet& subresource_set, 
        bool clear_depth, 
        float depth, 
        bool clear_stencil, 
        uint8_t stencil
    )
    {
        if (!clear_depth && !clear_stencil)
        {
            LOG_ERROR("require the texture is depth or stencil.");
            return false;
        }
        
        DX12Texture* dx12_texture = check_cast<DX12Texture*>(texture);
        const auto& texture_desc = dx12_texture->get_desc();

        const FormatInfo& crFormatInfo = get_format_info(texture_desc.format);
        if (!(crFormatInfo.has_stencil || crFormatInfo.has_depth) || !texture_desc.is_depth_stencil)
        {
            LOG_ERROR("require the texture is depth or stencil.");
            return false;
        }

        TextureSubresourceSet subresource = subresource_set.resolve(texture_desc, false);

        _cmdlist_ref_instances->ref_resources.emplace_back(texture);

		ReturnIfFalse(set_texture_state(texture, subresource, ResourceStates::DepthWrite));

        commit_barriers();

        D3D12_CLEAR_FLAGS clear_flags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;
        if (!clear_depth) clear_flags = D3D12_CLEAR_FLAG_STENCIL;
        else if (!clear_stencil) clear_flags = D3D12_CLEAR_FLAG_DEPTH;

        for (uint32_t ix = subresource.base_mip_level; ix < subresource.base_mip_level + subresource.mip_level_count; ++ix)
        {
            uint32_t view_index = dx12_texture->get_view_index(
                ViewType::DX12_DepthStencilView,
                subresource,
                false
            );
			D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle{ _descriptor_heaps->depth_stencil_heap.get_cpu_handle(view_index) };

            active_cmdlist->d3d12_cmdlist->ClearDepthStencilView(dsv_handle, clear_flags, depth, stencil, 0, nullptr);
        }
        return true; 
    }
    
    bool DX12CommandList::copy_texture(TextureInterface* dst, const TextureSlice& dst_slice, TextureInterface* src, const TextureSlice& src_slice)
    {
        const auto& dst_desc = dst->get_desc();
        const auto& src_desc = src->get_desc();

        TextureSlice resolved_dst_slice = dst_slice.resolve(dst_desc);
        TextureSlice resolved_src_slice = src_slice.resolve(src_desc);

        uint32_t dst_subresource_index = calculate_texture_subresource(
            resolved_dst_slice.mip_level, 
            resolved_dst_slice.array_slice, 
            0, 
            dst_desc.mip_levels, 
            dst_desc.array_size
        );

        uint32_t src_subresource_index = calculate_texture_subresource(
            resolved_src_slice.mip_level, 
            resolved_src_slice.array_slice, 
            0, 
            src_desc.mip_levels, 
            src_desc.array_size
        );


        D3D12_TEXTURE_COPY_LOCATION dst_location;
        dst_location.pResource        = reinterpret_cast<ID3D12Resource*>(dst->get_native_object());
        dst_location.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst_location.SubresourceIndex = dst_subresource_index;

        D3D12_TEXTURE_COPY_LOCATION src_location;
        src_location.pResource        = reinterpret_cast<ID3D12Resource*>(src->get_native_object());
        src_location.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src_location.SubresourceIndex = src_subresource_index;

        D3D12_BOX src_box;
        src_box.left   = resolved_src_slice.x;
        src_box.top    = resolved_src_slice.y;
        src_box.front  = resolved_src_slice.z;
        src_box.right  = resolved_src_slice.x + resolved_src_slice.width;
        src_box.bottom = resolved_src_slice.y + resolved_src_slice.height;
        src_box.back   = resolved_src_slice.z + resolved_src_slice.depth;

		ReturnIfFalse(set_texture_state(dst, TextureSubresourceSet{ dst_slice.mip_level, 1, dst_slice.array_slice, 1 }, ResourceStates::CopyDest));
		ReturnIfFalse(set_texture_state(src, TextureSubresourceSet{ src_slice.mip_level, 1, src_slice.array_slice, 1 }, ResourceStates::CopySource));

        commit_barriers();

        _cmdlist_ref_instances->ref_resources.emplace_back(dst);
        _cmdlist_ref_instances->ref_resources.emplace_back(src);

        active_cmdlist->d3d12_cmdlist->CopyTextureRegion(
            &dst_location,
            resolved_dst_slice.x,
            resolved_dst_slice.y,
            resolved_dst_slice.z,
            &src_location,
            &src_box
        );
        
        return true; 
    }
    bool DX12CommandList::copy_texture(StagingTextureInterface* dst, const TextureSlice& dst_slice, TextureInterface* src, const TextureSlice& src_slice)
    {
        const auto& dst_desc = dst->get_desc();
        const auto& src_desc = src->get_desc();
        
        TextureSlice resolved_dst_slice = dst_slice.resolve(dst_desc);
        TextureSlice resolved_src_slice = src_slice.resolve(src_desc);


        uint32_t src_subresource_index = calculate_texture_subresource(
            resolved_src_slice.mip_level, 
            resolved_src_slice.array_slice, 
            0, 
            src_desc.mip_levels, 
            src_desc.array_size
        );

        D3D12_TEXTURE_COPY_LOCATION dst_location;
        dst_location.pResource        = reinterpret_cast<ID3D12Resource*>(dst->get_native_object());
        dst_location.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst_location.PlacedFootprint = check_cast<DX12StagingTexture*>(dst)->get_slice_region(resolved_dst_slice).footprint;

        D3D12_TEXTURE_COPY_LOCATION src_location;
        src_location.pResource        = reinterpret_cast<ID3D12Resource*>(src->get_native_object());
        src_location.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src_location.SubresourceIndex = src_subresource_index;

        D3D12_BOX src_box;
        src_box.left   = resolved_src_slice.x;
        src_box.top    = resolved_src_slice.y;
        src_box.front  = resolved_src_slice.z;
        src_box.right  = resolved_src_slice.x + resolved_src_slice.width;
        src_box.bottom = resolved_src_slice.y + resolved_src_slice.height;
        src_box.back   = resolved_src_slice.z + resolved_src_slice.depth;

		ReturnIfFalse(set_staging_texture_state(dst, ResourceStates::CopyDest));
		ReturnIfFalse(set_texture_state(src, TextureSubresourceSet{ src_slice.mip_level, 1, src_slice.array_slice, 1 }, ResourceStates::CopySource));

        commit_barriers();

        _cmdlist_ref_instances->ref_staging_textures.emplace_back(dst);
        _cmdlist_ref_instances->ref_resources.emplace_back(src);

        active_cmdlist->d3d12_cmdlist->CopyTextureRegion(
            &dst_location,
            resolved_dst_slice.x,
            resolved_dst_slice.y,
            resolved_dst_slice.z,
            &src_location,
            &src_box
        );

        return true; 
    }
    bool DX12CommandList::copy_texture(TextureInterface* dst, const TextureSlice& dst_slice, StagingTextureInterface* src, const TextureSlice& src_slice)
    { 
        const auto& dst_desc = dst->get_desc();
        const auto& src_desc = src->get_desc();

        TextureSlice resolved_dst_slice = dst_slice.resolve(dst_desc);
        TextureSlice resolved_src_slice = src_slice.resolve(src_desc);

        uint32_t dst_subresource_index = calculate_texture_subresource(
            resolved_dst_slice.mip_level, 
            resolved_dst_slice.array_slice, 
            0, 
            dst_desc.mip_levels, 
            dst_desc.array_size
        );

        D3D12_TEXTURE_COPY_LOCATION dst_location;
        dst_location.pResource        = reinterpret_cast<ID3D12Resource*>(dst->get_native_object());
        dst_location.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst_location.SubresourceIndex = dst_subresource_index;

        D3D12_TEXTURE_COPY_LOCATION src_location;
        src_location.pResource        = reinterpret_cast<ID3D12Resource*>(src->get_native_object());
        src_location.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src_location.PlacedFootprint = check_cast<DX12StagingTexture*>(src)->get_slice_region(resolved_src_slice).footprint;

        D3D12_BOX src_box;
        src_box.left   = resolved_src_slice.x;
        src_box.top    = resolved_src_slice.y;
        src_box.front  = resolved_src_slice.z;
        src_box.right  = resolved_src_slice.x + resolved_src_slice.width;
        src_box.bottom = resolved_src_slice.y + resolved_src_slice.height;
        src_box.back   = resolved_src_slice.z + resolved_src_slice.depth;

		ReturnIfFalse(set_texture_state(dst, TextureSubresourceSet{ dst_slice.mip_level, 1, dst_slice.array_slice, 1 }, ResourceStates::CopyDest));
		ReturnIfFalse(set_staging_texture_state(src, ResourceStates::CopySource));

        commit_barriers();

        _cmdlist_ref_instances->ref_resources.emplace_back(dst);
        _cmdlist_ref_instances->ref_staging_textures.emplace_back(src);

        active_cmdlist->d3d12_cmdlist->CopyTextureRegion(
            &dst_location,
            resolved_dst_slice.x,
            resolved_dst_slice.y,
            resolved_dst_slice.z,
            &src_location,
            &src_box
        );

        return true;
    }
    
    bool DX12CommandList::write_texture(
        TextureInterface* dst, 
        uint32_t array_slice, 
        uint32_t mip_level, 
        const uint8_t* data, 
        uint64_t row_pitch, 
        uint64_t depth_pitch
    )
    { 
        const auto& buffer_desc = dst->get_desc();

		ReturnIfFalse(set_texture_state(dst, TextureSubresourceSet{ mip_level, 1, array_slice, 1 }, ResourceStates::CopyDest));

        commit_barriers();

        uint32_t dwSubresourceIndex = calculate_texture_subresource(
            mip_level,
            array_slice,
            0,
            buffer_desc.mip_levels,
            buffer_desc.array_size
        );

        ID3D12Resource* d3d12_resource = reinterpret_cast<ID3D12Resource*>(dst->get_native_object());

        D3D12_RESOURCE_DESC d3d12_resource_desc = d3d12_resource->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        uint32_t row_num;
        uint64_t row_size_in_byte;
        uint64_t total_bytes;

        _context->device->GetCopyableFootprints(&d3d12_resource_desc, dwSubresourceIndex, 1, 0, &footprint, &row_num, &row_size_in_byte, &total_bytes);

        uint8_t* cpu_address = nullptr;
        ID3D12Resource* d3d12_upload_buffer = nullptr;
        uint64_t offset_in_upload_buffer = 0;

        ReturnIfFalse(_upload_manager.suballocate_buffer(
            total_bytes, 
            &d3d12_upload_buffer, 
            &offset_in_upload_buffer, 
            &cpu_address, 
            nullptr, 
            _recording_version, 
            D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
        ));
        footprint.Offset = offset_in_upload_buffer;

        ReturnIfFalse(row_num <= footprint.Footprint.Height);

        for (uint32_t dwDepthSlice = 0; dwDepthSlice < footprint.Footprint.Depth; ++dwDepthSlice)
        {
            for (uint32_t dwRow = 0; dwRow < row_num; ++dwRow)
            {
                void* pvDstAddress = static_cast<uint8_t*>(cpu_address) + footprint.Footprint.RowPitch * (dwRow + dwDepthSlice * row_num);
                const void* cpvSrcAddress = data + row_pitch * dwRow + depth_pitch * dwDepthSlice;
                memcpy(pvDstAddress, cpvSrcAddress, std::min(row_pitch, row_size_in_byte));
            }
        }

        D3D12_TEXTURE_COPY_LOCATION dst_location;
        dst_location.pResource = d3d12_resource;
        dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst_location.SubresourceIndex = dwSubresourceIndex;

        D3D12_TEXTURE_COPY_LOCATION src_location;
        src_location.pResource = d3d12_upload_buffer;
        src_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src_location.PlacedFootprint = footprint;

        _cmdlist_ref_instances->ref_resources.emplace_back(dst);

        if (d3d12_upload_buffer != _current_upload_buffer)
        {
            _cmdlist_ref_instances->ref_native_resources.emplace_back(d3d12_upload_buffer);
            _current_upload_buffer = d3d12_upload_buffer;
        }

        active_cmdlist->d3d12_cmdlist->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

        return true; 
    }

    bool DX12CommandList::resolve_texture(TextureInterface* dst, const TextureSubresourceSet& crDstSubresourceSet, TextureInterface* src, const TextureSubresourceSet& crSrcSubresourceSet)
    { 
        const auto& dst_desc = dst->get_desc();
        const auto& src_desc = src->get_desc();

        TextureSubresourceSet DstSubresource = crDstSubresourceSet.resolve(dst_desc, false);
        TextureSubresourceSet SrcSubresource = crSrcSubresourceSet.resolve(src_desc, false);

        if (DstSubresource.array_slice_count != SrcSubresource.array_slice_count || 
            DstSubresource.mip_level_count != SrcSubresource.mip_level_count)
            return false;

		ReturnIfFalse(set_texture_state(dst, DstSubresource, ResourceStates::ResolveDst));
		ReturnIfFalse(set_texture_state(src, SrcSubresource, ResourceStates::ResolveSrc));

        commit_barriers();
        
        const DxgiFormatMapping& dxgi_fomat_mapping = get_dxgi_format_mapping(dst_desc.format);

        uint32_t plane_count = check_cast<DX12Texture*>(dst)->plane_count;
        ID3D12Resource* d3d12_resource = reinterpret_cast<ID3D12Resource*>(dst->get_native_object());
        for (uint32_t plane = 0; plane < plane_count; ++plane)
        {
            for (uint32_t array_slice = 0; array_slice < DstSubresource.array_slice_count; ++array_slice)
            {
                for (uint32_t mip_level = 0; mip_level < DstSubresource.mip_level_count; ++mip_level)
                {
                    uint32_t dst_subresource_index = calculate_texture_subresource(
                        mip_level + DstSubresource.base_mip_level, 
                        array_slice + DstSubresource.base_array_slice, 
                        plane, 
                        dst_desc.mip_levels, 
                        dst_desc.array_size
                    );

                    uint32_t src_subresource_index = calculate_texture_subresource(
                        mip_level + DstSubresource.base_mip_level, 
                        array_slice + DstSubresource.base_array_slice, 
                        plane, 
                        src_desc.mip_levels, 
                        src_desc.array_size
                    );

                    active_cmdlist->d3d12_cmdlist->ResolveSubresource(
                        d3d12_resource, 
                        dst_subresource_index, 
                        d3d12_resource, 
                        src_subresource_index, 
                        dxgi_fomat_mapping.rtv_format
                    );
                }
            }
        }

        return true;
    }
    
    bool DX12CommandList::write_buffer(BufferInterface* buffer, const void* data, uint64_t data_size, uint64_t dst_byte_offset)
    {
        uint8_t* cpu_address;
        D3D12_GPU_VIRTUAL_ADDRESS gpu_address;
        ID3D12Resource* d3d12_upload_buffer;
        uint64_t offset_in_upload_buffer;
        if (!_upload_manager.suballocate_buffer(
            data_size, 
            &d3d12_upload_buffer, 
            &offset_in_upload_buffer, 
            &cpu_address, 
            &gpu_address, 
            _recording_version, 
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
        ))
        {
            LOG_ERROR("Couldn't suballocate a upload buffer. ");
            return false;
        }

        if (d3d12_upload_buffer != _current_upload_buffer)
        {
            _cmdlist_ref_instances->ref_native_resources.emplace_back(d3d12_upload_buffer);
            _current_upload_buffer = d3d12_upload_buffer;
        }

        memcpy(cpu_address, data, data_size);

        BufferDesc buffer_desc = buffer->get_desc();

        if (buffer_desc.is_volatile)
        {
            _volatile_constant_buffer_addresses[buffer] = gpu_address;
            _any_volatile_constant_buffer_writes = true;
        }
        else 
        {
			ReturnIfFalse(set_buffer_state(buffer, ResourceStates::CopyDest));

            commit_barriers();

            _cmdlist_ref_instances->ref_resources.emplace_back(buffer);
            active_cmdlist->d3d12_cmdlist->CopyBufferRegion(
                reinterpret_cast<ID3D12Resource*>(buffer->get_native_object()), 
                dst_byte_offset, 
                d3d12_upload_buffer, 
                offset_in_upload_buffer, 
                data_size
            );
        }

        return true;
    }

    bool DX12CommandList::clear_buffer_uint(BufferInterface* buffer, uint32_t clear_value)
    {
        ReturnIfFalse(buffer->get_desc().can_have_uavs);

		ReturnIfFalse(set_buffer_state(buffer, ResourceStates::UnorderedAccess));
        commit_barriers();
        ReturnIfFalse(commit_descriptor_heaps());

        uint32_t clear_uav_index = check_cast<DX12Buffer*>(buffer)->get_clear_uav_index();

        _cmdlist_ref_instances->ref_resources.emplace_back(buffer);

        const uint32_t clear_values[4] = { clear_value, clear_value, clear_value, clear_value };
        active_cmdlist->d3d12_cmdlist->ClearUnorderedAccessViewUint(
            _descriptor_heaps->shader_resource_heap.get_gpu_handle(clear_uav_index), 
            _descriptor_heaps->shader_resource_heap.get_cpu_handle(clear_uav_index), 
            reinterpret_cast<ID3D12Resource*>(buffer->get_native_object()), 
            clear_values, 
            0, 
            nullptr
        );

        return true;
    }
    
    bool DX12CommandList::copy_buffer(BufferInterface* dst, uint64_t dst_byte_offset, BufferInterface* src, uint64_t src_byte_offset, uint64_t data_byte_size)
    {
		ReturnIfFalse(set_buffer_state(dst, ResourceStates::CopyDest));
		ReturnIfFalse(set_buffer_state(src, ResourceStates::CopySource));
        commit_barriers();

        const auto& dst_desc = dst->get_desc();
        const auto& src_desc = src->get_desc();

        if (dst_desc.cpu_access == CpuAccessMode::None)
            _cmdlist_ref_instances->ref_resources.emplace_back(dst);
        else 
            _cmdlist_ref_instances->ref_staging_buffers.emplace_back(dst);

        if (src_desc.cpu_access == CpuAccessMode::None)
            _cmdlist_ref_instances->ref_resources.emplace_back(src);
        else 
            _cmdlist_ref_instances->ref_staging_buffers.emplace_back(src);

        active_cmdlist->d3d12_cmdlist->CopyBufferRegion(
            reinterpret_cast<ID3D12Resource*>(dst->get_native_object()),
            dst_byte_offset,
            reinterpret_cast<ID3D12Resource*>(src->get_native_object()),
            src_byte_offset,
            data_byte_size
        );

        return true;
    }
    
    bool DX12CommandList::set_push_constants(const void* data, uint64_t byte_size)
    {
        DX12RootSignature* dx12_root_signature = nullptr;
        bool is_graphics = false;

        if (_current_graphics_state_valid && _current_graphics_state.pipeline != nullptr)
        {
            DX12GraphicsPipeline* dx12_graphics_pipeline = check_cast<DX12GraphicsPipeline*>(_current_graphics_state.pipeline);
            dx12_root_signature = dx12_graphics_pipeline->dx12_root_signature.get();
            is_graphics = true;
        }
        else if (_current_compute_state_valid && _current_compute_state.pipeline != nullptr)
        {
            DX12ComputePipeline* dx12_compute_pipeline = check_cast<DX12ComputePipeline*>(_current_compute_state.pipeline);
            dx12_root_signature = dx12_compute_pipeline->dx12_root_signature.get();
            is_graphics = false;
        }
        else if (_current_ray_tracing_state_valid && _current_ray_tracing_state.shader_table)
        {
            ray_tracing::DX12Pipeline* dx12_pipeline = check_cast<ray_tracing::DX12Pipeline*>(_current_ray_tracing_state.shader_table->get_pipeline());
            dx12_root_signature = dx12_pipeline->_global_root_signature.get();
            is_graphics = false;
        }

		ReturnIfFalse(dx12_root_signature && dx12_root_signature->push_constant_size == byte_size);

        if (is_graphics)
        {
            active_cmdlist->d3d12_cmdlist->SetGraphicsRoot32BitConstants(
                dx12_root_signature->root_param_push_constant_index, 
                static_cast<uint32_t>(byte_size / 4), 
                data, 
                0
            );
        }
        else 
        {
            active_cmdlist->d3d12_cmdlist->SetComputeRoot32BitConstants(
                dx12_root_signature->root_param_push_constant_index, 
                static_cast<uint32_t>(byte_size / 4), 
                data, 
                0
            );
        }

        return true;
    }

    bool DX12CommandList::set_graphics_state(const GraphicsState& state)
    { 
        DX12GraphicsPipeline* current_dx12_graphics_pipeline = check_cast<DX12GraphicsPipeline*>(_current_graphics_state.pipeline);
        DX12GraphicsPipeline* dx12_graphics_pipeline = check_cast<DX12GraphicsPipeline*>(state.pipeline);

        uint32_t binding_update_mask = 0;     // 按位判断 bindingset 数组中哪一个 bindingset 需要更新绑定

        bool update_root_signature = !_current_graphics_state_valid || 
                                    _current_graphics_state.pipeline == nullptr || 
                                    current_dx12_graphics_pipeline->dx12_root_signature != dx12_graphics_pipeline->dx12_root_signature;
        
        if (!update_root_signature) binding_update_mask = ~0u;

        if (commit_descriptor_heaps()) binding_update_mask = ~0u;

        if (binding_update_mask == 0)
        {
            binding_update_mask = find_array_different_bits(
                _current_graphics_state.binding_sets, 
                _current_graphics_state.binding_sets.size(), 
                state.binding_sets, 
                state.binding_sets.size()
            );
        } 

        bool update_pipeline = !_current_graphics_state_valid || _current_graphics_state.pipeline != state.pipeline;

        if (update_pipeline)
        {
            ReturnIfFalse(bind_graphics_pipeline(state.pipeline, update_root_signature));
            _cmdlist_ref_instances->ref_resources.emplace_back(state.pipeline);
        }

        
        GraphicsPipelineDesc pipeline_desc = dx12_graphics_pipeline->get_desc();

        uint8_t btEffectiveStencilRefValue = 
            pipeline_desc.render_state.depth_stencil_state.dynamic_stencil_ref ? 
            state.dynamic_stencil_ref_value : 
            pipeline_desc.render_state.depth_stencil_state.stencil_ref_value;

        bool update_stencil_ref = !_current_graphics_state_valid || _current_graphics_state.dynamic_stencil_ref_value != btEffectiveStencilRefValue;

        if (pipeline_desc.render_state.depth_stencil_state.enable_stencil && (update_pipeline || update_stencil_ref))
        {
            active_cmdlist->d3d12_cmdlist->OMSetStencilRef(btEffectiveStencilRefValue);
        }

        bool update_blend_factor = !_current_graphics_state_valid || _current_graphics_state.blend_constant_color != state.blend_constant_color;
        if (dx12_graphics_pipeline->require_blend_factor && update_blend_factor)
        {
            active_cmdlist->d3d12_cmdlist->OMSetBlendFactor(&state.blend_constant_color.r);
        }

		bool update_frame_buffer = !_current_graphics_state_valid || _current_graphics_state.frame_buffer != state.frame_buffer;
		if (update_frame_buffer)
		{
			ReturnIfFalse(bind_frame_buffer(state.frame_buffer));
			_cmdlist_ref_instances->ref_resources.emplace_back(state.frame_buffer);
		}

        ReturnIfFalse(set_graphics_bindings(
            state.binding_sets,
            binding_update_mask, 
            dx12_graphics_pipeline->dx12_root_signature.get()
        ));


        bool update_index_buffer = 
            !_current_graphics_state_valid || 
            _current_graphics_state.index_buffer_binding != state.index_buffer_binding;
        if (update_index_buffer && state.index_buffer_binding.is_valid())
        {
            D3D12_INDEX_BUFFER_VIEW d3d12_index_buffer_view{};
            
            auto& index_buffer = state.index_buffer_binding.buffer;
			ReturnIfFalse(set_buffer_state(index_buffer.get(), ResourceStates::IndexBuffer));


            d3d12_index_buffer_view.Format = get_dxgi_format_mapping(state.index_buffer_binding.format).srv_format;
            d3d12_index_buffer_view.BufferLocation = 
                reinterpret_cast<ID3D12Resource*>(index_buffer->get_native_object())->GetGPUVirtualAddress() + 
                state.index_buffer_binding.offset;
            d3d12_index_buffer_view.SizeInBytes = 
                static_cast<uint32_t>(index_buffer->get_desc().byte_size - state.index_buffer_binding.offset);
            
            _cmdlist_ref_instances->ref_resources.emplace_back(index_buffer.get());

            active_cmdlist->d3d12_cmdlist->IASetIndexBuffer(&d3d12_index_buffer_view);
        }

        bool update_vertex_buffer = 
            !_current_graphics_state_valid || 
            !is_same_arrays(_current_graphics_state.vertex_buffer_bindings, state.vertex_buffer_bindings);

        if (update_vertex_buffer && !state.vertex_buffer_bindings.empty())
        {
            D3D12_VERTEX_BUFFER_VIEW d3d12_vertex_buffer_views[MAX_VERTEX_ATTRIBUTES];
            uint32_t dwMaxVertexBufferBindingSlot = 0;

            DX12InputLayout* dx12_input_layout = check_cast<DX12InputLayout*>(pipeline_desc.input_layout);

            for (const auto& binding : state.vertex_buffer_bindings)
            {
				ReturnIfFalse(set_buffer_state(binding.buffer.get(), ResourceStates::VertexBuffer));
                
                if (binding.slot > MAX_VERTEX_ATTRIBUTES) return false;
                
                d3d12_vertex_buffer_views[binding.slot].StrideInBytes = dx12_input_layout->slot_strides[binding.slot];
                d3d12_vertex_buffer_views[binding.slot].BufferLocation = 
                    reinterpret_cast<ID3D12Resource*>(binding.buffer->get_native_object())->GetGPUVirtualAddress() + binding.offset;
                d3d12_vertex_buffer_views[binding.slot].SizeInBytes = 
                    static_cast<uint32_t>(std::min(binding.buffer->get_desc().byte_size - binding.offset, static_cast<uint64_t>(ULONG_MAX)));

                dwMaxVertexBufferBindingSlot = std::max(dwMaxVertexBufferBindingSlot, binding.slot);

                _cmdlist_ref_instances->ref_resources.emplace_back(binding.buffer.get());
            }

            if (_current_graphics_state_valid)
            {
                for (const auto& binding : _current_graphics_state.vertex_buffer_bindings)
                {
                    if (binding.slot < MAX_VERTEX_ATTRIBUTES)
                    {
                        dwMaxVertexBufferBindingSlot = std::max(dwMaxVertexBufferBindingSlot, binding.slot);
                    }
                }
            }

            active_cmdlist->d3d12_cmdlist->IASetVertexBuffers(0, dwMaxVertexBufferBindingSlot + 1, d3d12_vertex_buffer_views);
        }
        
        commit_barriers();

        bool update_viewports = 
            !_current_graphics_state_valid ||
            !is_same_arrays(_current_graphics_state.viewport_state.viewports, state.viewport_state.viewports) ||
            !is_same_arrays(_current_graphics_state.viewport_state.rects, state.viewport_state.rects);

        if (update_viewports)
        {
            FrameBufferInfo frame_buffer_info = state.frame_buffer->get_info();

            DX12ViewportState viewport_state = convert_viewport_state(pipeline_desc.render_state.raster_state, frame_buffer_info, state.viewport_state);

            if (viewport_state.viewports.size() > 0)
            {
                active_cmdlist->d3d12_cmdlist->RSSetViewports(viewport_state.viewports.size(), viewport_state.viewports.data());
            }

            if (viewport_state.scissor_rects.size() > 0)
            {
                active_cmdlist->d3d12_cmdlist->RSSetScissorRects(viewport_state.scissor_rects.size(), viewport_state.scissor_rects.data());
            }
        }

        _current_graphics_state_valid = true;
        _current_compute_state_valid = false;
        _current_ray_tracing_state_valid = false;
        _current_graphics_state = state;
        _current_graphics_state.dynamic_stencil_ref_value = btEffectiveStencilRefValue;

        


        return true;
    }

    bool DX12CommandList::set_compute_state(const ComputeState& state)
    {
        DX12ComputePipeline* curr_dx12_compute_pipeline = check_cast<DX12ComputePipeline*>(_current_compute_state.pipeline);
        DX12ComputePipeline* dx12_compute_pipeline = check_cast<DX12ComputePipeline*>(state.pipeline);

        uint32_t binding_update_mask = 0;

        bool update_root_signature = !_current_compute_state_valid || _current_compute_state.pipeline == nullptr;
        if (!update_root_signature && curr_dx12_compute_pipeline->dx12_root_signature != dx12_compute_pipeline->dx12_root_signature)
            update_root_signature = true;

        if (!_current_compute_state_valid || update_root_signature) binding_update_mask = ~0u; 
        if (commit_descriptor_heaps()) binding_update_mask = ~0u;

        if (binding_update_mask == 0)
        {
            binding_update_mask = find_array_different_bits(
                _current_compute_state.binding_sets, 
                _current_compute_state.binding_sets.size(), 
                state.binding_sets, 
                state.binding_sets.size()
            );
        } 

        if (update_root_signature)
        {
            active_cmdlist->d3d12_cmdlist->SetComputeRootSignature(
                dx12_compute_pipeline->dx12_root_signature->d3d12_root_signature.Get()
            );
        }

        bool update_pipeline = !_current_compute_state_valid || _current_compute_state.pipeline != state.pipeline;
        if (update_pipeline)
        {
            active_cmdlist->d3d12_cmdlist->SetPipelineState(reinterpret_cast<ID3D12PipelineState*>(state.pipeline->get_native_object()));
            _cmdlist_ref_instances->ref_resources.emplace_back(state.pipeline);
        }

        set_compute_bindings(
            state.binding_sets, 
            binding_update_mask, 
            dx12_compute_pipeline->dx12_root_signature.get()
        );
        
        _current_compute_state_valid = true;
        _current_graphics_state_valid = false;
        _current_ray_tracing_state_valid = false;
        _current_compute_state = state;


        return true;
    }

    bool DX12CommandList::draw(const DrawArguments& arguments)
    {
        update_graphics_volatile_buffers();

        active_cmdlist->d3d12_cmdlist->DrawInstanced(
            arguments.index_count, 
            arguments.instance_count, 
            arguments.start_vertex_location, 
            arguments.start_instance_location
        );
    
        return true;
    }
    
    bool DX12CommandList::draw_indexed(const DrawArguments& arguments)
    {
        update_graphics_volatile_buffers();

        active_cmdlist->d3d12_cmdlist->DrawIndexedInstanced(
            arguments.index_count, 
            arguments.instance_count, 
            arguments.start_index_location, 
            arguments.start_vertex_location, 
            arguments.start_instance_location
        );

        return true;
    }

    bool DX12CommandList::dispatch(uint32_t thread_group_num_x, uint32_t thread_group_num_y, uint32_t thread_group_num_z)
    {
        update_compute_volatile_buffers();

        active_cmdlist->d3d12_cmdlist->Dispatch(thread_group_num_x, thread_group_num_y, thread_group_num_z);
    
        return true;
    }

    bool DX12CommandList::begin_timer_query(TimerQueryInterface* query)
    {
        _cmdlist_ref_instances->ref_timer_queries.emplace_back(query);

        DX12TimerQuery* dx12_timer_query = check_cast<DX12TimerQuery*>(query);

        active_cmdlist->d3d12_cmdlist->EndQuery(
            _context->timer_query_heap.Get(), 
            D3D12_QUERY_TYPE_TIMESTAMP, 
            dx12_timer_query->begin_query_index
        );

        return true;
    }

    bool DX12CommandList::end_timer_query(TimerQueryInterface* query)
    {
        _cmdlist_ref_instances->ref_timer_queries.emplace_back(query);

        DX12TimerQuery* dx12_timer_query = check_cast<DX12TimerQuery*>(query);

        active_cmdlist->d3d12_cmdlist->EndQuery(
            _context->timer_query_heap.Get(), 
            D3D12_QUERY_TYPE_TIMESTAMP, 
            dx12_timer_query->end_query_index
        );

        active_cmdlist->d3d12_cmdlist->ResolveQueryData(
            _context->timer_query_heap.Get(), 
            D3D12_QUERY_TYPE_TIMESTAMP, 
            dx12_timer_query->end_query_index, 
            2, 
            reinterpret_cast<ID3D12Resource*>(_context->timer_query_resolve_buffer->get_native_object()), 
            dx12_timer_query->end_query_index * 8
        );

        return true;
    }
    
    bool DX12CommandList::begin_marker(const char* cpcName)
    {
        PIXBeginEvent(active_cmdlist->d3d12_cmdlist.Get(), 0, cpcName);
        return true;
    }
    bool DX12CommandList::end_marker()
    {
        PIXEndEvent(active_cmdlist->d3d12_cmdlist.Get());
        return true;
    }

    bool DX12CommandList::set_resource_state_from_binding_set(BindingSetInterface* binding_set)
    {
        if (binding_set->is_bindless()) return false;
        const BindingSetDesc& BindingSetDesc = binding_set->get_desc();

        DX12BindingSet* dx12_binding_set = check_cast<DX12BindingSet*>(binding_set);

        for (uint32_t ix = 0; ix < dx12_binding_set->bindings_which_need_transition.size(); ++ix)
        {
            const auto& binding = BindingSetDesc.binding_items[dx12_binding_set->bindings_which_need_transition[ix]];

            switch (binding.type)
            {
            case ResourceViewType::Texture_SRV:
                {
                    ResourceStates state = _desc.queue_type == CommandQueueType::Compute ? 
                                                   ResourceStates::NonPixelShaderResource : ResourceStates::PixelShaderResource;
                    ReturnIfFalse(set_texture_state(check_cast<TextureInterface*>(binding.resource.get()), binding.subresource, state));
                    break;
                }
            case ResourceViewType::Texture_UAV:
                {
                    ReturnIfFalse(set_texture_state(
                        check_cast<TextureInterface*>(binding.resource.get()), 
                        binding.subresource, 
                        ResourceStates::UnorderedAccess
                    ));
                    break;
                }
            case ResourceViewType::RawBuffer_SRV:
            case ResourceViewType::TypedBuffer_SRV:
            case ResourceViewType::StructuredBuffer_SRV:
                {
					ResourceStates state = _desc.queue_type == CommandQueueType::Compute ?
						                           ResourceStates::NonPixelShaderResource : ResourceStates::PixelShaderResource;
                    ReturnIfFalse(set_buffer_state(check_cast<BufferInterface*>(binding.resource.get()), state));
                    break;
                }
            case ResourceViewType::RawBuffer_UAV:
            case ResourceViewType::TypedBuffer_UAV:
            case ResourceViewType::StructuredBuffer_UAV:
                {
                    ReturnIfFalse(set_buffer_state(check_cast<BufferInterface*>(binding.resource.get()), ResourceStates::UnorderedAccess));
                    break;
                }
            case ResourceViewType::ConstantBuffer:
                {
                    ReturnIfFalse(set_buffer_state(check_cast<BufferInterface*>(binding.resource.get()), ResourceStates::ConstantBuffer));
                    break;
                }
            case ResourceViewType::AccelStruct:
                {
                    ReturnIfFalse(set_buffer_state(
                        check_cast<ray_tracing::DX12AccelStruct*>(binding.resource.get())->get_buffer(), 
                        ResourceStates::AccelStructRead
                    ));
                    break;
                }
            default: break;
            }
        }
        return true;
    }

    bool DX12CommandList::set_resource_state_from_frame_buffer(FrameBufferInterface* frame_buffer)
    {
        const FrameBufferDesc& frame_buffer_desc = frame_buffer->get_desc();

        for (uint32_t ix = 0; ix < frame_buffer_desc.color_attachments.size(); ++ix)
        {
            const auto& attachment = frame_buffer_desc.color_attachments[ix];
            ReturnIfFalse(set_texture_state(attachment.texture.get(), attachment.subresource, ResourceStates::RenderTarget));
        }
        
        if (frame_buffer_desc.depth_stencil_attachment.is_valid())
        {
            ReturnIfFalse(set_texture_state(
                frame_buffer_desc.depth_stencil_attachment.texture.get(), 
                frame_buffer_desc.depth_stencil_attachment.subresource, 
                ResourceStates::DepthWrite
            ));
        }
    
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
    
    bool DX12CommandList::set_texture_state(TextureInterface* texture, const TextureSubresourceSet& subresource_set, ResourceStates states)
    {
        ReturnIfFalse(_resource_state_tracker.set_texture_state(texture, subresource_set, states));
        
        if (_cmdlist_ref_instances != nullptr)
        {
            _cmdlist_ref_instances->ref_resources.emplace_back(texture);
        }

        return true;
    }

    bool DX12CommandList::set_buffer_state(BufferInterface* buffer, ResourceStates states)
    {
        ReturnIfFalse(_resource_state_tracker.set_buffer_state(buffer, states));

        if (_cmdlist_ref_instances != nullptr)
        {
            _cmdlist_ref_instances->ref_resources.emplace_back(buffer);
        }

        return true;
    }

    void DX12CommandList::commit_barriers()
    {
        const auto& texture_barriers = _resource_state_tracker.get_texture_barriers();
        const auto& buffer_barriers = _resource_state_tracker.get_buffer_barriers();

        const uint64_t total_barrier_count = texture_barriers.size() + buffer_barriers.size();
        if (total_barrier_count == 0) return;


        _d3d12_barriers.clear();
        _d3d12_barriers.reserve(total_barrier_count);

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
                    uint32_t plane_count = check_cast<DX12Texture*>(barrier.texture)->plane_count;
                    const auto& texture_desc = barrier.texture->get_desc();
                    for (uint32_t pland_index = 0; pland_index < plane_count; ++pland_index)
                    {
                        d3d12_barrier.Transition.Subresource = calculate_texture_subresource(
                            barrier.mip_level, 
                            barrier.array_slice, 
                            pland_index, 
                            texture_desc.mip_levels, 
                            texture_desc.array_size
                        );

                        _d3d12_barriers.emplace_back(d3d12_barrier);
                    }
                }
            }
            else if ((d3d12_after_state & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0)
            {
                // 切换 uav 的资源时也需要放入 barrier.
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
                // 切换 uav 的资源时也需要放入 barrier.
                d3d12_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                d3d12_barrier.UAV.pResource = d3d12_resource;

                _d3d12_barriers.emplace_back(d3d12_barrier);
            }
        }

        if (!_d3d12_barriers.empty())
        {
            active_cmdlist->d3d12_cmdlist->ResourceBarrier(static_cast<uint32_t>(_d3d12_barriers.size()), _d3d12_barriers.data());
        }

        _resource_state_tracker.clear_barriers();
    }
    
    ResourceStates DX12CommandList::get_texture_subresource_state(
        TextureInterface* texture, 
        uint32_t array_slice, 
        uint32_t mip_level
    )
    {
        return _resource_state_tracker.get_texture_subresource_state(texture, array_slice, mip_level);
    }
    
    ResourceStates DX12CommandList::get_buffer_state(BufferInterface* buffer)
    {
        return _resource_state_tracker.get_buffer_state(buffer);
    }

    DeviceInterface* DX12CommandList::get_deivce()
    {
        return _device;
    }

    CommandListDesc DX12CommandList::get_desc()
    {
        return _desc;
    }

    void* DX12CommandList::get_native_object()
    {
        return static_cast<void*>(active_cmdlist->d3d12_cmdlist.Get());
    }


    bool DX12CommandList::set_ray_tracing_state(const ray_tracing::PipelineState& state)
    {
        ray_tracing::DX12ShaderTable* shader_table = check_cast<ray_tracing::DX12ShaderTable*>(state.shader_table);
        ray_tracing::DX12Pipeline* pipeline = check_cast<ray_tracing::DX12Pipeline*>(shader_table->get_pipeline());
        ray_tracing::DX12ShaderTableState* shader_table_state = get_shaderTableState(shader_table);

        bool rebuild_shader_table = shader_table_state->committed_version != shader_table->_version ||
            shader_table_state->d3d12_descriptor_heap_srv != _descriptor_heaps->shader_resource_heap.get_shader_visible_heap() ||
            shader_table_state->d3d12_descriptor_heap_samplers != _descriptor_heaps->sampler_heap.get_shader_visible_heap();

        if (rebuild_shader_table)
        {
            uint32_t entry_size = pipeline->get_shaderTableEntrySize();
            uint8_t* cpu_address;
            D3D12_GPU_VIRTUAL_ADDRESS gpu_address;
            ReturnIfFalse(_upload_manager.suballocate_buffer(
                entry_size * shader_table->get_entry_count(), 
                nullptr, 
                nullptr, 
                &cpu_address, 
                &gpu_address, 
                _recording_version, 
                D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT
            ));

            uint32_t entry_index = 0;

            auto WriteEntry = [&](const ray_tracing::DX12ShaderTable::ShaderEntry& entry) 
            {
                memcpy(cpu_address, entry.shader_identifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

                if (entry.binding_set)
                {
                    DX12BindingSet* bindingSet = check_cast<DX12BindingSet*>(entry.binding_set);
                    DX12BindingLayout* layout = check_cast<DX12BindingLayout*>(bindingSet->get_layout());

                    if (layout->descriptor_table_sampler_size > 0)
                    {
                        auto table = reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(
                            cpu_address + 
                            D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 
                            layout->root_param_sampler_index * sizeof(D3D12_GPU_DESCRIPTOR_HANDLE)
                        );
                        *table = _descriptor_heaps->sampler_heap.get_gpu_handle(bindingSet->descriptor_table_sampler_base_index);
                    }

                    if (layout->descriptor_table_srv_etc_size > 0)
                    {
                        auto table = reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(
                            cpu_address + 
                            D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 
                            layout->root_param_srv_etc_index * sizeof(D3D12_GPU_DESCRIPTOR_HANDLE)
                        );
                        *table = _descriptor_heaps->shader_resource_heap.get_gpu_handle(bindingSet->descriptor_table_srv_etc_base_index);
                    }

                    ReturnIfFalse(layout->descriptor_volatile_constant_buffers.empty());
                }

                cpu_address += entry_size;
                gpu_address += entry_size;
                entry_index += 1;

                return true;
            };

            D3D12_DISPATCH_RAYS_DESC& d3d12_dispatch_rays_desc = shader_table_state->d3d12_dispatch_rays_desc;
            memset(&d3d12_dispatch_rays_desc, 0, sizeof(D3D12_DISPATCH_RAYS_DESC));

            d3d12_dispatch_rays_desc.RayGenerationShaderRecord.StartAddress = gpu_address;
            d3d12_dispatch_rays_desc.RayGenerationShaderRecord.SizeInBytes = entry_size;
            WriteEntry(shader_table->_raygen_shader);

            if (!shader_table->_miss_shaders.empty())
            {
                d3d12_dispatch_rays_desc.MissShaderTable.StartAddress = gpu_address;
                d3d12_dispatch_rays_desc.MissShaderTable.SizeInBytes = entry_size * static_cast<uint32_t>(shader_table->_miss_shaders.size());
                d3d12_dispatch_rays_desc.MissShaderTable.StrideInBytes = shader_table->_miss_shaders.size() == 1 ? 0 : entry_size;
                for (const auto& entry : shader_table->_miss_shaders) WriteEntry(entry);
            }
            if (!shader_table->_hit_groups.empty())
            {
                d3d12_dispatch_rays_desc.HitGroupTable.StartAddress = gpu_address;
                d3d12_dispatch_rays_desc.HitGroupTable.SizeInBytes = entry_size * static_cast<uint32_t>(shader_table->_hit_groups.size());
                d3d12_dispatch_rays_desc.HitGroupTable.StrideInBytes = shader_table->_hit_groups.size() == 1 ? 0 : entry_size;
                for (const auto& entry : shader_table->_miss_shaders) WriteEntry(entry);
            }
            if (!shader_table->_callable_shaders.empty())
            {
                d3d12_dispatch_rays_desc.CallableShaderTable.StartAddress = gpu_address;
                d3d12_dispatch_rays_desc.CallableShaderTable.SizeInBytes = entry_size * static_cast<uint32_t>(shader_table->_callable_shaders.size());
                d3d12_dispatch_rays_desc.CallableShaderTable.StrideInBytes = shader_table->_callable_shaders.size() == 1 ? 0 : entry_size;
                for (const auto& entry : shader_table->_miss_shaders) WriteEntry(entry);
            }

            shader_table_state->committed_version = shader_table->_version;
            shader_table_state->d3d12_descriptor_heap_srv = _descriptor_heaps->shader_resource_heap.get_shader_visible_heap();
            shader_table_state->d3d12_descriptor_heap_samplers = _descriptor_heaps->sampler_heap.get_shader_visible_heap();
            
            _cmdlist_ref_instances->ref_resources.push_back(shader_table);
        }

        
        ray_tracing::DX12Pipeline* current_pipeline = check_cast<ray_tracing::DX12Pipeline*>(_current_ray_tracing_state.shader_table->get_pipeline());
        const bool update_root_signature = 
            !_current_ray_tracing_state_valid || 
            _current_ray_tracing_state.shader_table == nullptr ||
            current_pipeline->_global_root_signature != pipeline->_global_root_signature;

        
        uint32_t binding_update_mask = 0;     // 按位判断 bindingset 数组中哪一个 bindingset 需要更新绑定

        if (!update_root_signature) binding_update_mask = ~0u;

        if (commit_descriptor_heaps()) binding_update_mask = ~0u;

        if (binding_update_mask == 0)
        {
            binding_update_mask = find_array_different_bits(
                _current_graphics_state.binding_sets, 
                _current_graphics_state.binding_sets.size(), 
                state.binding_sets, 
                state.binding_sets.size()
            );
        } 

        if (update_root_signature) 
        {
            active_cmdlist->d3d12_cmdlist4->SetComputeRootSignature(pipeline->_global_root_signature->d3d12_root_signature.Get());
        }

        bool update_pipeline = !_current_ray_tracing_state_valid || _current_ray_tracing_state.shader_table->get_pipeline() != pipeline;

        if (update_pipeline)
        {
            active_cmdlist->d3d12_cmdlist4->SetPipelineState1(reinterpret_cast<ID3D12StateObject*>(pipeline->get_native_object()));
            _cmdlist_ref_instances->ref_resources.push_back(pipeline);
        }

        set_compute_bindings(state.binding_sets, binding_update_mask, pipeline->_global_root_signature.get());

        _current_graphics_state_valid = false;
        _current_compute_state_valid = false;
        _current_ray_tracing_state_valid = true;
        _current_ray_tracing_state = state;
        return false;
    }

    bool DX12CommandList::dispatch_rays(const ray_tracing::DispatchRaysArguments& arguments)
    {
        ReturnIfFalse(_current_ray_tracing_state_valid && update_compute_volatile_buffers());

        D3D12_DISPATCH_RAYS_DESC desc = get_shaderTableState(_current_ray_tracing_state.shader_table)->d3d12_dispatch_rays_desc;
        desc.Width = arguments.width;
        desc.Height = arguments.height;
        desc.Depth = arguments.depth;

        active_cmdlist->d3d12_cmdlist4->DispatchRays(&desc);
        return true;
    }
    
    bool DX12CommandList::build_bottom_level_accel_struct(
        ray_tracing::AccelStructInterface* accel_struct,
        const ray_tracing::GeometryDesc* geometry_descs,
        uint32_t geometry_desc_count
    )
    {
        auto& desc = check_cast<ray_tracing::DX12AccelStruct*>(accel_struct)->_desc;
        ReturnIfFalse(!desc.is_top_level);

        bool preform_update = (desc.flags & ray_tracing::AccelStructBuildFlags::PerformUpdate) != 0;

        desc.bottom_level_geometry_descs.clear();
        desc.bottom_level_geometry_descs.reserve(geometry_desc_count);
        for (uint32_t ix = 0; ix < geometry_desc_count; ++ix)
        {
            const auto& geometry_desc = geometry_descs[ix];
            desc.bottom_level_geometry_descs[ix] = geometry_desc;

            if (geometry_desc.type == ray_tracing::GeometryType::Triangle)
            {
                ReturnIfFalse(set_buffer_state(geometry_desc.triangles.vertex_buffer.get(), ResourceStates::AccelStructBuildInput));
                ReturnIfFalse(set_buffer_state(geometry_desc.triangles.index_buffer.get(), ResourceStates::AccelStructBuildInput));

                _cmdlist_ref_instances->ref_resources.push_back(geometry_desc.triangles.vertex_buffer.get());
                _cmdlist_ref_instances->ref_resources.push_back(geometry_desc.triangles.index_buffer.get());
            }
            else 
            {
                ReturnIfFalse(set_buffer_state(geometry_desc.aabbs.buffer.get(), ResourceStates::AccelStructBuildInput));
                _cmdlist_ref_instances->ref_resources.push_back(geometry_desc.aabbs.buffer.get());
            }

        }

        commit_barriers();

        ray_tracing::DX12AccelStructBuildInputs dx12_build_inputs;
        dx12_build_inputs.type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        dx12_build_inputs.dwDescNum = static_cast<uint32_t>(desc.bottom_level_geometry_descs.size());
        dx12_build_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY_OF_POINTERS;
        dx12_build_inputs.GeometryDescs.resize(desc.bottom_level_geometry_descs.size());
        dx12_build_inputs.geometry_descs.resize(desc.bottom_level_geometry_descs.size());
        for (uint32_t ix = 0; ix < static_cast<uint32_t>(dx12_build_inputs.GeometryDescs.size()); ++ix)
        {
            dx12_build_inputs.geometry_descs[ix] = dx12_build_inputs.GeometryDescs.data() + ix;
        }
        dx12_build_inputs.cpcpGeometryDesc = dx12_build_inputs.geometry_descs.data();


        dx12_build_inputs.GeometryDescs.resize(geometry_desc_count);
        for (uint32_t ix = 0; ix < static_cast<uint32_t>(desc.bottom_level_geometry_descs.size()); ++ix)
        {
            const auto& geometry_desc = desc.bottom_level_geometry_descs[ix];
            D3D12_GPU_VIRTUAL_ADDRESS gpu_address = 0;
            if (geometry_desc.use_transform)
            {
                uint8_t* cpu_address = nullptr;
                ReturnIfFalse(!_upload_manager.suballocate_buffer(
                    sizeof(float3x4), 
                    nullptr, 
                    nullptr, 
                    &cpu_address, 
                    &gpu_address, 
                    _recording_version, 
                    D3D12_RAYTRACING_TRANSFORM3X4_BYTE_ALIGNMENT
                ));

                memcpy(cpu_address, &geometry_desc.affine_matrix, sizeof(float3x4));
            }

            dx12_build_inputs.GeometryDescs[ix] = ray_tracing::DX12AccelStruct::convert_geometry_desc(geometry_desc, gpu_address);
        }

        ray_tracing::DX12AccelStruct* dx12_accel_struct = check_cast<ray_tracing::DX12AccelStruct*>(accel_struct);
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO accel_struct_prebuild_info = dx12_accel_struct->get_accel_struct_prebuild_info();

        ReturnIfFalse(accel_struct_prebuild_info.ResultDataMaxSizeInBytes <= dx12_accel_struct->get_buffer()->get_desc().byte_size);

        uint64_t scratch_size = preform_update ? 
            accel_struct_prebuild_info.UpdateScratchDataSizeInBytes :
            accel_struct_prebuild_info.ScratchDataSizeInBytes;

        D3D12_GPU_VIRTUAL_ADDRESS scratch_gpu_address{};
        ReturnIfFalse(_dx12_scratch_manager.suballocate_buffer(
            scratch_size, 
            nullptr, 
            nullptr, 
            nullptr, 
            &scratch_gpu_address, 
            _recording_version, 
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT
        ));

        set_buffer_state(dx12_accel_struct->get_buffer(), ResourceStates::AccelStructWrite);
        commit_barriers();

        D3D12_GPU_VIRTUAL_ADDRESS accel_struct_data_address = 
            reinterpret_cast<ID3D12Resource*>(dx12_accel_struct->get_buffer()->get_native_object())->GetGPUVirtualAddress();

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC accel_struct_build_desc = {};
        accel_struct_build_desc.Inputs = dx12_build_inputs.Convert();
        accel_struct_build_desc.ScratchAccelerationStructureData = scratch_gpu_address;
        accel_struct_build_desc.DestAccelerationStructureData = accel_struct_data_address;
        accel_struct_build_desc.SourceAccelerationStructureData = preform_update ? accel_struct_data_address : 0;
        active_cmdlist->d3d12_cmdlist4->BuildRaytracingAccelerationStructure(&accel_struct_build_desc, 0, nullptr);
        _cmdlist_ref_instances->ref_resources.push_back(accel_struct);

        return true;
    }

    bool DX12CommandList::build_top_level_accel_struct(
        ray_tracing::AccelStructInterface* accel_struct, 
        const ray_tracing::InstanceDesc* instance_descs, 
        uint32_t instance_count
    )
    {
        ray_tracing::DX12AccelStruct* dx12_accel_struct = check_cast<ray_tracing::DX12AccelStruct*>(accel_struct);
        const auto& desc = dx12_accel_struct->get_desc();
        ReturnIfFalse(desc.is_top_level);

        BufferInterface* accel_struct_buffer = dx12_accel_struct->get_buffer();

        dx12_accel_struct->d3d12_ray_tracing_instance_descs.resize(instance_count);
        dx12_accel_struct->bottom_level_accel_structs.clear();
        
        for (uint32_t ix = 0; ix < instance_count; ++ix)
        {
            const auto& instance_desc = instance_descs[ix];
            auto& d3d12_instance_desc = dx12_accel_struct->d3d12_ray_tracing_instance_descs[ix];

            if (instance_desc.bottom_level_accel_struct)
            {
                dx12_accel_struct->bottom_level_accel_structs.emplace_back(instance_desc.bottom_level_accel_struct);
                ray_tracing::DX12AccelStruct* dx12_blas = check_cast<ray_tracing::DX12AccelStruct*>(instance_desc.bottom_level_accel_struct);
                BufferInterface* blas_buffer = dx12_blas->get_buffer();

                d3d12_instance_desc = ray_tracing::convert_instance_desc(instance_desc);
                d3d12_instance_desc.AccelerationStructure = 
                    reinterpret_cast<ID3D12Resource*>(blas_buffer->get_native_object())->GetGPUVirtualAddress();
                ReturnIfFalse(set_buffer_state(blas_buffer, ResourceStates::AccelStructBuildBlas));
            }
            else 
            {
                d3d12_instance_desc.AccelerationStructure = D3D12_GPU_VIRTUAL_ADDRESS{0};
            }
        }

        uint64_t upload_size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * dx12_accel_struct->d3d12_ray_tracing_instance_descs.size();
        D3D12_GPU_VIRTUAL_ADDRESS gpu_address{};
        uint8_t* cpu_address = nullptr;
        ReturnIfFalse(_upload_manager.suballocate_buffer(
            upload_size, 
            nullptr, 
            nullptr, 
            &cpu_address, 
            &gpu_address, 
            _recording_version, 
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
        ));

        memcpy(cpu_address, dx12_accel_struct->d3d12_ray_tracing_instance_descs.data(), upload_size);

        ReturnIfFalse(set_buffer_state(accel_struct_buffer, ResourceStates::AccelStructWrite));
        commit_barriers();

        bool perform_update = (desc.flags & ray_tracing::AccelStructBuildFlags::AllowUpdate) != 0;

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS d3d12_accel_struct_inputs;
        d3d12_accel_struct_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        d3d12_accel_struct_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        d3d12_accel_struct_inputs.InstanceDescs = gpu_address;
        d3d12_accel_struct_inputs.NumDescs = instance_count;
        d3d12_accel_struct_inputs.Flags = ray_tracing::convert_accel_struct_build_flags(desc.flags);
        if (perform_update) d3d12_accel_struct_inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
        _context->device5->GetRaytracingAccelerationStructurePrebuildInfo(&d3d12_accel_struct_inputs, &ASPreBuildInfo);

        ReturnIfFalse(ASPreBuildInfo.ResultDataMaxSizeInBytes <= accel_struct_buffer->get_desc().byte_size);

        uint64_t stScratchSize = perform_update ? 
            ASPreBuildInfo.UpdateScratchDataSizeInBytes :
            ASPreBuildInfo.ScratchDataSizeInBytes;

        D3D12_GPU_VIRTUAL_ADDRESS d3d12_scratch_gpu_address{};
        ReturnIfFalse(_dx12_scratch_manager.suballocate_buffer(
            stScratchSize, 
            nullptr, 
            nullptr, 
            nullptr, 
            &d3d12_scratch_gpu_address, 
            _recording_version, 
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT
        ));


        D3D12_GPU_VIRTUAL_ADDRESS d3d12_accel_struct_data_address = 
            reinterpret_cast<ID3D12Resource*>(accel_struct_buffer->get_native_object())->GetGPUVirtualAddress();

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC d3d12_build_as_desc = {};
        d3d12_build_as_desc.Inputs = d3d12_accel_struct_inputs;
        d3d12_build_as_desc.ScratchAccelerationStructureData = d3d12_scratch_gpu_address;
        d3d12_build_as_desc.DestAccelerationStructureData = d3d12_accel_struct_data_address;
        d3d12_build_as_desc.SourceAccelerationStructureData = perform_update ? d3d12_accel_struct_data_address : 0;

        active_cmdlist->d3d12_cmdlist4->BuildRaytracingAccelerationStructure(&d3d12_build_as_desc, 0, nullptr);
        _cmdlist_ref_instances->ref_resources.push_back(accel_struct);

        return true;
    }

    bool DX12CommandList::set_accel_struct_state(ray_tracing::AccelStructInterface* accel_struct, ResourceStates state)
    {
        ReturnIfFalse(_resource_state_tracker.set_buffer_state(check_cast<ray_tracing::DX12AccelStruct*>(accel_struct)->get_buffer(), state));
        
        if (_cmdlist_ref_instances != nullptr)
        {
            _cmdlist_ref_instances->ref_resources.emplace_back(accel_struct);
        }
        return true;
    }

    ray_tracing::DX12ShaderTableState* DX12CommandList::get_shaderTableState(ray_tracing::ShaderTableInterface* shader_table)
    {
        auto iter = _shader_table_states.find(shader_table);
        if (iter != _shader_table_states.end()) return iter->second.get();

        std::unique_ptr<ray_tracing::DX12ShaderTableState> shader_table_state = std::make_unique<ray_tracing::DX12ShaderTableState>();
        auto* ret = shader_table_state.get();

        _shader_table_states[shader_table] = std::move(shader_table_state);

        return ret;
    }

    bool DX12CommandList::allocate_upload_buffer(uint64_t size, uint8_t** cpu_address, D3D12_GPU_VIRTUAL_ADDRESS* gpu_address)
    {
        _upload_manager.suballocate_buffer(
            size, 
            nullptr, 
            nullptr, 
            cpu_address, 
            gpu_address, 
            _recording_version, 
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
        );
        
        return true;
    }

    bool DX12CommandList::commit_descriptor_heaps()
    {
        ID3D12DescriptorHeap* d3d12_srv_etc_heap = _descriptor_heaps->shader_resource_heap.get_shader_visible_heap();
        ID3D12DescriptorHeap* d3d12_sampler_heap = _descriptor_heaps->sampler_heap.get_shader_visible_heap();

        if (d3d12_srv_etc_heap != _current_srv_etc_heap || d3d12_sampler_heap != _current_sampler_heap)
        {
            ID3D12DescriptorHeap* d3d12_heaps[2] = { d3d12_srv_etc_heap, d3d12_sampler_heap };
            active_cmdlist->d3d12_cmdlist->SetDescriptorHeaps(2, d3d12_heaps);

            _current_srv_etc_heap = d3d12_srv_etc_heap;
            _current_sampler_heap = d3d12_sampler_heap;

            _cmdlist_ref_instances->ref_native_resources.emplace_back(d3d12_srv_etc_heap);
            _cmdlist_ref_instances->ref_native_resources.emplace_back(d3d12_sampler_heap);

            return true;
        }
        return false;
    }

    bool DX12CommandList::get_buffer_gpu_address(BufferInterface* buffer, D3D12_GPU_VIRTUAL_ADDRESS* gpu_address)
    {
        if (buffer == nullptr || gpu_address == nullptr) return false;
        
        if (buffer->get_desc().is_volatile)
        {
            *gpu_address = _volatile_constant_buffer_addresses[buffer];
        }
        else 
        {
            *gpu_address = reinterpret_cast<ID3D12Resource*>(buffer->get_native_object())->GetGPUVirtualAddress();
        }
    
        return true;
    }

    bool DX12CommandList::update_graphics_volatile_buffers()
    {
        if (!_any_volatile_constant_buffer_writes) return false;

        for (auto& rBinding : _current_graphics_volatile_constant_buffers)
        {
            D3D12_GPU_VIRTUAL_ADDRESS current_gpu_address = _volatile_constant_buffer_addresses[rBinding.buffer];
            if (current_gpu_address != rBinding.gpu_address)
            {
                active_cmdlist->d3d12_cmdlist->SetGraphicsRootConstantBufferView(rBinding.binding_point, current_gpu_address);
                rBinding.gpu_address = current_gpu_address;
            }
        }
        _any_volatile_constant_buffer_writes = false;
        
        return true;
    }

    bool DX12CommandList::update_compute_volatile_buffers()
    {
        if (_any_volatile_constant_buffer_writes) return false;

        for (auto& rBinding : _current_compute_volatile_constant_buffers)
        {
            D3D12_GPU_VIRTUAL_ADDRESS current_gpu_address = _volatile_constant_buffer_addresses[rBinding.buffer];
            if (current_gpu_address != rBinding.gpu_address)
            {
                active_cmdlist->d3d12_cmdlist->SetComputeRootConstantBufferView(rBinding.binding_point, current_gpu_address);
                rBinding.gpu_address = current_gpu_address;
            }
        }
        _any_volatile_constant_buffer_writes = false;
        
        return true;
    }

    std::shared_ptr<DX12CommandListInstance> DX12CommandList::excuted(ID3D12Fence* d3d12_fence, uint64_t last_submitted_value)
    {
        std::shared_ptr<DX12CommandListInstance> pRet = _cmdlist_ref_instances;
        pRet->fence = d3d12_fence;
        pRet->submitted_value = last_submitted_value;

        _cmdlist_ref_instances.reset();

        active_cmdlist->last_submitted_value = last_submitted_value;
        _cmdlist_pool.emplace_back(active_cmdlist);
        active_cmdlist.reset();

        for (const auto& p : pRet->ref_staging_textures)
        {
            DX12StagingTexture* pDX12StagingTexture = check_cast<DX12StagingTexture*>(p);
            pDX12StagingTexture->last_used_d3d12_fence = d3d12_fence;
            pDX12StagingTexture->last_used_fence_value = last_submitted_value;
        }
        for (const auto& p : pRet->ref_staging_buffers)
        {
            DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(p);
            dx12_buffer->last_used_d3d12_fence = d3d12_fence;
            dx12_buffer->last_used_fence_value = last_submitted_value;
        }
        for (const auto& p : pRet->ref_timer_queries)
        {
            DX12TimerQuery* dx12_timer_query = check_cast<DX12TimerQuery*>(p);
            dx12_timer_query->_started = true;
            dx12_timer_query->_resolved = false;
            dx12_timer_query->_d3d12_fence = d3d12_fence;
            dx12_timer_query->_fence_counter = last_submitted_value;
        }

        uint64_t submitted_version = make_version(last_submitted_value, _desc.queue_type, true);
        _upload_manager.submit_chunks(_recording_version, submitted_version);
        _recording_version = 0;

        return pRet;
    }

    bool DX12CommandList::set_staging_texture_state(StagingTextureInterface* staging_texture, ResourceStates state)
    {
        return _resource_state_tracker.set_buffer_state(check_cast<DX12StagingTexture*>(staging_texture)->get_buffer(), state);
    }

    bool DX12CommandList::set_graphics_bindings(
        const PipelineStateBindingSetArray& binding_sets,  
        uint32_t binding_update_mask, 
        DX12RootSignature* dx12_root_signature
    )
    {
        if (binding_update_mask > 0)
        {
            std::vector<VolatileConstantBufferBinding> new_graphics_volatile_constant_buffers;

            for (uint32_t ix = 0; ix < binding_sets.size(); ++ix)
            {
                BindingSetInterface* binding_set = binding_sets[ix];
                if (binding_set == nullptr) continue;

                // cbUpdateTheSet 放在后边做判断是为了能够进行一些判断增加容错.
                const bool update_set = (binding_update_mask & (1 << ix)) != 0;

                const auto& layout_offset = dx12_root_signature->binding_layout_map[ix];
                uint32_t binding_layout_root_param_offset = layout_offset.second;

                if (!binding_set->is_bindless())
                {
                    if (binding_set->get_layout() != layout_offset.first) return false;

                    DX12BindingSet* dx12_binding_set = check_cast<DX12BindingSet*>(binding_set);


                    for (
                        uint32_t constant_buffer_index = 0; 
                        constant_buffer_index < dx12_binding_set->root_param_index_volatile_constant_buffers.size(); 
                        ++constant_buffer_index
                    )
                    {
                        uint32_t constant_buffer_root_param_index = 
                            dx12_binding_set->root_param_index_volatile_constant_buffers[constant_buffer_index].first;
                        BufferInterface* volatile_constant_buffer = 
                            dx12_binding_set->root_param_index_volatile_constant_buffers[constant_buffer_index].second;

                        // Plus the offset. 
                        constant_buffer_root_param_index += binding_layout_root_param_offset;

                        if (volatile_constant_buffer != nullptr)
                        {;

                            if (volatile_constant_buffer->get_desc().is_volatile)
                            {
                                D3D12_GPU_VIRTUAL_ADDRESS gpu_address = 
                                    _volatile_constant_buffer_addresses[volatile_constant_buffer];

                                if (gpu_address == 0)
                                {
                                    LOG_ERROR("Attempted use of a volatile constant buffer before it was written into. ");
                                    return false;
                                }

                                if (update_set || gpu_address != _current_graphics_volatile_constant_buffers[new_graphics_volatile_constant_buffers.size()].gpu_address)
                                {
                                    active_cmdlist->d3d12_cmdlist->SetGraphicsRootConstantBufferView(
                                        constant_buffer_root_param_index, 
                                        gpu_address
                                    );
                                }
                                new_graphics_volatile_constant_buffers.emplace_back(
                                    VolatileConstantBufferBinding{ 
                                        constant_buffer_root_param_index, 
                                        volatile_constant_buffer, 
                                        gpu_address 
                                    }
                                );
                            }
                            else if (update_set)
                            {
                                D3D12_GPU_VIRTUAL_ADDRESS gpu_address = 
                                    reinterpret_cast<ID3D12Resource*>(volatile_constant_buffer->get_native_object())->GetGPUVirtualAddress();
                                if (gpu_address != 0)
                                {
                                    active_cmdlist->d3d12_cmdlist->SetGraphicsRootConstantBufferView(
                                        constant_buffer_root_param_index, 
                                        gpu_address
                                    );
                                }
                            }
                        }
                        else if (update_set)
                        {
                            active_cmdlist->d3d12_cmdlist->SetGraphicsRootConstantBufferView(constant_buffer_root_param_index, 0);
                        }
                    }

                    if (update_set)
                    {
                        if (dx12_binding_set->is_descriptor_table_sampler_valid)
                        {
                            active_cmdlist->d3d12_cmdlist->SetGraphicsRootDescriptorTable(
                                binding_layout_root_param_offset + dx12_binding_set->root_param_sampler_index,
                                _descriptor_heaps->sampler_heap.get_gpu_handle(dx12_binding_set->descriptor_table_sampler_base_index));
                        }

                        if (dx12_binding_set->is_descriptor_table_srv_etc_valid)
                        {
                            active_cmdlist->d3d12_cmdlist->SetGraphicsRootDescriptorTable(
                                binding_layout_root_param_offset + dx12_binding_set->root_param_srv_etc_index,
                                _descriptor_heaps->shader_resource_heap.get_gpu_handle(dx12_binding_set->descriptor_table_srv_etc_base_index));
                        }
                        
                        if (binding_set->is_bindless()) return false;
                        BindingSetDesc desc = binding_set->get_desc();
                        _cmdlist_ref_instances->ref_resources.emplace_back(binding_set);
                    }

                    if ((update_set || dx12_binding_set->has_uav_bingings))
                    {
                        set_resource_state_from_binding_set(binding_set);
                    }
                    
                }
                else if (update_set)
                {
                    DX12BindlessSet* dx12_descriptor_table = check_cast<DX12BindlessSet*>(binding_set);

                    active_cmdlist->d3d12_cmdlist->SetGraphicsRootDescriptorTable(
                        binding_layout_root_param_offset, 
                        _descriptor_heaps->shader_resource_heap.get_gpu_handle(dx12_descriptor_table->first_descriptor_index)
                    );
                }
            }
            _current_graphics_volatile_constant_buffers = new_graphics_volatile_constant_buffers;
        }

        uint32_t binding_mask = (1 << binding_sets.size()) - 1;
        if ((binding_update_mask & binding_mask) == binding_mask)
        {
            // Only reset this flag when this function has gone over all the binging sets. 
            _any_volatile_constant_buffer_writes = false;
        }
        
        return true;
    }

    bool DX12CommandList::set_compute_bindings(
        const PipelineStateBindingSetArray& binding_sets, 
        uint32_t binding_update_mask, 
        DX12RootSignature* dx12_root_signature
    )
    {
        if (binding_update_mask > 0)
        {
            std::vector<VolatileConstantBufferBinding> new_compute_volatile_constant_buffers;

            for (uint32_t ix = 0; ix < binding_sets.size(); ++ix)
            {
                BindingSetInterface* binding_set = binding_sets[ix];
                if (binding_set == nullptr) continue;

                const bool update_set = (binding_update_mask & (1 << ix)) != 0;

                const auto& layout_index = dx12_root_signature->binding_layout_map[ix];
                uint32_t root_param_offest = layout_index.second;

                if (!binding_set->is_bindless())
                {
                    BindingLayoutInterface* temp_binding_layout_for_compare = binding_set->get_layout();
                    ReturnIfFalse(temp_binding_layout_for_compare != nullptr);
                    
                    if (temp_binding_layout_for_compare != layout_index.first) return false;

                    DX12BindingSet* dx12_binding_set = check_cast<DX12BindingSet*>(binding_set);

                    for (
                        uint32_t constant_buffer_index = 0; 
                        constant_buffer_index < dx12_binding_set->root_param_index_volatile_constant_buffers.size(); 
                        ++constant_buffer_index
                    )
                    {
                        BufferInterface* volatile_constant_buffer = 
                            dx12_binding_set->root_param_index_volatile_constant_buffers[constant_buffer_index].second;
                        uint32_t constant_buffer_root_param_index = 
                            dx12_binding_set->root_param_index_volatile_constant_buffers[constant_buffer_index].first;
                        
                        // Plus the offset. 
                        constant_buffer_root_param_index += root_param_offest;

                        if (volatile_constant_buffer != nullptr)
                        {
                            if (volatile_constant_buffer->get_desc().is_volatile)
                            {
                                D3D12_GPU_VIRTUAL_ADDRESS gpu_address = _volatile_constant_buffer_addresses[volatile_constant_buffer];

                                if (gpu_address == 0)
                                {
                                    LOG_ERROR("Attempted use of a volatile constant buffer before it was written into. ");
                                    return false;
                                }
                                
                                if (update_set || gpu_address != _current_compute_volatile_constant_buffers[new_compute_volatile_constant_buffers.size()].gpu_address)
                                {
                                    active_cmdlist->d3d12_cmdlist->SetComputeRootConstantBufferView(constant_buffer_root_param_index, gpu_address);
                                }
                                new_compute_volatile_constant_buffers.emplace_back(
                                    VolatileConstantBufferBinding{ 
                                        constant_buffer_root_param_index, 
                                        volatile_constant_buffer, 
                                        gpu_address 
                                    }
                                );
                            }
                            else if (update_set)
                            {
                                D3D12_GPU_VIRTUAL_ADDRESS gpu_address = 
                                    reinterpret_cast<ID3D12Resource*>(volatile_constant_buffer->get_native_object())->GetGPUVirtualAddress();
                                if (gpu_address == 0)
                                {
                                    LOG_ERROR("Attempted use of a volatile constant buffer before it was written into. ");
                                    return false;
                                }
                                active_cmdlist->d3d12_cmdlist->SetComputeRootConstantBufferView(constant_buffer_root_param_index, gpu_address);
                            }
                        }
                        else if (update_set)
                        {
                            // This can only happen as a result of an improperly built binding set. 
                            // Such binding set should fail to create.
                            active_cmdlist->d3d12_cmdlist->SetComputeRootConstantBufferView(constant_buffer_root_param_index, 0);
                        }
                    }

                    if (update_set)
                    {
                        if (dx12_binding_set->is_descriptor_table_sampler_valid)
                        {
                            active_cmdlist->d3d12_cmdlist->SetComputeRootDescriptorTable(
                                root_param_offest + dx12_binding_set->root_param_sampler_index,
                                _descriptor_heaps->sampler_heap.get_gpu_handle(dx12_binding_set->descriptor_table_sampler_base_index));
                        }

                        if (dx12_binding_set->is_descriptor_table_srv_etc_valid)
                        {
                            active_cmdlist->d3d12_cmdlist->SetComputeRootDescriptorTable(
                                root_param_offest + dx12_binding_set->root_param_srv_etc_index,
                                _descriptor_heaps->shader_resource_heap.get_gpu_handle(dx12_binding_set->descriptor_table_srv_etc_base_index));
                        }
                        
                        if (binding_set->is_bindless()) return false;
                        BindingSetDesc desc = binding_set->get_desc();
                        _cmdlist_ref_instances->ref_resources.emplace_back(binding_set);
                    }

                    if ((update_set || dx12_binding_set->has_uav_bingings)) // UAV bindings may place UAV barriers on the same binding set
                    {
                        set_resource_state_from_binding_set(binding_set);
                    }
                }
                else
                {
                    DX12BindlessSet* dx12_descriptor_table = check_cast<DX12BindlessSet*>(binding_set);

                    active_cmdlist->d3d12_cmdlist->SetComputeRootDescriptorTable(
                        layout_index.second, 
                        _descriptor_heaps->shader_resource_heap.get_gpu_handle(dx12_descriptor_table->first_descriptor_index)
                    );
                }
            }
            _current_compute_volatile_constant_buffers = new_compute_volatile_constant_buffers;
        }

        uint32_t binding_mask = (1 << binding_sets.size()) - 1;
        if ((binding_update_mask & binding_mask) == binding_mask)
        {
            // Only reset this flag when this function has gone over all the binging sets. 
            _any_volatile_constant_buffer_writes = false;
        }
        
        return true;
    }

    void DX12CommandList::clear_state_cache()
    {
        _any_volatile_constant_buffer_writes = false;
        _current_graphics_state_valid = false;
        _current_compute_state_valid = false;
        _current_srv_etc_heap = nullptr;
        _current_sampler_heap = nullptr;
        _current_graphics_volatile_constant_buffers.clear();
        _current_compute_volatile_constant_buffers.clear();
    }

    bool DX12CommandList::bind_graphics_pipeline(GraphicsPipelineInterface* graphics_pipeline, bool update_root_signature) const
    {
        if (graphics_pipeline == nullptr) return false;

        DX12GraphicsPipeline* dx12_graphics_pipeline = check_cast<DX12GraphicsPipeline*>(graphics_pipeline);

        GraphicsPipelineDesc desc = graphics_pipeline->get_desc();
        
        if (update_root_signature)
        {
            active_cmdlist->d3d12_cmdlist->SetGraphicsRootSignature(dx12_graphics_pipeline->dx12_root_signature->d3d12_root_signature.Get());
        }

        active_cmdlist->d3d12_cmdlist->SetPipelineState(reinterpret_cast<ID3D12PipelineState*>(dx12_graphics_pipeline->get_native_object()));

        active_cmdlist->d3d12_cmdlist->IASetPrimitiveTopology(convert_primitive_type(desc.PrimitiveType, desc.dwPatchControlPoints));

        return true;
    }
    
    bool DX12CommandList::bind_frame_buffer(FrameBufferInterface* frame_buffer)
    {
		set_resource_state_from_frame_buffer(frame_buffer);

        DX12FrameBuffer* dx12_frame_buffer = check_cast<DX12FrameBuffer*>(frame_buffer);

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
        for (uint32_t ix : dx12_frame_buffer->_rtv_indices)
        {
            rtvs.emplace_back(_descriptor_heaps->render_target_heap.get_cpu_handle(ix));
        }

        bool has_depth_stencil = dx12_frame_buffer->_dsv_index != INVALID_SIZE_32;
        D3D12_CPU_DESCRIPTOR_HANDLE dsv;
        if (has_depth_stencil)
        {
            dsv = _descriptor_heaps->depth_stencil_heap.get_cpu_handle(dx12_frame_buffer->_dsv_index);
        }

        active_cmdlist->d3d12_cmdlist->OMSetRenderTargets(
            static_cast<uint32_t>(dx12_frame_buffer->_rtv_indices.size()),
            rtvs.data(),
            false,
            has_depth_stencil ? &dsv : nullptr
        );

        return true;
    }
    
    std::shared_ptr<DX12InternalCommandList> DX12CommandList::create_internal_cmdlist() const
    {
        std::shared_ptr<DX12InternalCommandList> ret = std::make_shared<DX12InternalCommandList>();

        D3D12_COMMAND_LIST_TYPE cmdlist_type;
        switch (_desc.queue_type)
        {
        case CommandQueueType::Graphics: cmdlist_type = D3D12_COMMAND_LIST_TYPE_DIRECT; break;
        case CommandQueueType::Compute:  cmdlist_type = D3D12_COMMAND_LIST_TYPE_COMPUTE; break;
        case CommandQueueType::Copy:     cmdlist_type = D3D12_COMMAND_LIST_TYPE_COPY; break;
        default: 
            assert(!"invalid Enumeration value");
            return nullptr;
        }

        _context->device->CreateCommandAllocator(cmdlist_type, IID_PPV_ARGS(ret->d3d12_cmd_allocator.GetAddressOf()));
        _context->device->CreateCommandList(
            0, 
            cmdlist_type, 
            ret->d3d12_cmd_allocator.Get(), 
            nullptr, 
            IID_PPV_ARGS(ret->d3d12_cmdlist.GetAddressOf())
        );

        ret->d3d12_cmdlist->QueryInterface(IID_PPV_ARGS(ret->d3d12_cmdlist4.GetAddressOf()));
        ret->d3d12_cmdlist->QueryInterface(IID_PPV_ARGS(ret->d3d12_cmdlist6.GetAddressOf()));

        return ret;
    }

    ray_tracing::DX12ShaderTableState* DX12CommandList::get_shader_tabel_state(ray_tracing::ShaderTableInterface* shader_table)
    {
        auto iter = _shader_table_states.find(shader_table);
        if (iter != _shader_table_states.end())
        {
            return iter->second.get();
        }

        std::unique_ptr<ray_tracing::DX12ShaderTableState> shader_table_state = std::make_unique<ray_tracing::DX12ShaderTableState>();
        ray_tracing::DX12ShaderTableState* ret = shader_table_state.get();

        _shader_table_states.insert(std::make_pair(shader_table, std::move(shader_table_state)));
        return ret;
    }

}
