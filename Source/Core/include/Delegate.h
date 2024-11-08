#ifndef CORE_DELEGATE_H
#define CORE_DELEGATE_H
#include <basetsd.h>
#include <functional>
#include <cassert>
#include "ComRoot.h"
#include "SysCall.h"

namespace FTS 
{
    #define DeclareDelegateEvent(ClassName, ...)                \
        class ClassName : public TDelegate<__VA_ARGS__>         \
        {                                                       \
        public:                                                 \
            using TDelegate::AddEvent;                          \
            using TDelegate::Broadcast;                         \
        }


    #define DeclareMultiDelegateEvent(ClassName, ...)           \
        class ClassName : public TMultiDelegate<__VA_ARGS__>    \
        {                                                       \
        public:                                                 \
            using TMultiDelegate::AddEvent;                     \
            using TMultiDelegate::Broadcast;                    \
        }


    template<typename T, typename... Args>
    SIZE_T GetFuncAddress(std::function<T(Args...)> Func) 
    {
        typedef T(FuncType)(Args...);
        FuncType** fnPointer = Func.template target<FuncType*>();
        return SIZE_T(*fnPointer);
    }


    struct IDelegate
    {
		virtual ~IDelegate() = default;
    };

    template <typename... Args>
    class TDelegate : public IDelegate
    {
    public:
        void AddEvent(std::function<BOOL(Args...)> Func)
        {
            m_DelegateFunc = std::move(Func);
        }

        template <typename F>
        void AddEvent(F* Instance, BOOL(F::*MemberFunc)(Args...))
        {
            assert(Instance != nullptr && "Try to use nullptr member function.");

            m_DelegateFunc = [Instance, MemberFunc](Args&&... InArguments) -> BOOL
            {
                return (Instance->*MemberFunc)(std::forward<Args>(InArguments)...);
            };
        }

        void RemoveEvent()
        {
            m_DelegateFunc = nullptr;
        }

        BOOL Broadcast(Args... InArguments) const
        {
            if (m_DelegateFunc)
            {
                return m_DelegateFunc(std::forward<Args>(InArguments)...);
            }
            return false;
        }

    private:
        std::function<BOOL(Args...)> m_DelegateFunc;
    };

    template <typename... Args>
    class TMultiDelegate : public IDelegate
    {
    public:
        void AddEvent(std::function<BOOL(Args...)> Func)
        {
            m_DelegateArray.emplace_back(std::move(Func));
        }

        template <typename F>
        void AddEvent(F* Instance, BOOL(F::*MemberFunc)(Args...))
        {
            assert(Instance != nullptr && "Try to use nullptr member function.");

            m_DelegateArray.emplace_back(
                [Instance, MemberFunc](Args&&... InArguments) -> BOOL
                {
                    return (Instance->*MemberFunc)(std::forward<Args>(InArguments)...);
                }
            );
        }

        void RemoveEvent(std::function<BOOL(Args...)> Func)
        {
            m_DelegateArray.erase(
                std::remove_if(
                    m_DelegateArray.begin(),
                    m_DelegateArray.end(),
                    [&](const auto& crFunc)
                    {
                        return GetFuncAddress(crFunc) == GetFuncAddress(Func);
                    }
                ),
                m_DelegateArray.end()
            );
        }

        template <typename F>
        void RemoveEvent(F* Instance, BOOL(F::*MemberFunc)(Args...))
        {
            std::function<BOOL(Args...)> Func = std::bind(MemberFunc, Instance);
            m_DelegateArray.erase(
                std::remove_if(
                    m_DelegateArray.begin(),
                    m_DelegateArray.end(),
                    [&](const auto& crFunc)
                    {
                        return GetFuncAddress(crFunc) == GetFuncAddress(Func);
                    }
                ),
                m_DelegateArray.end()
            );
        }

        BOOL Broadcast(Args... InArguments) const
        {
            for (const auto& DelegateFunction : m_DelegateArray)
            {
                ReturnIfFalse(DelegateFunction(std::forward<Args>(InArguments)...));
            }
            return true;
        }

    private:
        std::vector<std::function<BOOL(Args...)>> m_DelegateArray;
    };
}

#endif