#ifndef DYNAMIC_RHI_VULKAN_CMDLIST_H
#define DYNAMIC_RHI_VULKAN_CMDLIST_H

#include "vk_forward.h"
#include "../command_list.h"
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


    class VKCommandList : public CommandListInterface
    {
    public:

        std::shared_ptr<VKCommandBuffer> get_current_command_buffer() const;

    };
}









#endif