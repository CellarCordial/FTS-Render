#include "state_track.h"
#include <memory>
#include <utility>
#include <vector>
#include "Resource.h"


namespace fantasy 
{
    void ResourceStateTracker::set_texture_enable_uav_barriers(TextureInterface* texture, bool enable_barriers)
    {
        TextureState* state_track = get_texture_state_track(texture);
        state_track->enable_uav_barriers = enable_barriers;
        state_track->uav_barrier_placed = false;
    }

    void ResourceStateTracker::set_buffer_enable_uav_barriers(BufferInterface* buffer, bool enable_barriers)
    {
        BufferState* state_track = get_buffer_state_track(buffer);
        state_track->enable_uav_barriers = enable_barriers;
        state_track->uav_barrier_placed = false;
    }

    ResourceStates ResourceStateTracker::get_texture_state(TextureInterface* texture, uint32_t array_slice, uint32_t mip_level)
    {
        auto state_track = get_texture_state_track(texture);

        const TextureDesc& texture_desc = texture->get_desc();

        uint32_t subresource_index = calculate_texture_subresource(mip_level, array_slice, texture_desc.mip_levels);
        return state_track->subresource_states[subresource_index];
    }

    ResourceStates ResourceStateTracker::get_buffer_state(BufferInterface* buffer)
    {
        return get_buffer_state_track(buffer)->state;
    }

    void ResourceStateTracker::set_texture_state(TextureInterface* texture, const TextureSubresourceSet& subresource_set, ResourceStates state)
    {
        const TextureDesc& desc = texture->get_desc();

        TextureState* track_state = get_texture_state_track(texture);
        if (subresource_set.is_entire_texture(desc) && track_state->subresource_states.size() == 1)
        {
            bool is_transition_necessary = (track_state->state & state) != state;
            bool is_uav_necessary = ((state & ResourceStates::UnorderedAccess) == ResourceStates::UnorderedAccess) &&
                                   (track_state->enable_uav_barriers && !track_state->uav_barrier_placed);
            if (is_transition_necessary || is_uav_necessary)
            {
                TextureBarrier barrier;
                barrier.texture = texture;
                barrier.is_entire_texture = true;
                barrier.state_before = track_state->state;
                barrier.state_after = state;
                _texture_barriers.push_back(barrier);
            }
            
            if (is_transition_necessary) track_state->state = state;

            if (!is_transition_necessary && is_uav_necessary)
            {
                track_state->uav_barrier_placed = true;
            }
        }
        else 
        {
            bool any_uav_barrier = false;
            for (uint32_t slice = subresource_set.base_array_slice; slice < subresource_set.base_array_slice + subresource_set.array_slice_count; ++slice)
            {
                for (uint32_t mip = subresource_set.base_mip_level; mip < subresource_set.base_mip_level + subresource_set.mip_level_count; ++mip)
                {
                    uint32_t subresource_index = calculate_texture_subresource(mip, slice, desc.mip_levels);
                    ResourceStates prior_state = track_state->subresource_states[subresource_index];
                    
                    bool is_transition_necessary = (prior_state & state) != state;
                    bool is_uav_necessary = ((state & ResourceStates::UnorderedAccess) != 0) && 
                                            (track_state->enable_uav_barriers && !track_state->uav_barrier_placed) &&
                                            !any_uav_barrier;

                    if (is_transition_necessary || is_uav_necessary)
                    {
                        TextureBarrier barrier;
                        barrier.texture = texture;
                        barrier.is_entire_texture = false;
                        barrier.mip_level = mip;
                        barrier.array_slice = slice;
                        barrier.state_before = prior_state;
                        barrier.state_after = state;
                        _texture_barriers.push_back(barrier);
                    }
                    
                    if (is_transition_necessary) track_state->subresource_states[subresource_index] = state;

                    if (!is_transition_necessary && is_uav_necessary)
                    {
                        any_uav_barrier = true;
                        track_state->uav_barrier_placed = true;
                    }
                }
            }
        }
    }
    
    void ResourceStateTracker::set_buffer_state(BufferInterface* buffer, ResourceStates state)
    {
        const BufferDesc& desc = buffer->get_desc();

        BufferState* track = get_buffer_state_track(buffer);

        bool is_transition_necessary = (track->state & state) != state;
        if (is_transition_necessary)
        {
            for (auto& barrier : _buffer_barriers)
            {
                if (barrier.buffer == buffer)
                {
                    barrier.state_after = static_cast<ResourceStates>(barrier.state_after | state);
                    track->state = barrier.state_after;
                    return;
                }
            }
        }

        bool is_uav_necessary = ((state & ResourceStates::UnorderedAccess) == ResourceStates::UnorderedAccess) && 
                               (track->enable_uav_barriers && !track->uav_barrier_placed);
        if (is_transition_necessary || is_uav_necessary)
        {
            BufferBarrier barrier;
            barrier.buffer = buffer;
            barrier.state_before = track->state;
            barrier.state_after = state;
            _buffer_barriers.push_back(barrier);
        }
        
        if (is_transition_necessary) track->state = state;

        if (!is_transition_necessary && is_uav_necessary)
        {
            track->uav_barrier_placed = true;
        }
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

        std::unique_ptr<TextureState> track = std::make_unique<TextureState>();
        TextureState* texture_state_track = track.get();
        _texture_states.insert(std::make_pair(texture, std::move(track)));
        
        const TextureDesc& desc = texture->get_desc();
        texture_state_track->state = ResourceStates::Common;
        texture_state_track->subresource_states.resize(static_cast<size_t>(desc.mip_levels * desc.array_size), texture_state_track->state);

        return texture_state_track;
    }

    BufferState* ResourceStateTracker::get_buffer_state_track(BufferInterface* buffer)
    {
        auto iter = _buffer_states.find(buffer);
        if (iter != _buffer_states.end()) return iter->second.get();

        std::unique_ptr<BufferState> track = std::make_unique<BufferState>();
        BufferState* buffer_state_track = track.get();
        _buffer_states.insert(std::make_pair(buffer, std::move(track)));
        
        const BufferDesc& desc = buffer->get_desc();
        buffer_state_track->state = ResourceStates::Common;

        return buffer_state_track;
    }

}