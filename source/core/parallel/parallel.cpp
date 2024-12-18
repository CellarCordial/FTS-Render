#include "parallel.h"
#include <cassert>
#include <functional>
#include <queue>
#include "thread_pool.h"
#include "../math/common.h"
#include "../tools/log.h"

namespace fantasy 
{
    namespace parallel 
    {
        template <typename T>
        class TaskQueue
        {
        public:
            void push(const T& task)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _queue.push(task);
                _condition_variable.notify_one();
            }
            
            T pop()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                T msg = _queue.front();
                _queue.pop();
                return msg;
            }
            
            void wait()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _condition_variable.wait(lock, [this]() {
                    return (!_queue.empty());
                    });
            }

        private:
            std::condition_variable _condition_variable;
            std::mutex _mutex;
            std::queue<T> _queue;
        };



        static TaskQueue<TaskNode*> node_queue;
        static std::unique_ptr<ThreadPool> thread_pool;

        void initialize()
        {
            thread_pool = std::make_unique<ThreadPool>();
        }

        void destroy()
        {
            thread_pool.reset(nullptr);
        }

        void parallel_for(std::function<void(uint64_t)> func, uint64_t count, uint32_t chun_size)
        {
            if (count == 0) return;

            assert(count >= chun_size);
            thread_pool->parallel_for(func, count, chun_size);
        }

        void parallel_for(std::function<void(uint64_t, uint64_t)> func, uint64_t x, uint64_t y)
        {
			if (x == 0 || y == 0) return;

            thread_pool->parallel_for(func, x, y);
        }

        bool thread_finished(uint64_t index)
        {
            if (index == INVALID_SIZE_64) return false;
            return thread_pool->thread_finished(index);
        }

        bool thread_success(uint64_t index)
        {
			if (index == INVALID_SIZE_64) return false;
			return thread_pool->thread_success(index);
        }

        uint64_t begin_thread(std::function<bool()>&& rrFunc)
        {
            return thread_pool->submit(std::move(rrFunc));
        }

        bool run(TaskFlow& flow)
        {
            ReturnIfFalse(!flow.empty());

            uint32_t unfinished_task_count = flow.TotalTaskNum;

            const auto& src_nodes = flow.GetSrcNodes();
            for (const auto& node : src_nodes)
            {
                thread_pool->submit(
                    [node]() -> bool
                    {
                        ReturnIfFalse(node->run());
                        node_queue.push(node);     // 入队就说明已经完成了
                        return true;
                    }
                );
            }
            
            while (unfinished_task_count > 0)
            {
                node_queue.wait();
                unfinished_task_count--;

                TaskNode* node = node_queue.pop();
                for (const auto& successor : node->successors)
                {
                    successor->unfinished_dependent_task_count--;
                    if (successor->unfinished_dependent_task_count == 0)
                    {
                        thread_pool->submit(
                            [successor]() -> bool
                            {
                                ReturnIfFalse(successor->run());
                                node_queue.push(successor);
                                return true;
                            }
                        );
                        successor->unfinished_dependent_task_count = successor->unfinished_dependent_task_count_back_up;
                    }
                }
            }

            thread_pool->wait_for_idle();
            return true;
        }
    }

}