#ifndef TASK_FLOW_CONCURRENT_QUEUE_H
#define TASK_FLOW_CONCURRENT_QUEUE_H

#include <memory>
#include <mutex>
#include "../../Core/include/SysCall.h"

namespace FTS 
{
    template <typename T>
    class TConcurrentQueue
    {
        struct Node
        {
            std::shared_ptr<T> Value;
            std::unique_ptr<Node> Next;
        };
    public:
        TConcurrentQueue() : m_pHead(std::make_unique<Node>()), m_pTail(m_pHead.get()) {}     // 开头的DummyHead可以使Push()只访问尾节点

    public:
        BOOL Empty() const
        {
            std::lock_guard LockGuard(m_HeadMutex);
            return m_pHead.get() == GetTail();
        }

        void Push(T InValue)
        {
            std::shared_ptr<T> Value = std::make_shared<T>(std::forward<T>(InValue));
            std::unique_ptr<Node> NewDummyNode = std::make_unique<Node>();
            Node* NewTail = NewDummyNode.get();
            {
                std::lock_guard LockGuard(m_TailMutex);
                m_pTail->Value = Value;
                m_pTail->Next = std::move(NewDummyNode);
                m_pTail = NewTail;
            }
            ConditionVariable.notify_one();
        }

        BOOL TryPop(T& OutValue)
        {
            return TryPopImpl(OutValue) != nullptr;
        }

        std::shared_ptr<T> TryPop()
        {
            std::unique_ptr<Node> PoppedHead = TryPopImpl();
            return PoppedHead ? PoppedHead->Value : nullptr;
        }

    private:
        Node* GetTail() const
        {
            std::lock_guard LockGuard(m_TailMutex);
            return m_pTail;
        }

        std::unique_ptr<Node> Pop()
        {
            std::unique_ptr<Node> Output = std::move(m_pHead);
            m_pHead = std::move(Output->Next);
            return Output;
        }


    private:
        std::unique_ptr<Node> TryPopImpl(T& OutValue)
        {
            std::lock_guard LockGuard(m_HeadMutex);

            if (m_pHead.get() == GetTail())
            {
                return nullptr;
            }

            OutValue = std::move(*m_pHead->Value);

            return Pop();
        }


        std::unique_ptr<Node> TryPopImpl()
        {
            std::lock_guard LockGuard(m_HeadMutex);

            if (m_pHead.get() == GetTail())
            {
                return nullptr;
            }

            return Pop();
        }


    private:
        std::unique_ptr<Node> m_pHead;
        Node* m_pTail;

        mutable std::mutex m_HeadMutex;
        mutable std::mutex m_TailMutex;

    public:
        std::condition_variable ConditionVariable;
    };

}


#endif