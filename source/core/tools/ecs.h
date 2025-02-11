#ifndef CORE_ECS_H
#define CORE_ECS_H
#include "../math/common.h"
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include "log.h"

namespace fantasy
{
    class World;
	class Entity;

    struct EntitySystemInterface
	{
		virtual ~EntitySystemInterface() = default;

		virtual bool initialize(World* world) = 0;
		virtual bool destroy() = 0;
		virtual bool tick(float time_delta) = 0;
	};

	struct IEventSubscriber
	{
		virtual ~IEventSubscriber() = default;
	};

	template <typename T>
	struct EventSubscriber : public IEventSubscriber
	{
		virtual ~EventSubscriber() = default;

		virtual bool publish(World* world, const T& event) = 0;
	};


	namespace event
	{
		template <typename T>
		struct OnComponentAssigned
		{
			Entity* entity;
			T* component;
		};
		
		template <typename T>
		struct OnComponentRemoved
		{
			Entity* entity;
			T* component;
		};
	}

	struct ComponentContainerInterface
	{
		virtual bool removed(Entity* entity) = 0;

		virtual ~ComponentContainerInterface() = default;
	};


	template <typename... ComponentTypes>
	struct EntityIterator
	{
		World* _world;
		uint64_t _entity_index;
		bool _is_last_entity;
		bool _include_pending_destroy;

		EntityIterator(World* world, uint64_t component_index, bool is_last_component, bool include_pending_destroy);

		bool is_end() const;
		Entity* get_entity() const;
		EntityIterator& operator++();

		Entity* operator*() const { return get_entity(); }

		bool operator==(const EntityIterator<ComponentTypes...>& iterator) const
		{
			ReturnIfFalse(_world == iterator._world);
			if (_is_last_entity) return iterator.is_end();
			return _entity_index == iterator._entity_index;
		}

		bool operator!=(const EntityIterator<ComponentTypes...>& iterator) const
		{
			return !((*this) == iterator);
		}

	};

	template <>
	struct EntityIterator<>
	{
		World* _world;
		uint64_t _entity_index;
		bool _is_last_entity;
		bool _include_pending_destroy;

		EntityIterator(World* world, uint64_t component_index, bool is_last_component, bool include_pending_destroy);

		bool is_end() const;
		Entity* get_entity() const;
		EntityIterator& operator++();

		Entity* operator*() const { return get_entity(); }

		bool operator==(const EntityIterator<>& iterator) const
		{
			ReturnIfFalse(_world == iterator._world);
			if (is_end()) return iterator.is_end();
			return _entity_index == iterator._entity_index;
		}		

		bool operator!=(const EntityIterator<>& iterator) const
		{
			return !((*this) == iterator);
		}

	};

	template <typename... ComponentTypes>
	struct EntityView
	{
		EntityIterator<ComponentTypes...> _begin;
		EntityIterator<ComponentTypes...> _end;		// 在最后一个元素的后一位.

		EntityView(const EntityIterator<ComponentTypes...>& begin, const EntityIterator<ComponentTypes...>& end);

		EntityIterator<ComponentTypes...> begin() 	{ return _begin; }
		EntityIterator<ComponentTypes...> end() 	{ return _end;   }
	};

	template <>
	struct EntityView<>
	{
		EntityIterator<> _begin;
		EntityIterator<> _end;		// 在最后一个元素的后一位.

		EntityView(const EntityIterator<>& begin, const EntityIterator<>& end);

		EntityIterator<> begin() 	{ return _begin; }
		EntityIterator<> end() 	{ return _end;   }
	};

	class Entity
	{
	public:
		Entity(World* world, uint64_t id);
		~Entity();

		World* get_world() const;
		uint64_t get_id() const;
		bool is_pending_destroy() const;
		void remove_all();


		template <typename T>
		T* get_component() const;

		template <typename T, typename... Args>
		requires std::is_constructible_v<T, Args...>
		T* assign(Args&&... arguments);

		template <typename T>
		bool contain() const
		{
			auto type_index = std::type_index(typeid(T));
			return _components.find(type_index) != _components.end();
		}

		template <typename T, typename U, typename... Types>
		bool contain() const
		{
			return contain<T>() && contain<U, Types...>();
		}

		template <typename... Types>
		bool with(typename std::common_type<std::function<void(Types*...)>>::type func_view)
		{
			if (!contain<Types...>()) return false;
			func_view(get_component<Types>()...);
			return true;
		}

		template <typename T>
		bool remove()
		{
			auto iter = _components.find(std::type_index(typeid(T)));
			if (iter != _components.end())
			{
				iter->second->removed(this);

				{
					std::lock_guard lock(_component_mutex);
					_components.erase(iter);
				}

				return true;
			}
			return false;
		}

	private:
		friend class World;

		std::mutex _component_mutex;
		std::unordered_map<std::type_index, std::unique_ptr<ComponentContainerInterface>> _components;
		World* _world;

		uint64_t _index = INVALID_SIZE_64;	// Index in world.
		bool _is_pending_destroy = false;	// 设定为 true, 意味着已经(需要) broadcast 一次 event::OnAnyEntityDestroyed
	};



	class World
	{
	public:
		World() { create_entity(); }
		~World();

		Entity* create_entity();
		bool destroy_entity(Entity* entity, bool bImmediately = false);

		Entity* create_entity_delay();
		void add_delay_entity(Entity* entity);

		Entity* get_global_entity() { return _entities[0].get(); }

		bool tick(float delta);
		
		void cleanup();
		bool reset();

		// World 会直接管理内存, 请直接使用 new.
		EntitySystemInterface* register_system(EntitySystemInterface* system);
		bool unregister_system(EntitySystemInterface* system);

		void disable_system(EntitySystemInterface* system);
		void enable_system(EntitySystemInterface* system);

		template <typename T>
		void subscribe(EventSubscriber<T>* subscriber)
		{
			assert(subscriber != nullptr);

			auto type_index = std::type_index(typeid(T));
			auto iter = _subscribers.find(type_index);
			if (iter == _subscribers.end())
			{
				std::vector<IEventSubscriber*> vec;
				vec.emplace_back(subscriber);

				_subscribers[type_index] = std::move(vec);
			}
			else 
			{
				iter->second.emplace_back(subscriber);
			}
		}

		template <typename T>
		void unsubscribe(EventSubscriber<T>* subscriber)
		{
			auto type_index = std::type_index(typeid(T));
			auto iter = _subscribers.find(type_index);
			if (iter != _subscribers.end())
			{
				iter->second.erase(std::remove(iter->second.begin(), iter->second.end(), subscriber), iter->second.end());
				if (iter->second.size() == 0)
				{
					_subscribers.erase(iter);
				}
			}
		}

		void unsubscribe_all(void* system)
		{
			for (auto& subscriber : _subscribers)
			{
				subscriber.second.erase(std::remove(subscriber.second.begin(), subscriber.second.end(), system), subscriber.second.end());
				
				if (subscriber.second.empty())
				{
					_subscribers.erase(subscriber.first);
				}
			}

		}


		template <typename T>
		bool broadcast(const T& event)
		{
			auto iter = _subscribers.find(std::type_index(typeid(T)));
			if (iter != _subscribers.end())
			{
				for (const auto& subscriber : iter->second)
				{
					ReturnIfFalse(static_cast<EventSubscriber<T>*>(subscriber)->publish(this, event));
				}
			}
			return true;
		}

		template <typename... ComponentTypes>
		EntityView<ComponentTypes...> get_entity_view(bool include_pending_destroy = false)
		{
			return EntityView<ComponentTypes...>(
				EntityIterator<ComponentTypes...>(this, 0, false, include_pending_destroy), 
				EntityIterator<ComponentTypes...>(this, get_entity_num(), true, include_pending_destroy)
			);
		}

		template <typename... ComponentTypes>
		bool each(typename std::common_type<std::function<bool(Entity*, ComponentTypes*...)>>::type func, bool include_pending_destroy = false)
		{
			auto view = get_entity_view<ComponentTypes...>();
			for (auto* entity : view)
			{
				ReturnIfFalse(func(entity, entity->template get_component<ComponentTypes>()...));
			}
			return true;
		}

		EntityView<> get_entity_view(bool include_pending_destroy = false)
		{
			return EntityView<>(
				EntityIterator<>(this, 0, false, include_pending_destroy), 
				EntityIterator<>(this, get_entity_num(), true, include_pending_destroy)
			);
		}

		bool all(std::function<bool(Entity*)> func, bool include_pending_destroy = false)
		{
			auto view = get_entity_view();
			for (auto* entity : view)
			{
				ReturnIfFalse(func(entity));
			}
			return true;
		}

		uint64_t get_entity_num() const { return _entities.size(); }
		Entity* get_entity(uint64_t index) const { return _entities[index].get(); }

	private:
		std::vector<std::unique_ptr<Entity>> _entities;
		std::vector<std::unique_ptr<Entity>> _delay_entities;
		std::vector<std::unique_ptr<EntitySystemInterface>> _systems;
		std::vector<std::unique_ptr<EntitySystemInterface>> disabled_systems;
		std::unordered_map<std::type_index, std::vector<IEventSubscriber*>> _subscribers;
	};


	template <typename... ComponentTypes>
	EntityIterator<ComponentTypes...>::EntityIterator(World* world, uint64_t component_index, bool is_last_component, bool include_pending_destroy) :
		_world(world), 
		_entity_index(component_index), 
		_is_last_entity(is_last_component), 
		_include_pending_destroy(is_last_component)
	{
		if (_entity_index == world->get_entity_num() - 1) _is_last_entity = true;
	}

	template <typename... ComponentTypes>
	bool EntityIterator<ComponentTypes...>::is_end() const
	{
		return _entity_index >= _world->get_entity_num();
	}

	template <typename... ComponentTypes>
	Entity* EntityIterator<ComponentTypes...>::get_entity() const
	{
		if (is_end()) return nullptr;
		return _world->get_entity(_entity_index);
	}

	template <typename... ComponentTypes>
	EntityIterator<ComponentTypes...>& EntityIterator<ComponentTypes...>::operator++()
	{
		_entity_index++;
		while (
			_entity_index < _world->get_entity_num() &&
			(
				get_entity() == nullptr ||
				!get_entity()->template contain<ComponentTypes...>() ||
				(get_entity()->is_pending_destroy() && !_include_pending_destroy)
			)
		)
		{
			_entity_index++;
		}

		if (_entity_index >= _world->get_entity_num()) _is_last_entity = true;
		return *this;
	}

	template <typename... ComponentTypes>
	EntityView<ComponentTypes...>::EntityView(const EntityIterator<ComponentTypes...>& begin, const EntityIterator<ComponentTypes...>& end) :
		_begin(begin), _end(end)
	{
		if (
			_begin.get_entity() == nullptr ||
			!(*_begin)->template contain<ComponentTypes...>() ||
			(_begin.get_entity()->is_pending_destroy() && !_begin._include_pending_destroy)
		)
		{
			++_begin;
		}
	}

	template <typename T>
	struct TComponentContainer : public ComponentContainerInterface
	{
		TComponentContainer() = default;
		TComponentContainer(const T& data) : _data(data) 
		{
		}

		bool removed(Entity* entity) override
		{
			return entity->get_world()->broadcast<event::OnComponentRemoved<T>>(event::OnComponentRemoved<T>{ entity, &_data });
		}

		T _data;
	};

	template <typename T>
	T* Entity::get_component() const
	{
		auto iter = _components.find(std::type_index(typeid(T)));
		if (iter != _components.end())
		{
			return &(reinterpret_cast<TComponentContainer<T>*>(iter->second.get())->_data);
		}
		return nullptr;
	}

	template <typename T, typename... Args>
	requires std::is_constructible_v<T, Args...>
	T* Entity::assign(Args&&... arguments)
	{
		auto type_index = std::type_index(typeid(T));
		auto iter = _components.find(type_index);
		if (iter != _components.end())
		{
			auto* container = reinterpret_cast<TComponentContainer<T>*>(iter->second.get());
			container->_data = T(arguments...);
			if (!_world->broadcast<event::OnComponentAssigned<T>>(event::OnComponentAssigned<T>{ this, &container->_data })) return nullptr;
			return &container->_data;
		}
		else
		{
			std::unique_ptr<TComponentContainer<T>> pContainer = std::make_unique<TComponentContainer<T>>(T(arguments...));

			T* ret = &pContainer->_data;
			if (!_world->broadcast<event::OnComponentAssigned<T>>(event::OnComponentAssigned<T>{ this, ret })) return nullptr;

			{
				std::lock_guard Lock(_component_mutex);
				_components[type_index] = std::move(pContainer);
			}

			return ret;
		}
	}
}









#endif