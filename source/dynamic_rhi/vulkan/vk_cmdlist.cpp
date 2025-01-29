#include "vk_cmdlist.h"
#include "../../core/tools/check_cast.h"
#include <cstdint>
#include <memory>

namespace fantasy 
{
    VKCommandQueue::VKCommandQueue(
        const VKContext* context, 
        CommandQueueType queue_type, 
        vk::Queue vulkan_queue, 
        uint32_t queue_family_index
    ) : 
        _context(context), 
        _vulkan_queue(vulkan_queue), 
        _queue_type(queue_type), 
        _queue_family_index(queue_family_index)
    {
        vk::SemaphoreTypeCreateInfo semaphore_type_info;
        semaphore_type_info.semaphoreType = vk::SemaphoreType::eTimeline;
        
        vk::SemaphoreCreateInfo semaphore_info;
        semaphore_type_info.pNext = &semaphore_type_info;

        tracking_semaphore = context->device.createSemaphore(semaphore_info, context->allocation_callbacks);
    }

    VKCommandQueue::~VKCommandQueue()
    {
        _context->device.destroySemaphore(tracking_semaphore, _context->allocation_callbacks);
    }

    std::shared_ptr<VKCommandBuffer> VKCommandQueue::create_command_buffer()
    {
        std::shared_ptr<VKCommandBuffer> ret = std::make_shared<VKCommandBuffer>(_context);

        vk::CommandPoolCreateInfo cmd_pool_create_info;
        cmd_pool_create_info.queueFamilyIndex = _queue_family_index;
        cmd_pool_create_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
                                     vk::CommandPoolCreateFlagBits::eTransient;

        if (
            vk::Result::eSuccess != _context->device.createCommandPool(
            &cmd_pool_create_info, 
            _context->allocation_callbacks, 
            &ret->cmd_pool
        ))
        {
            LOG_ERROR("Create command buffer failed.");
            return nullptr;
        }
        
        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = ret->cmd_pool;
        alloc_info.commandBufferCount = 1;
        if (_context->device.allocateCommandBuffers(&alloc_info, &ret->cmd_buffer) != vk::Result::eSuccess)
        {
            LOG_ERROR("Create command buffer failed.");
            return nullptr;
        }

        return ret;
    }

    std::shared_ptr<VKCommandBuffer> VKCommandQueue::get_command_buffer()
    {
        std::lock_guard lock(_mutex);

        uint64_t recording_id = ++_last_recording_id;

        std::shared_ptr<VKCommandBuffer> cmdBuf;
        if (_cmd_buffers_pool.empty())
        {
            cmdBuf = create_command_buffer();
        }
        else
        {
            cmdBuf = _cmd_buffers_pool.front();
            _cmd_buffers_pool.pop_front();
        }

        cmdBuf->recording_id = recording_id;
        return cmdBuf;
    }


    void VKCommandQueue::add_wait_semaphore(vk::Semaphore semaphore, uint64_t value)
    {
        if (!semaphore) return;

        _wait_semaphores.push_back(semaphore);
        _wait_semaphore_values.push_back(value);
    }

    void VKCommandQueue::add_signal_semaphore(vk::Semaphore semaphore, uint64_t value)
    {
        if (!semaphore) return;

        _signal_semaphores.push_back(semaphore);
        _signal_semaphore_values.push_back(value);
    }

    uint64_t VKCommandQueue::submit(CommandListInterface* const* cmdlist, uint64_t cmd_count)
    {
        std::vector<vk::PipelineStageFlags> waitStageArray(_wait_semaphores.size());
        std::vector<vk::CommandBuffer> commandBuffers(cmd_count);

        for (size_t i = 0; i < _wait_semaphores.size(); i++)
        {
            waitStageArray[i] = vk::PipelineStageFlagBits::eTopOfPipe;
        }

        _last_submitted_id++;

        for (size_t i = 0; i < cmd_count; i++)
        {
            VKCommandList* vk_cmdlist = check_cast<VKCommandList*>(cmdlist[i]);
            std::shared_ptr<VKCommandBuffer> commandBuffer = vk_cmdlist->get_current_command_buffer();

            commandBuffers[i] = commandBuffer->cmd_buffer;
            _cmd_buffers_in_flight.push_back(commandBuffer);

            for (const auto& buffer : commandBuffer->ref_staging_buffers)
            {
                assert(false);
                // buffer->lastUseQueue = _queue_type;
                // buffer->lastUseCommandListID = _last_submitted_id;
            }
        }
        
        _signal_semaphores.push_back(tracking_semaphore);
        _signal_semaphore_values.push_back(_last_submitted_id);

        vk::TimelineSemaphoreSubmitInfo timelineSemaphoreInfo;
        timelineSemaphoreInfo.signalSemaphoreValueCount = static_cast<uint32_t>(_signal_semaphore_values.size());
        timelineSemaphoreInfo.pSignalSemaphoreValues = _signal_semaphore_values.data();

        if (!_wait_semaphore_values.empty()) 
        {
            timelineSemaphoreInfo.waitSemaphoreValueCount = static_cast<uint32_t>(_wait_semaphore_values.size());
            timelineSemaphoreInfo.pWaitSemaphoreValues = _wait_semaphore_values.data();
        }

        vk::SubmitInfo submitInfo;
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(_signal_semaphores.size());
        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(_wait_semaphores.size());
        submitInfo.commandBufferCount = static_cast<uint32_t>(cmd_count);
        submitInfo.pSignalSemaphores = _signal_semaphores.data();
        submitInfo.pWaitSemaphores = _wait_semaphores.data();
        submitInfo.pWaitDstStageMask = waitStageArray.data();
        submitInfo.pCommandBuffers = commandBuffers.data();
        submitInfo.pNext = &timelineSemaphoreInfo;

        try 
        {
            _vulkan_queue.submit(submitInfo);
        }
        catch (vk::DeviceLostError e)
        {
            LOG_ERROR("Device moved!");
            return INVALID_SIZE_64;
        }

        _wait_semaphores.clear();
        _wait_semaphore_values.clear();
        _signal_semaphores.clear();
        _signal_semaphore_values.clear();
        
        return _last_submitted_id;
    }

}