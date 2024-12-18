#include "thread_pool.h"
#include <chrono>
#include <memory>
#include <mutex>

namespace fantasy 
{
    ThreadPool::ThreadPool(uint32_t thread_num)
    {
        uint64_t max_thread_num = std::thread::hardware_concurrency() / 4;
        if (thread_num > 0) max_thread_num = thread_num;
        
        for (uint64_t ix = 0; ix < max_thread_num; ++ix)
        {
            _threads.emplace_back(&ThreadPool::worker_thread, this, ix);
        }
    }

    ThreadPool::~ThreadPool()
    {
        {
            _done = true;
            _pool_task_queue.condition_variable.notify_all();
        }
        
        for (auto& thread : _threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
    }

    uint64_t ThreadPool::submit(std::function<bool()> func)
    {
        auto task = std::make_shared<std::packaged_task<bool()>>(func);
        _futures.emplace_back(task->get_future());
        _pool_task_queue.push([task]() { (*task)(); });

        return _futures.size() - 1;
    }

    void ThreadPool::wait_for_idle(uint32_t index)
    {
        while (index-- > 0)
        {
            if (_futures.back().get())
            {
                _futures.pop_back();
            }
        }
    }

    bool ThreadPool::thread_finished(uint64_t index)
    {
        return _futures[index].wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
    }

    bool ThreadPool::thread_success(uint64_t index)
    {
        auto iter = std::next(_futures.begin(), index);
        if (iter->get())
        {
            _futures.erase(iter);
            return true;
        }
        return false;
    }

    void ThreadPool::parallel_for(std::function<void(uint64_t)> func, uint64_t count, uint32_t chun_size)
    {
        for (uint64_t ix = 0; ix < count; ix += chun_size)
        {
            auto task = std::make_shared<std::packaged_task<bool()>>(
                [&func, ix, count, chun_size]() 
                {
                    uint32_t dwEndIndex = std::min(ix + chun_size, count);
                    for (uint64_t ij = ix; ij < dwEndIndex; ++ij)
                    {
                        func(ij);
                    }
                    return true;
                }
            );
            _futures.emplace_back(task->get_future());
            _pool_task_queue.push([task]() { (*task)(); });
        }

        wait_for_idle(count);
    }
    
    void ThreadPool::parallel_for(std::function<void(uint64_t, uint64_t)> func, uint64_t x, uint64_t y)
    {
        for (uint64_t iy = 0; iy < y; ++iy)
        {
            auto task = std::make_shared<std::packaged_task<bool()>>(
                [&func, iy, x]() 
                {
                    for (uint64_t ix = 0; ix < x; ++ix)
                    {
                        func(ix, iy);
                    }
                    return true;
                }
            );
            _futures.emplace_back(task->get_future());
            _pool_task_queue.push([task]() { (*task)(); });
        }
        wait_for_idle(y);
    }

    void ThreadPool::worker_thread(uint64_t index)
    {
        std::mutex mutex;
        std::unique_lock<std::mutex> Lock(mutex);
        while (!_done)
        {
            std::function<void()> Task;
            if (_pool_task_queue.try_pop(Task))
            {
                Task();
            }
            else
            {
                _pool_task_queue.condition_variable.wait(Lock);
            }
        }
    }

}