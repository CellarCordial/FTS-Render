#include "ThreadPool.h"
#include <chrono>
#include <memory>
#include <mutex>

namespace FTS 
{
    FThreadPool::FThreadPool(UINT32 InThreadNum)
    {
        UINT64 MaxThreadNum = std::thread::hardware_concurrency() / 4;
        if (InThreadNum > 0) MaxThreadNum = InThreadNum;
        
        for (UINT64 ix = 0; ix < MaxThreadNum; ++ix)
        {
            m_Threads.emplace_back(&FThreadPool::WorkerThread, this, ix);
        }
    }

    FThreadPool::~FThreadPool()
    {
        {
            m_Done = true;
            m_PoolTaskQueue.ConditionVariable.notify_all();
        }
        
        for (auto& Thread : m_Threads)
        {
            if (Thread.joinable())
            {
                Thread.join();
            }
        }
    }

    UINT64 FThreadPool::Submit(std::function<BOOL()> Func)
    {
        auto Task = std::make_shared<std::packaged_task<BOOL()>>(Func);
        m_Futures.emplace_back(Task->get_future());
        m_PoolTaskQueue.Push([Task]() { (*Task)(); });

        return m_Futures.size() - 1;
    }

    BOOL FThreadPool::WaitForIdle()
    {
        m_Futures.remove_if(
            [](auto& crFuture)
            {
                return crFuture.get();
            }
        );
        return m_Futures.empty();
    }

    BOOL FThreadPool::ThreadFinished(UINT64 stIndex)
    {
        return std::next(m_Futures.begin(), stIndex)->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
    }

    BOOL FThreadPool::ThreadSuccess(UINT64 stIndex)
    {
        return std::next(m_Futures.begin(), stIndex)->get();
    }

    void FThreadPool::ParallelFor(std::function<void(UINT64)> Func, UINT64 stCount, UINT32 dwChunkSize)
    {
        for (UINT64 ix = 0; ix < stCount; ix += dwChunkSize)
        {
            auto Task = std::make_shared<std::packaged_task<BOOL()>>(
                [&Func, ix, stCount, dwChunkSize]() 
                {
                    UINT32 dwEndIndex = std::min(ix + dwChunkSize, stCount);
                    for (UINT64 ij = ix; ij < dwEndIndex; ++ij)
                    {
                        Func(ij);
                    }
                    return true;
                }
            );
            m_Futures.emplace_back(Task->get_future());
            m_PoolTaskQueue.Push([Task]() { (*Task)(); });
        }

        WaitForIdle();
    }
    
    void FThreadPool::ParallelFor(std::function<void(UINT64, UINT64)> Func, UINT64 stX, UINT64 stY)
    {
        for (UINT64 iy = 0; iy < stY; ++iy)
        {
            auto Task = std::make_shared<std::packaged_task<BOOL()>>(
                [&Func, iy, stX]() 
                {
                    for (UINT64 ix = 0; ix < stX; ++ix)
                    {
                        Func(ix, iy);
                    }
                    return true;
                }
            );
            m_Futures.emplace_back(Task->get_future());
            m_PoolTaskQueue.Push([Task]() { (*Task)(); });
        }
        WaitForIdle();
    }

    void FThreadPool::WorkerThread(UINT64 stIndex)
    {
        std::mutex m_CvMutex;
        std::unique_lock<std::mutex> Lock(m_CvMutex);
        while (!m_Done)
        {
            std::function<void()> Task;
            if (m_PoolTaskQueue.TryPop(Task))
            {
                Task();
            }
            else
            {
                m_PoolTaskQueue.ConditionVariable.wait(Lock);
            }
        }
    }

}