#ifndef RHI_STATE_TRACKER_H
#define RHI_STATE_TRACKER_H

#include "Resource.h"
#include <memory>
#include <unordered_map>
#include <vector>

 namespace fantasy 
 {
    struct TextureState
    {
        ResourceStates state = ResourceStates::Common;
        std::vector<ResourceStates> subresource_states;
        bool enable_uav_barriers = false;
        bool uav_barrier_placed = false;
    };

    struct BufferState
    {
        ResourceStates state = ResourceStates::Common;
        bool enable_uav_barriers = false;
        bool uav_barrier_placed = false;
    };

    struct TextureBarrier
    {
        TextureInterface* texture = nullptr;
        uint32_t mip_level = 0;
        uint32_t array_slice = 0;
        bool is_entire_texture = true;

        ResourceStates state_before = ResourceStates::Common;
        ResourceStates state_after = ResourceStates::Common;
    };

    struct BufferBarrier
    {
        BufferInterface* buffer = nullptr;

        ResourceStates state_before = ResourceStates::Common;
        ResourceStates state_after = ResourceStates::Common;
    };

    class ResourceStateTracker
    {
    public:
        void set_texture_enable_uav_barriers(TextureInterface* texture, bool enable_barriers);
        void set_buffer_enable_uav_barriers(BufferInterface* buffer, bool enable_barriers);

        ResourceStates get_texture_state(TextureInterface* texture, uint32_t array_slice, uint32_t mip_level);
        ResourceStates get_buffer_state(BufferInterface* buffer);

        void set_texture_state(TextureInterface* texture, const TextureSubresourceSet& subresource_set, ResourceStates state);
        void set_buffer_state(BufferInterface* buffer, ResourceStates state);
        
        const std::vector<TextureBarrier>& get_texture_barriers() const;
        const std::vector<BufferBarrier>& get_buffer_barriers() const;

        void clear_barriers();

    private:
        TextureState* get_texture_state_track(TextureInterface* texture);
        BufferState* get_buffer_state_track(BufferInterface* buffer);

    public:
        std::unordered_map<TextureInterface*, std::unique_ptr<TextureState>> texture_states;
        std::unordered_map<BufferInterface*, std::unique_ptr<BufferState>> buffer_states;
    
    private:

        std::vector<TextureBarrier> _texture_barriers;
        std::vector<BufferBarrier> _buffer_barriers;
    };
 }

 #endif