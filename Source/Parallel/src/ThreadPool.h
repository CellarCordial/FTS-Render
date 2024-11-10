#ifndef TASK_FLOW_THREAD_POOL_H
#define TASK_FLOW_THREAD_POOL_H

#include <functional>
#include <future>
#include <vector>

#include "ThreadQueue.h"

namespace FTS 
{
    class FThreadPool
    {
    public:
        FThreadPool(UINT32 InThreadNum = 0);
        ~FThreadPool();

        UINT64 Submit(std::function<BOOL()> Func);
        BOOL WaitForIdle();

        BOOL ThreadFinished(UINT64 stIndex);
        BOOL ThreadSuccess(UINT64 stIndex);

        void ParallelFor(std::function<void(UINT64)> Func, UINT64 stCount, UINT32 dwChunkSize = 1);
        void ParallelFor(std::function<void(UINT64, UINT64)> Func, UINT64 stX, UINT64 stY);

    private:
        void WorkerThread(UINT64 stIndex);

    private:
        std::atomic<BOOL> m_Done = false;

        std::vector<std::thread> m_Threads;
        std::list<std::future<BOOL>> m_Futures;
        TConcurrentQueue<std::function<void()>> m_PoolTaskQueue;
    };


}


#endif