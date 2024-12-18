#include "ecs.h"
#include <algorithm>
#include <memory>


namespace fantasy 
{
    Entity::Entity(World* pWorld, uint64_t stID) : _world(pWorld), _index(stID)
    {
    }

    Entity::~Entity()
    {
        remove_all();
    }

    World* Entity::get_world() const { return _world; }

    
    uint64_t Entity::get_id() const { return _index; }

    bool Entity::is_pending_destroy() const { return _is_pending_destroy; }

    void Entity::remove_all()
    {
        for (const auto& [TypeIndex, crpComponent] : _components)
        {
            crpComponent->removed(this);
        }

		{
			std::lock_guard Lock(_component_mutex);
            _components.clear();   
		}
    }


	World::~World()
    {
        for (auto& rpSystem : _systems) assert(rpSystem->Destroy());

        for (auto& rpEntity : _entities)
        {
            if (!rpEntity->is_pending_destroy())
            {
                rpEntity->_is_pending_destroy = true;
            }
        }
        _entities.clear();

        for (auto& rpSystem : _systems) rpSystem.reset();
    }

    Entity* World::create_entity()
    {
        uint64_t stIndex = _entities.size();
        _entities.emplace_back(std::make_unique<Entity>(this, stIndex));

        return _entities.back().get();
    }

    bool World::destroy_entity(Entity* entity, bool immediately)
    {
        auto CompareFunc = [&entity](const auto& crpEntity) -> bool
        {
            return crpEntity.get() == entity;
        };

        ReturnIfFalse(entity && std::find_if(_entities.begin(), _entities.end(), CompareFunc) != _entities.end());

        if (entity->is_pending_destroy())
        {
			if (immediately) 
            {
                _entities.erase(std::remove_if(_entities.begin(), _entities.end(), CompareFunc), _entities.end());
            }
            return true;
        }

        entity->_is_pending_destroy = true;
        
        if (immediately) 
        {
            _entities.erase(std::remove_if(_entities.begin(), _entities.end(), CompareFunc), _entities.end());
        }
        return true;
    }

    bool World::tick(float delta)
    {
        cleanup();
        for (const auto& system : _systems)
        {
            system->tick(delta);
        }
        return true;
    }

    void World::cleanup()
    {
        _entities.erase(
            std::remove_if(
                _entities.begin(), 
                _entities.end(), 
                [this](auto& crpEntity)
                {
                    if (crpEntity->is_pending_destroy())
                    {
                        crpEntity.reset();
                        return true;
                    }
                    return false;
                }
            ),
            _entities.end()
        );
    }

    bool World::reset()
    {
        for (auto& entity : _entities)
        {
            if (!entity->is_pending_destroy())
            {
                entity->_is_pending_destroy = true;
            }
        }
        _entities.clear();
        return true;
    }

    
    EntitySystemInterface* World::register_system(EntitySystemInterface* system)
    {
        if (!system || !system->initialize(this))
        {
            LOG_ERROR("Register entity system failed.");
            return nullptr;
        }

        _systems.emplace_back(system);
        return system;
    }

    bool World::unregister_system(EntitySystemInterface* system)
    {
        ReturnIfFalse(system->destroy());

        _systems.erase(
            std::remove_if(
                _systems.begin(), 
                _systems.end(), 
                [&system](const auto& crpSystem)
                {
                    return crpSystem.get() == system;
                }
            ), 
            _systems.end()
        );
        return true;
    }

    void World::disable_system(EntitySystemInterface* system)
    {
        if (!system) return;

        auto iter = std::find_if(
            _systems.begin(), 
            _systems.end(), 
            [&system](const auto& crpSystem)
            {
                return crpSystem.get() == system;
            }
        );

        if (iter != _systems.end())
        {
            disabled_systems.push_back(std::move(*iter));
            _systems.erase(iter);
        }
    }

    void World::enable_system(EntitySystemInterface* system)
    {
        if (!system) return;

        auto iter = std::find_if(
            disabled_systems.begin(), 
            disabled_systems.end(), 
            [&system](const auto& crpSystem)
            {
                return crpSystem.get() == system;
            }
        );

        if (iter != disabled_systems.end())
        {
            _systems.push_back(std::move(*iter));
            disabled_systems.erase(iter);
        }
    }

	EntityView<>::EntityView(const EntityIterator<>& begin, const EntityIterator<>& end) :
		_begin(begin), _end(end)
	{
		if (
			_begin.get_entity() == nullptr ||
			(_begin.get_entity()->is_pending_destroy() && !_begin._include_pending_destroy)
		)
		{
			++_begin;
		}
	}

	EntityIterator<>::EntityIterator(World* world, uint64_t component_index, bool is_last_component, bool include_pending_destroy) :
		_world(world), 
		_entity_index(component_index), 
		_is_last_entity(is_last_component), 
		_include_pending_destroy(include_pending_destroy)
	{
		if (_entity_index == _world->get_entity_num() - 1) _is_last_entity = true;
	}

	bool EntityIterator<>::is_end() const
	{
		return _is_last_entity || _entity_index >= _world->get_entity_num();
	}

	Entity* EntityIterator<>::get_entity() const
	{
		if (is_end()) return nullptr;
		return _world->get_entity(_entity_index);
	}

	EntityIterator<>& EntityIterator<>::operator++()
	{
		_entity_index++;
		while (
			_entity_index < _world->get_entity_num() &&
			(
				get_entity() == nullptr ||
				(get_entity()->is_pending_destroy() && !_include_pending_destroy)
			)
		)
		{
			_entity_index++;
		}

		if (_entity_index >= _world->get_entity_num()) _is_last_entity = true;
		return *this;
	}


}