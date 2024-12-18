#ifndef TASK_FLOW_CONCURRENT_QUEUE_H
#define TASK_FLOW_CONCURRENT_QUEUE_H

#include <memory>
#include <mutex>

namespace fantasy 
{
    template <typename T>
    class ConcurrentQueue
    {
        struct Node
        {
            std::shared_ptr<T> value;
            std::unique_ptr<Node> next;
        };
    public:
        ConcurrentQueue() : _head(std::make_unique<Node>()), _tail(_head.get()) {}     // 开头的DummyHead可以使Push()只访问尾节点

    public:
        bool empty() const
        {
            std::lock_guard lock_guard(_head_mutex);
            return _head.get() == get_tail();
        }

        void push(T val)
        {
            std::shared_ptr<T> value = std::make_shared<T>(std::forward<T>(val));
            std::unique_ptr<Node> new_node = std::make_unique<Node>();
            Node* new_tail = new_node.get();
            {
                std::lock_guard lock_guard(_tail_mutex);
                _tail->value = value;
                _tail->next = std::move(new_node);
                _tail = new_tail;
            }
            condition_variable.notify_one();
        }

        bool try_pop(T& out_val)
        {
            return try_pop_impl(out_val) != nullptr;
        }

        std::shared_ptr<T> try_pop()
        {
            std::unique_ptr<Node> popped_head = try_pop_impl();
            return popped_head ? popped_head->value : nullptr;
        }

    private:
        Node* get_tail() const
        {
            std::lock_guard lock_guard(_tail_mutex);
            return _tail;
        }

        std::unique_ptr<Node> pop()
        {
            std::unique_ptr<Node> ret = std::move(_head);
            _head = std::move(ret->next);
            return ret;
        }


    private:
        std::unique_ptr<Node> try_pop_impl(T& out_val)
        {
            std::lock_guard lock_guard(_head_mutex);

            if (_head.get() == get_tail())
            {
                return nullptr;
            }

            out_val = std::move(*_head->value);

            return pop();
        }


        std::unique_ptr<Node> try_pop_impl()
        {
            std::lock_guard lock_guard(_head_mutex);

            if (_head.get() == get_tail())
            {
                return nullptr;
            }

            return pop();
        }


    private:
        std::unique_ptr<Node> _head;
        Node* _tail;

        mutable std::mutex _head_mutex;
        mutable std::mutex _tail_mutex;

    public:
        std::condition_variable condition_variable;
    };

}


#endif