#ifndef TASK_FLOW_H
#define TASK_FLOW_H


#include <span>
#include <memory>
#include <vector>
#include <functional>
#include "../../Core/include/SysCall.h"

namespace FTS 
{
    struct FTaskNode
    {
        std::function<BOOL()> Func;
        std::vector<FTaskNode*> Successors;
        std::vector<FTaskNode*> Dependents;
        UINT32 UnfinishedDependentTaskNum = 0;
        UINT32 UnfinishedDependentTaskNumBackup = 0;


        FTaskNode(std::function<BOOL()> InFunc) : Func(std::move(InFunc)) {}

        void Precede(FTaskNode* InNode)
        {
            Successors.push_back(InNode);
            InNode->Dependents.push_back(this);
            InNode->UnfinishedDependentTaskNum++;
            InNode->UnfinishedDependentTaskNumBackup++;
        }

        BOOL Run() const { return Func(); }
    };

    class FTask
    {
    public:
        FTask() = default;
        FTask(FTaskNode* InNode) : Node(InNode) {}

    public:
        template <typename... Args>
        void Succeed(Args&&... Arguments)
        {
            static_assert((std::is_base_of<FTask, std::decay_t<Args>>::value && ...), "All Args must be Task or derived from Task.");
            (Arguments.Node->Precede(Node), ...);
        }

        template <typename... Args>
        void Precede(Args&&... Arguments)
        {
            static_assert((std::is_base_of<FTask, std::decay_t<Args>>::value && ...), "All Args must be Task or derived from Task.");
            (Node->Precede(Arguments.Node), ...);
        }

    private:
        FTaskNode* Node = nullptr;
    };

    // 最后能返回 BOOL
    class FTaskFlow
    {
        friend class StaticTaskExecutor;
    public:

        template <typename F, typename... Args>
        FTask Emplace(F&& InFunc, Args&&... Arguments)
        {
            static_assert(std::is_same<decltype(InFunc(Arguments...)), BOOL>::value, "Thread work must return bool.");
            TotalTaskNum++;
            
            auto Func = std::bind(std::forward<F>(InFunc), std::forward<Args>(Arguments)...);
            Nodes.emplace_back(std::make_unique<FTaskNode>([Func]() -> BOOL { return Func(); }));
            return Nodes.back().get();
        }

        template <typename T, typename... Args>
        FTask Emplace(T* Instance, void(T::*MemberFunc)(Args...), Args... Arguments)
        {
            static_assert(std::is_same<decltype(InFunc(Arguments...)), BOOL>::value, "Thread work must return bool.");
            TotalTaskNum++;
            
            auto Func = [Instance, MemberFunc](Args... FuncArgs) { (Instance->*MemberFunc)(std::forward<Args>(FuncArgs)...); };
            
            Nodes.emplace_back(std::make_unique<FTaskNode>([=]() -> BOOL { return Func(Arguments...); }));
            return Nodes.back().get();
        }

        void Reset()
        {
            SrcNodes.clear();
            Nodes.clear();
            TotalTaskNum = 0;
        }

        std::span<FTaskNode*> GetSrcNodes()
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
        
        bool Empty() const
        {
            return TotalTaskNum == 0;
        }

        UINT32 TotalTaskNum = 0;

    private:
        std::vector<FTaskNode*> SrcNodes;
        std::vector<std::unique_ptr<FTaskNode>> Nodes;
    };

    namespace TaskFlow
    {
        void Initialize();
        void Destroy();
        
        BOOL Run(FTaskFlow& InFlow);

        template <typename... Args>
        BOOL Run(Args&&... Arguments)
        {
            static_assert((std::is_base_of<FTaskFlow, std::decay_t<Args>>::value && ...), "All Args must be Task or derived from Task.");
            return (Run(Arguments), ...);
        }

        UINT64 BeginThreadImpl(std::function<BOOL()>&& rrFunc);

        template <typename T, typename... Args>
        UINT64 BeginThread(T* Instance, BOOL(T::*MemberFunc)(Args...), Args... Arguments)
        {
            auto Func = [Instance, MemberFunc](Args... FuncArgs) -> BOOL { return (Instance->*MemberFunc)(std::forward<Args>(FuncArgs)...); };
            return BeginThreadImpl( [=]() -> BOOL { return Func(Arguments...); } );
        }

        template <typename F, typename... Args>
        UINT64 BeginThread(F&& InFunc, Args&&... Arguments)
        {
            static_assert(std::is_same<decltype(InFunc(Arguments...)), BOOL>::value, "Thread work must return bool.");

            auto Func = std::bind(std::forward<F>(InFunc), std::forward<Args>(Arguments)...);
            return BeginThreadImpl( [=]() -> BOOL { return Func(Arguments...); } );
        }

        void ParallelFor(std::function<void(UINT64)> Func, UINT64 stCount, UINT32 dwChunkSize = 1);
        void ParallelFor(std::function<void(UINT64, UINT64)> Func, UINT64 stX, UINT64 stY);
        BOOL ThreadFinished(UINT64 stIndex);
        BOOL ThreadSuccess(UINT64 stIndex);
    };
}

#endif