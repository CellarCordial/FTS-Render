#ifndef COMMON_ENTITIY_H
#define COMMON_ENTITIY_H
#include "SysCall.h"
#include "ComRoot.h"
#include "../../Math/include/Common.h"

#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace FTS
{
    class FWorld;
	class FEntity;

    struct IEntitySystem
	{
		virtual ~IEntitySystem() = default;

		virtual BOOL Initialize(FWorld* pWorld) = 0;
		virtual BOOL Destroy() = 0;
		virtual BOOL Tick(FWorld* world, FLOAT fDelta) = 0;
	};

	struct IEventSubscriber
	{
		virtual ~IEventSubscriber() = default;
	};

	template <typename T>
	struct TEventSubscriber : public IEventSubscriber
	{
		virtual ~TEventSubscriber() = default;

		virtual BOOL Publish(FWorld* pWorld, const T& crEvent) = 0;
	};


	namespace Event
	{
		template <typename T>
		struct OnComponentAssigned
		{
			FEntity* pEntity;
			T* pComponent;
		};
		
		template <typename T>
		struct OnComponentRemoved
		{
			FEntity* pEntity;
			T* pComponent;
		};
	}

	struct IComponentContainer
	{
		virtual BOOL Removed(FEntity* pEntity) = 0;

		virtual ~IComponentContainer() = default;
	};


	template <typename... ComponentTypes>
	struct TEntityIterator
	{
		FWorld* pWorld;
		UINT64 stEntityIndex;
		BOOL bIsLastEntity;
		BOOL bIncludePendingDestroy;

		TEntityIterator(FWorld* pWorld, UINT64 stComponentIndex, BOOL bIsLastComponent, BOOL bIncludePendingDestroy);

		BOOL IsEnd() const;
		FEntity* GetEntity() const;
		TEntityIterator& operator++();

		FEntity* operator*() const { return GetEntity(); }

		BOOL operator==(const TEntityIterator<ComponentTypes...>& crIterator) const
		{
			ReturnIfFalse(pWorld == crIterator.pWorld);
			if (bIsLastEntity) return crIterator.IsEnd();
			return stEntityIndex == crIterator.stEntityIndex;
		}

		BOOL operator!=(const TEntityIterator<ComponentTypes...>& crIterator) const
		{
			return !((*this) == crIterator);
		}

	};

	template <>
	struct TEntityIterator<>
	{
		FWorld* pWorld;
		UINT64 stEntityIndex;
		BOOL bIsLastEntity;
		BOOL bIncludePendingDestroy;

		TEntityIterator(FWorld* pWorld, UINT64 stComponentIndex, BOOL bIsLastComponent, BOOL bIncludePendingDestroy);

		BOOL IsEnd() const;
		FEntity* GetEntity() const;
		TEntityIterator& operator++();

		FEntity* operator*() const { return GetEntity(); }

		BOOL operator==(const TEntityIterator<>& crIterator) const
		{
			ReturnIfFalse(pWorld == crIterator.pWorld);
			if (IsEnd()) return crIterator.IsEnd();
			return stEntityIndex == crIterator.stEntityIndex;
		}		

		BOOL operator!=(const TEntityIterator<>& crIterator) const
		{
			return !((*this) == crIterator);
		}

	};

	template <typename... ComponentTypes>
	struct TEntityView
	{
		TEntityIterator<ComponentTypes...> Begin;
		TEntityIterator<ComponentTypes...> End;		// 在最后一个元素的后一位.

		TEntityView(const TEntityIterator<ComponentTypes...>& crBegin, const TEntityIterator<ComponentTypes...>& crEnd);

		TEntityIterator<ComponentTypes...> begin() 	{ return Begin; }
		TEntityIterator<ComponentTypes...> end() 	{ return End;   }
	};

	template <>
	struct TEntityView<>
	{
		TEntityIterator<> Begin;
		TEntityIterator<> End;		// 在最后一个元素的后一位.

		TEntityView(const TEntityIterator<>& crBegin, const TEntityIterator<>& crEnd);

		TEntityIterator<> begin() 	{ return Begin; }
		TEntityIterator<> end() 	{ return End;   }
	};

	class FEntity
	{
	public:
		FEntity(FWorld* pWorld, UINT64 stID);
		~FEntity();

		FWorld* GetWorld() const;
		UINT64 GetID() const;
		BOOL IsPendingDestroy() const;
		void RemoveAll();


		template <typename T>
		T* GetComponent() const;

		template <typename T, typename... Args>
		requires std::is_constructible_v<T, Args...>
		T* Assign(Args&&... rrArgs);

		template <typename T>
		BOOL Contain() const
		{
			auto TypeIndex = std::type_index(typeid(T));
			return m_Components.find(TypeIndex) != m_Components.end();
		}

		template <typename T, typename U, typename... Types>
		BOOL Contain() const
		{
			return Contain<T>() && Contain<U, Types...>();
		}

		template <typename... Types>
		BOOL With(typename std::common_type<std::function<void(Types*...)>>::type FuncView)
		{
			if (!Contain<Types...>()) return false;
			FuncView(Get<Types>()...);
			return true;
		}

		template <typename T>
		BOOL Remove()
		{
			auto Iter = m_Components.find(std::type_index(typeid(T)));
			if (Iter != m_Components.end())
			{
				Iter->second->Removed(this);

				{
					std::lock_guard Lock(m_ComponentMutex);
					m_Components.erase(Iter);
				}

				return true;
			}
			return false;
		}

	private:
		friend class FWorld;

		std::mutex m_ComponentMutex;
		std::unordered_map<std::type_index, std::unique_ptr<IComponentContainer>> m_Components;
		FWorld* m_pWorld;

		UINT64 m_stIndex = INVALID_SIZE_64;	// Index in world.
		BOOL m_bPendingDestroy = false;		// 设定为 true, 意味着已经(需要) Boardcast 一次 Event::FOnAnyEntityDestroyed
	};



	class FWorld
	{
	public:
		FWorld() { CreateEntity(); }
		~FWorld();

		FEntity* CreateEntity();
		BOOL DestroyEntity(FEntity* pEntity, BOOL bImmediately = false);

		FEntity* GetGlobalEntity() { return m_pEntities[0].get(); }

		BOOL Tick(FLOAT fDelta);
		
		void CleanUp();
		BOOL Reset();

		// World 会直接管理内存, 请直接使用 new.
		IEntitySystem* RegisterSystem(IEntitySystem* pSystem);
		BOOL UnregisterSystem(IEntitySystem* pSystem);

		void DisableSystem(IEntitySystem* pSystem);
		void EnableSystem(IEntitySystem* pSystem);

		template <typename T>
		void Subscribe(TEventSubscriber<T>* pSubscriber)
		{
			assert(pSubscriber != nullptr);

			auto TypeIndex = std::type_index(typeid(T));
			auto Iter = m_pSubscribers.find(TypeIndex);
			if (Iter == m_pSubscribers.end())
			{
				std::vector<IEventSubscriber*> vec;
				vec.emplace_back(pSubscriber);

				m_pSubscribers[TypeIndex] = std::move(vec);
			}
			else 
			{
				Iter->second.emplace_back(pSubscriber);
			}
		}

		template <typename T>
		void Unsubscribe(TEventSubscriber<T>* pSubscriber)
		{
			auto TypeIndex = std::type_index(typeid(T));
			auto Iter = m_pSubscribers.find(TypeIndex);
			if (Iter != m_pSubscribers.end())
			{
				Iter->second.erase(std::remove(Iter->second.begin(), Iter->second.end(), pSubscriber), Iter->second.end());
				if (Iter->second.size() == 0)
				{
					m_pSubscribers.erase(Iter);
				}
			}
		}

		void UnsubscribeAll(void* pSystem)
		{
			for (auto& rpSubscribers : m_pSubscribers)
			{
				rpSubscribers.second.erase(std::remove(rpSubscribers.second.begin(), rpSubscribers.second.end(), pSystem), rpSubscribers.second.end());
				
				if (rpSubscribers.second.empty())
				{
					m_pSubscribers.erase(rpSubscribers.first);
				}
			}

		}


		template <typename T>
		BOOL Boardcast(const T& crEvent)
		{
			auto Iter = m_pSubscribers.find(std::type_index(typeid(T)));
			if (Iter != m_pSubscribers.end())
			{
				for (const auto& crpSubscriber : Iter->second)
				{
					ReturnIfFalse(static_cast<TEventSubscriber<T>*>(crpSubscriber)->Publish(this, crEvent));
				}
			}
			return true;
		}

		template <typename... ComponentTypes>
		TEntityView<ComponentTypes...> GetEntityView(BOOL bIncludePendingDestroy = false)
		{
			return TEntityView<ComponentTypes...>(
				TEntityIterator<ComponentTypes...>(this, 0, false, bIncludePendingDestroy), 
				TEntityIterator<ComponentTypes...>(this, GetEntityNum(), true, bIncludePendingDestroy)
			);
		}

		template <typename... ComponentTypes>
		BOOL Each(typename std::common_type<std::function<BOOL(FEntity*, ComponentTypes*...)>>::type Func, BOOL bIncludePendingDestroy = false)
		{
			auto View = GetEntityView<ComponentTypes...>();
			for (auto* pEntity : View)
			{
				ReturnIfFalse(Func(pEntity, pEntity->template GetComponent<ComponentTypes>()...));
			}
			return true;
		}

		TEntityView<> GetEntityView(BOOL bIncludePendingDestroy = false)
		{
			return TEntityView<>(
				TEntityIterator<>(this, 0, false, bIncludePendingDestroy), 
				TEntityIterator<>(this, GetEntityNum(), true, bIncludePendingDestroy)
			);
		}

		BOOL All(std::function<BOOL(FEntity*)> Func, BOOL bIncludePendingDestroy = false)
		{
			auto View = GetEntityView();
			for (auto* pEntity : View)
			{
				ReturnIfFalse(Func(pEntity));
			}
			return true;
		}

		UINT64 GetEntityNum() const { return m_pEntities.size(); }
		FEntity* GetEntity(UINT64 stIndex) const { return m_pEntities[stIndex].get(); }

	private:
		std::vector<std::unique_ptr<FEntity>> m_pEntities;
		std::vector<std::unique_ptr<IEntitySystem>> m_pSystems;
		std::vector<std::unique_ptr<IEntitySystem>> m_pDisabledSystems;
		std::unordered_map<std::type_index, std::vector<IEventSubscriber*>> m_pSubscribers;
	};


	template <typename... ComponentTypes>
	TEntityIterator<ComponentTypes...>::TEntityIterator(FWorld* pWorld, UINT64 stComponentIndex, BOOL bIsLastComponent, BOOL bIncludePendingDestroy) :
		pWorld(pWorld), 
		stEntityIndex(stComponentIndex), 
		bIsLastEntity(bIsLastComponent), 
		bIncludePendingDestroy(bIsLastComponent)
	{
		if (stEntityIndex == pWorld->GetEntityNum() - 1) bIsLastEntity = true;
	}

	template <typename... ComponentTypes>
	BOOL TEntityIterator<ComponentTypes...>::IsEnd() const
	{
		return stEntityIndex >= pWorld->GetEntityNum();
	}

	template <typename... ComponentTypes>
	FEntity* TEntityIterator<ComponentTypes...>::GetEntity() const
	{
		if (IsEnd()) return nullptr;
		return pWorld->GetEntity(stEntityIndex);
	}

	template <typename... ComponentTypes>
	TEntityIterator<ComponentTypes...>& TEntityIterator<ComponentTypes...>::operator++()
	{
		stEntityIndex++;
		while (
			stEntityIndex < pWorld->GetEntityNum() &&
			(
				GetEntity() == nullptr ||
				!GetEntity()->template Contain<ComponentTypes...>() ||
				(GetEntity()->IsPendingDestroy() && !bIncludePendingDestroy)
			)
		)
		{
			stEntityIndex++;
		}

		if (stEntityIndex >= pWorld->GetEntityNum()) bIsLastEntity = true;
		return *this;
	}

	template <typename... ComponentTypes>
	TEntityView<ComponentTypes...>::TEntityView(const TEntityIterator<ComponentTypes...>& crBegin, const TEntityIterator<ComponentTypes...>& crEnd) :
		Begin(crBegin), End(crEnd)
	{
		if (
			Begin.GetEntity() == nullptr ||
			!(*Begin)->template Contain<ComponentTypes...>() ||
			(Begin.GetEntity()->IsPendingDestroy() && !Begin.bIncludePendingDestroy)
		)
		{
			++Begin;
		}
	}

	template <typename T>
	struct TComponentContainer : public IComponentContainer
	{
		TComponentContainer() = default;
		TComponentContainer(const T& crData) : Data(crData) 
		{
		}

		BOOL Removed(FEntity* pEntity) override
		{
			return pEntity->GetWorld()->Boardcast<Event::OnComponentRemoved<T>>(Event::OnComponentRemoved<T>{ pEntity, &Data });
		}

		T Data;
	};

	template <typename T>
	T* FEntity::GetComponent() const
	{
		auto Iter = m_Components.find(std::type_index(typeid(T)));
		if (Iter != m_Components.end())
		{
			return &(reinterpret_cast<TComponentContainer<T>*>(Iter->second.get())->Data);
		}
		return nullptr;
	}

	template <typename T, typename... Args>
	requires std::is_constructible_v<T, Args...>
	T* FEntity::Assign(Args&&... rrArgs)
	{
		auto TypeIndex = std::type_index(typeid(T));
		auto Iter = m_Components.find(TypeIndex);
		if (Iter != m_Components.end())
		{
			auto* pContainer = reinterpret_cast<TComponentContainer<T>*>(Iter->second.get());
			pContainer->Data = T(rrArgs...);
			if (!m_pWorld->Boardcast<Event::OnComponentAssigned<T>>(Event::OnComponentAssigned<T>{ this, &pContainer->Data })) return nullptr;
			return &pContainer->Data;
		}
		else
		{
			std::unique_ptr<TComponentContainer<T>> pContainer = std::make_unique<TComponentContainer<T>>(T(rrArgs...));

			T* pRet = &pContainer->Data;
			if (!m_pWorld->Boardcast<Event::OnComponentAssigned<T>>(Event::OnComponentAssigned<T>{ this, pRet })) return nullptr;

			{
				std::lock_guard Lock(m_ComponentMutex);
				m_Components[TypeIndex] = std::move(pContainer);
			}

			return pRet;
		}
	}

	

}









#endif