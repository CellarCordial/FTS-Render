#ifndef TASK_FLOW_THREAD_POOL_H
#define TASK_FLOW_THREAD_POOL_H

#include <functional>
#include <future>
#include <vector>

#include "thread_queue.h"

namespace fantasy 
{
    class ThreadPool
    {
    public:
        ThreadPool(uint32_t thread_num = 0);
        ~ThreadPool();

        uint64_t submit(std::function<bool()> func);
        void wait_for_idle(uint32_t index = 1);

        bool thread_finished(uint64_t index);
        bool thread_success(uint64_t index);

        void parallel_for(std::function<void(uint64_t)> func, uint64_t count, uint32_t chun_size = 1);
        void parallel_for(std::function<void(uint64_t, uint64_t)> func, uint64_t x, uint64_t y);

    private:
        void worker_thread(uint64_t index);

    private:
        std::atomic<bool> _done = false;

        std::vector<std::thread> _threads;
        std::vector<std::future<bool>> _futures;
        ConcurrentQueue<std::function<void()>> _pool_task_queue;
    };


}


#endif