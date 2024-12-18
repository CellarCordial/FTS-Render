#ifndef TASK_FLOW_H
#define TASK_FLOW_H


#include <memory>
#include <vector>
#include <functional>

namespace fantasy 
{
    struct TaskNode
    {
        std::function<bool()> func;
        std::vector<TaskNode*> successors;
        std::vector<TaskNode*> Dependents;
        uint32_t unfinished_dependent_task_count = 0;
        uint32_t unfinished_dependent_task_count_back_up = 0;


        TaskNode(std::function<bool()> InFunc) : func(std::move(InFunc)) {}

        void precede(TaskNode* InNode)
        {
            successors.push_back(InNode);
            InNode->Dependents.push_back(this);
            InNode->unfinished_dependent_task_count++;
            InNode->unfinished_dependent_task_count_back_up++;
        }

        bool run() const { return func(); }
    };

    class Task
    {
    public:
        Task() = default;
        Task(TaskNode* InNode) : Node(InNode) {}

    public:
        template <typename... Args>
        void succeed(Args&&... Arguments)
        {
            static_assert((std::is_base_of<Task, std::decay_t<Args>>::value && ...), "All Args must be Task or derived from Task.");
            (Arguments.Node->precede(Node), ...);
        }

        template <typename... Args>
        void precede(Args&&... Arguments)
        {
            static_assert((std::is_base_of<Task, std::decay_t<Args>>::value && ...), "All Args must be Task or derived from Task.");
            (Node->precede(Arguments.Node), ...);
        }

    private:
        TaskNode* Node = nullptr;
    };

    // 最后能返回 bool
    class TaskFlow
    {
        friend class StaticTaskExecutor;
    public:

        template <typename F, typename... Args>
        Task Emplace(F&& InFunc, Args&&... Arguments)
        {
            static_assert(std::is_same<decltype(InFunc(Arguments...)), bool>::value, "Thread work must return bool.");
            TotalTaskNum++;
            
            auto func = std::bind(std::forward<F>(InFunc), std::forward<Args>(Arguments)...);
            Nodes.emplace_back(std::make_unique<TaskNode>([func]() -> bool { return func(); }));
            return Nodes.back().get();
		}

		template <typename T, typename... Args>
		Task Emplace(T* Instance, void(T::* MemberFunc)(Args...), Args... Arguments)
		{
			static_assert(std::is_same<decltype(InFunc(Arguments...)), bool>::value, "Thread work must return bool.");
			TotalTaskNum++;

			auto func = [Instance, MemberFunc](Args... FuncArgs) { (Instance->*MemberFunc)(std::forward<Args>(FuncArgs)...); };

			Nodes.emplace_back(std::make_unique<TaskNode>([=]() -> bool { return func(Arguments...); }));
			return Nodes.back().get();
		}

        void reset()
        {
            SrcNodes.clear();
            Nodes.clear();
            TotalTaskNum = 0;
        }

        const std::vector<TaskNode*>& GetSrcNodes()
        {
            SrcNodes.clear();
            for (const auto& Node : Nodes)
            {
                if (Node->Dependents.empty())
                {
                    SrcNodes.push_back(Node.get());
                }
            }
            return SrcNodes;
        }
        
        bool empty() const
        {
            return TotalTaskNum == 0;
        }

        uint32_t TotalTaskNum = 0;

    private:
        std::vector<TaskNode*> SrcNodes;
        std::vector<std::unique_ptr<TaskNode>> Nodes;
    };

    namespace parallel
    {
        void initialize();
        void destroy();
        
        bool run(TaskFlow& InFlow);

        template <typename... Args>
        bool run(Args&&... Arguments)
        {
            static_assert((std::is_base_of<TaskFlow, std::decay_t<Args>>::value && ...), "All Args must be Task or derived from Task.");
            return (run(Arguments), ...);
        }

        uint64_t begin_thread(std::function<bool()>&& rrFunc);
        void parallel_for(std::function<void(uint64_t)> func, uint64_t count, uint32_t chun_size = 1);
        void parallel_for(std::function<void(uint64_t, uint64_t)> func, uint64_t x, uint64_t y);
        bool thread_finished(uint64_t index);
        bool thread_success(uint64_t index);
    };
}

#endif