#ifndef CORE_DELEGATE_H
#define CORE_DELEGATE_H
#include <functional>
#include <cassert>
#include "log.h"

namespace fantasy 
{
    #define DELCARE_DELEGATE_EVENT(ClassName, ...)      \
        struct ClassName : public Delegate<__VA_ARGS__> \
        {                                               \
            using Delegate::add_event;                  \
            using Delegate::broadcast;                 \
        }


    #define DELCARE_MULTI_DELEGATE_EVENT(ClassName, ...)        \
        struct ClassName : public MultiDelegate<__VA_ARGS__>    \
        {                                                       \
            using MultiDelegate::add_event;                     \
            using MultiDelegate::broadcast;                    \
        }


    template<typename T, typename... Args>
    size_t get_func_address(std::function<T(Args...)> func) 
    {
        typedef T(FuncType)(Args...);
        FuncType** func_pointer = func.template target<FuncType*>();
        return size_t(*func_pointer);
    }


    struct DelegateInterface
    {
		virtual ~DelegateInterface() = default;
    };

    template <typename... Args>
    class Delegate : public DelegateInterface
    {
    public:
        void add_event(std::function<bool(Args...)> func)
        {
            _delegate_func = std::move(func);
        }

        void remove_event()
        {
            _delegate_func = nullptr;
        }

        bool broadcast(Args... arguments) const
        {
            if (_delegate_func)
            {
                return _delegate_func(std::forward<Args>(arguments)...);
            }
            return false;
        }

    private:
        std::function<bool(Args...)> _delegate_func;
    };

    template <typename... Args>
    class MultiDelegate : public DelegateInterface
    {
    public:
        void add_event(std::function<bool(Args...)> func)
        {
            _delegate_array.emplace_back(std::move(func));
        }

        void remove_event(std::function<bool(Args...)> func)
        {
            _delegate_array.erase(
                std::remove_if(
                    _delegate_array.begin(),
                    _delegate_array.end(),
                    [&](const auto& delegate_func)
                    {
                        return get_func_address(delegate_func) == get_func_address(func);
                    }
                ),
                _delegate_array.end()
            );
        }

        template <typename F>
        void remove_event(F* instance, bool(F::*member_func)(Args...))
        {
            std::function<bool(Args...)> func = std::bind(member_func, instance);
            _delegate_array.erase(
                std::remove_if(
                    _delegate_array.begin(),
                    _delegate_array.end(),
                    [&](const auto& delegate_func)
                    {
                        return get_func_address(delegate_func) == get_func_address(func);
                    }
                ),
                _delegate_array.end()
            );
        }

        bool broadcast(Args... arguments) const
        {
            for (const auto& delegate_func : _delegate_array)
            {
                ReturnIfFalse(delegate_func(std::forward<Args>(arguments)...));
            }
            return true;
        }

    private:
        std::vector<std::function<bool(Args...)>> _delegate_array;
    };
}

#endif