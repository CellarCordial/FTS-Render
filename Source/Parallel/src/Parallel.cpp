#include "../include/Parallel.h"
#include <functional>
#include <queue>
#include "ThreadPool.h"
#include "../../Core/include/ComRoot.h"
#include "../../Math/include/Common.h"

namespace FTS 
{
    namespace Parallel 
    {
        template <typename T>
        class TTaskQueue
        {
        public:
            void Push(const T& task)
            {
                std::unique_lock<std::mutex> lock(Mutex);
                Queue.push(task);
                ConditionVariable.notify_one();
            }
            
            T Pop()
            {
                std::unique_lock<std::mutex> lock(Mutex);
                T msg = Queue.front();
                Queue.pop();
                return msg;
            }
            
            void Wait()
            {
                std::unique_lock<std::mutex> lock(Mutex);
                ConditionVariable.wait(lock, [this]() {
                    return (!Queue.empty());
                    });
            }

        private:
            std::condition_variable ConditionVariable;
            std::mutex Mutex;
            std::queue<T> Queue;
        };



        static TTaskQueue<FTaskNode*> gNodeQueue;
        static std::unique_ptr<FThreadPool> gpPool;

        void Initialize()
        {
            gpPool = std::make_unique<FThreadPool>();
        }

        void Destroy()
        {
            gpPool.reset(nullptr);
        }

        void ParallelFor(std::function<void(UINT64)> Func, UINT64 stCount, UINT32 dwChunkSize)
        {
            if (stCount == 0) return;

            assert(stCount >= dwChunkSize);
            gpPool->ParallelFor(Func, stCount, dwChunkSize);
        }

        void ParallelFor(std::function<void(UINT64, UINT64)> Func, UINT64 stX, UINT64 stY)
        {
			if (stX == 0 || stY == 0) return;

            gpPool->ParallelFor(Func, stX, stY);
        }

        BOOL ThreadFinished(UINT64 stIndex)
        {
            if (stIndex == INVALID_SIZE_64) return false;
            return gpPool->ThreadFinished(stIndex);
        }

        BOOL ThreadSuccess(UINT64 stIndex)
        {
			if (stIndex == INVALID_SIZE_64) return false;
			return gpPool->ThreadSuccess(stIndex);
        }

        UINT64 BeginThread(std::function<BOOL()>&& rrFunc)
        {
            return gpPool->Submit(std::move(rrFunc));
        }

        BOOL Run(FTaskFlow& InFlow)
        {
            ReturnIfFalse(!InFlow.Empty());

            UINT32 TotalUnfinishedTaskNum = InFlow.TotalTaskNum;

            const auto& SrcNodes = InFlow.GetSrcNodes();
            for (const auto& Node : SrcNodes)
            {
                gpPool->Submit(
                    [Node]() -> BOOL
                    {
                        ReturnIfFalse(Node->Run());
                        gNodeQueue.Push(Node);     // 入队就说明已经完成了
                        return true;
                    }
                );
            }
            
            while (TotalUnfinishedTaskNum > 0)
            {
                gNodeQueue.Wait();
                TotalUnfinishedTaskNum--;

                FTaskNode* Node = gNodeQueue.Pop();
                for (const auto& Successor : Node->Successors)
                {
                    Successor->UnfinishedDependentTaskNum--;
                    if (Successor->UnfinishedDependentTaskNum == 0)
                    {
                        gpPool->Submit(
                            [Successor]() -> BOOL
                            {
                                ReturnIfFalse(Successor->Run());
                                gNodeQueue.Push(Successor);
                                return true;
                            }
                        );
                        Successor->UnfinishedDependentTaskNum = Successor->UnfinishedDependentTaskNumBackup;    // It keeps the TaskFlow's initial state
                    }
                }
            }

            return gpPool->WaitForIdle();
        }
    }

}