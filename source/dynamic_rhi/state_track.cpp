#include "state_track.h"
#include <memory>
#include <utility>
#include <vector>
#include "../core/tools/log.h"


namespace fantasy 
{
    void ResourceStateTracker::set_texture_enable_uav_barriers(TextureInterface* texture, bool enable_barriers)
    {
        TextureState* pStatesTrack = get_texture_state_track(texture);
        pStatesTrack->enable_uav_barriers = enable_barriers;
        pStatesTrack->first_uav_barrier_placed = false;
    }

    void ResourceStateTracker::set_buffer_enable_uav_barriers(BufferInterface* buffer, bool enable_barriers)
    {
        BufferState* pStatesTrack = get_buffer_state_track(buffer);
        pStatesTrack->enable_uav_barriers = enable_barriers;
        pStatesTrack->first_uav_barrier_placed = false;
    }

    ResourceStates ResourceStateTracker::get_texture_subresource_state(TextureInterface* texture, uint32_t array_slice, uint32_t mip_level)
    {
        auto pTracking = get_texture_state_track(texture);

        TextureDesc TextureDesc = texture->get_desc();

        uint32_t dwSubresourceIndex = calculate_texture_subresource(mip_level, array_slice, TextureDesc);
        return pTracking->subresource_states[dwSubresourceIndex];
    }

    ResourceStates ResourceStateTracker::get_buffer_state(BufferInterface* buffer)
    {
        return get_buffer_state_track(buffer)->state;
    }

    bool ResourceStateTracker::set_texture_state(TextureInterface* texture, const TextureSubresourceSet& subresource_set, ResourceStates state)
    {
        TextureDesc desc = texture->get_desc();
        TextureSubresourceSet resolved_subresource_set = subresource_set.resolve(desc, false);

        TextureState* pTrackState = get_texture_state_track(texture);
        if (resolved_subresource_set.is_entire_texture(desc) && pTrackState->subresource_states.size() == 1)
        {
            bool bIsTransitionNecessary = pTrackState->state != state;
            bool bIsUavNecessary = ((state & ResourceStates::UnorderedAccess) == ResourceStates::UnorderedAccess) &&
                                   (pTrackState->enable_uav_barriers && !pTrackState->first_uav_barrier_placed);
            if (bIsTransitionNecessary || bIsUavNecessary)
            {
                TextureBarrier Barrier;
                Barrier.texture = texture;
                Barrier.is_entire_texture = true;
                Barrier.state_before = pTrackState->state;
                Barrier.state_after = state;
                _texture_barriers.push_back(Barrier);
            }
            
            pTrackState->state = state;

            if (!bIsTransitionNecessary && bIsUavNecessary)
            {
                pTrackState->first_uav_barrier_placed = true;
            }
        }
        else 
        {
            bool bAnyUavBarrier = false;
            for (uint32_t dwArraySliceIndex = resolved_subresource_set.base_array_slice; dwArraySliceIndex < resolved_subresource_set.base_array_slice + resolved_subresource_set.array_slice_count; ++dwArraySliceIndex)
            {
                for (uint32_t dwMipLevelIndex = resolved_subresource_set.base_mip_level; dwMipLevelIndex < resolved_subresource_set.base_mip_level + resolved_subresource_set.mip_level_count; ++dwMipLevelIndex)
                {
                    uint32_t dwSubresourceIndex = calculate_texture_subresource(dwMipLevelIndex, dwArraySliceIndex, desc);
                    ResourceStates PriorState = pTrackState->subresource_states[dwSubresourceIndex];
                    
                    bool bIsTransitionNecessary = PriorState != state;
                    bool bIsUavNecessary = ((state & ResourceStates::UnorderedAccess) != 0) && 
                                            !bAnyUavBarrier && 
                                            (pTrackState->enable_uav_barriers &&
                                            !pTrackState->first_uav_barrier_placed);

                    if (bIsTransitionNecessary || bIsUavNecessary)
                    {
                        TextureBarrier Barrier;
                        Barrier.texture = texture;
                        Barrier.is_entire_texture = false;
                        Barrier.mip_level = dwMipLevelIndex;
                        Barrier.array_slice = dwArraySliceIndex;
                        Barrier.state_before = PriorState;
                        Barrier.state_after = state;
                        _texture_barriers.push_back(Barrier);
                    }
                    
                    pTrackState->subresource_states[dwSubresourceIndex] = state;

                    if (!bIsTransitionNecessary && bIsUavNecessary)
                    {
                        bAnyUavBarrier = true;
                        pTrackState->first_uav_barrier_placed = true;
                    }
                }
            }
        }
        return true;
    }
    
    bool ResourceStateTracker::set_buffer_state(BufferInterface* buffer, ResourceStates state)
    {
        BufferDesc desc = buffer->get_desc();

        // CPU-visible buffers 不能改变 state
        if (desc.is_volatile || desc.cpu_access != CpuAccessMode::None)
        {
            if (get_buffer_state_track(buffer)->state == state)
            {
                return true;
            }
            else
            {
				LOG_ERROR("CPU-visible buffers can't change state.");
				return false;
            }
        }

        BufferState* pTrack = get_buffer_state_track(buffer);

        bool bIsTransitionNecessary = pTrack->state != state;
        if (bIsTransitionNecessary)
        {
            // 若 Barriers 中已经存在同 Buffer 的 Barrier.
            for (auto& rBarrier : _buffer_barriers)
            {
                if (rBarrier.buffer == buffer)
                {
                    rBarrier.state_after = static_cast<ResourceStates>(rBarrier.state_after | state);
                    pTrack->state = rBarrier.state_after;
                }
            }
        }

        bool bIsUavNecessary = ((state & ResourceStates::UnorderedAccess) == ResourceStates::UnorderedAccess) && 
                               (pTrack->enable_uav_barriers || !pTrack->first_uav_barrier_placed);
        if (bIsTransitionNecessary || bIsUavNecessary)
        {
            BufferBarrier Barrier;
            Barrier.buffer = buffer;
            Barrier.state_before = pTrack->state;
            Barrier.state_after = state;
            _buffer_barriers.push_back(Barrier);
        }

        if (!bIsTransitionNecessary && bIsUavNecessary)
        {
            pTrack->first_uav_barrier_placed = true;
        }

        pTrack->state = state;
		return true;
	}

    
    const std::vector<TextureBarrier>& ResourceStateTracker::get_texture_barriers() const
    {
        return _texture_barriers;
    }

    const std::vector<BufferBarrier>& ResourceStateTracker::get_buffer_barriers() const
    {
        return _buffer_barriers;
    }

    void ResourceStateTracker::clear_barriers()
    {
        _texture_barriers.clear();
        _buffer_barriers.clear();
    }

	TextureState* ResourceStateTracker::get_texture_state_track(TextureInterface* texture)
    {
        auto iter = _texture_states.find(texture);
        if (iter != _texture_states.end()) return iter->second.get();

        std::unique_ptr<TextureState> pTrack = std::make_unique<TextureState>();
        TextureState* pTextureStateTrack = pTrack.get();
        _texture_states.insert(std::make_pair(texture, std::move(pTrack)));
        
        TextureDesc desc = texture->get_desc();
        pTextureStateTrack->state = desc.initial_state;
        pTextureStateTrack->subresource_states.resize(static_cast<size_t>(desc.mip_levels * desc.array_size), pTextureStateTrack->state);

        return pTextureStateTrack;
    }

    BufferState* ResourceStateTracker::get_buffer_state_track(BufferInterface* buffer)
    {
        auto iter = _buffer_states.find(buffer);
        if (iter != _buffer_states.end()) return iter->second.get();

        std::unique_ptr<BufferState> pTrack = std::make_unique<BufferState>();
        BufferState* pBufferStateTrack = pTrack.get();
        _buffer_states.insert(std::make_pair(buffer, std::move(pTrack)));
        
        BufferDesc desc = buffer->get_desc();
        pBufferStateTrack->state = desc.initial_state;

        return pBufferStateTrack;
    }

}