#include "../include/Entity.h"
#include "../include/ComIntf.h"
#include <algorithm>
#include <memory>


namespace FTS 
{
    FEntity::FEntity(FWorld* pWorld, UINT64 stID) : m_pWorld(pWorld), m_stIndex(stID)
    {
    }

    FEntity::~FEntity()
    {
        RemoveAll();
    }

    FWorld* FEntity::GetWorld() const { return m_pWorld; }

    
    UINT64 FEntity::GetID() const { return m_stIndex; }

    BOOL FEntity::IsPendingDestroy() const { return m_bPendingDestroy; }

    void FEntity::RemoveAll()
    {
        for (const auto& [TypeIndex, crpComponent] : m_Components)
        {
            crpComponent->Removed(this);
        }

		{
			std::lock_guard Lock(m_ComponentMutex);
            m_Components.clear();   
		}
    }


	FWorld::~FWorld()
    {
        for (auto& rpSystem : m_pSystems) assert(rpSystem->Destroy());

        for (auto& rpEntity : m_pEntities)
        {
            if (!rpEntity->IsPendingDestroy())
            {
                rpEntity->m_bPendingDestroy = true;
            }
        }
        m_pEntities.clear();

        for (auto& rpSystem : m_pSystems) rpSystem.reset();
    }

    FEntity* FWorld::CreateEntity()
    {
        UINT64 stIndex = m_pEntities.size();
        m_pEntities.emplace_back(std::make_unique<FEntity>(this, stIndex));

        return m_pEntities.back().get();
    }

    BOOL FWorld::DestroyEntity(FEntity* pEntity, BOOL bImmediately)
    {
        auto CompareFunc = [&pEntity](const auto& crpEntity) -> BOOL
        {
            return crpEntity.get() == pEntity;
        };

        ReturnIfFalse(pEntity && std::find_if(m_pEntities.begin(), m_pEntities.end(), CompareFunc) != m_pEntities.end());

        if (pEntity->IsPendingDestroy())
        {
			if (bImmediately) 
            {
                m_pEntities.erase(std::remove_if(m_pEntities.begin(), m_pEntities.end(), CompareFunc), m_pEntities.end());
            }
            return true;
        }

        pEntity->m_bPendingDestroy = true;
        
        if (bImmediately) 
        {
            m_pEntities.erase(std::remove_if(m_pEntities.begin(), m_pEntities.end(), CompareFunc), m_pEntities.end());
        }
        return true;
    }

    BOOL FWorld::Tick(FLOAT fDelta)
    {
        CleanUp();
        for (const auto& crpSystem : m_pSystems)
        {
            crpSystem->Tick(this, fDelta);
        }
        return true;
    }

    void FWorld::CleanUp()
    {
        m_pEntities.erase(
            std::remove_if(
                m_pEntities.begin(), 
                m_pEntities.end(), 
                [this](auto& crpEntity)
                {
                    if (crpEntity->IsPendingDestroy())
                    {
                        crpEntity.reset();
                        return true;
                    }
                    return false;
                }
            ),
            m_pEntities.end()
        );
    }

    BOOL FWorld::Reset()
    {
        for (auto& rpEntity : m_pEntities)
        {
            if (!rpEntity->IsPendingDestroy())
            {
                rpEntity->m_bPendingDestroy = true;
            }
        }
        m_pEntities.clear();
        return true;
    }

    
    IEntitySystem* FWorld::RegisterSystem(IEntitySystem* pSystem)
    {
        if (!pSystem || !pSystem->Initialize(this))
        {
            LOG_ERROR("Register entity system failed.");
            return nullptr;
        }

        m_pSystems.emplace_back(pSystem);
        return pSystem;
    }

    BOOL FWorld::UnregisterSystem(IEntitySystem* pSystem)
    {
        ReturnIfFalse(pSystem->Destroy());

        m_pSystems.erase(
            std::remove_if(
                m_pSystems.begin(), 
                m_pSystems.end(), 
                [&pSystem](const auto& crpSystem)
                {
                    return crpSystem.get() == pSystem;
                }
            ), 
            m_pSystems.end()
        );
        return true;
    }

    void FWorld::DisableSystem(IEntitySystem* pSystem)
    {
        if (!pSystem) return;

        auto Iter = std::find_if(
            m_pSystems.begin(), 
            m_pSystems.end(), 
            [&pSystem](const auto& crpSystem)
            {
                return crpSystem.get() == pSystem;
            }
        );

        if (Iter != m_pSystems.end())
        {
            m_pDisabledSystems.push_back(std::move(*Iter));
            m_pSystems.erase(Iter);
        }
    }

    void FWorld::EnableSystem(IEntitySystem* pSystem)
    {
        if (!pSystem) return;

        auto Iter = std::find_if(
            m_pDisabledSystems.begin(), 
            m_pDisabledSystems.end(), 
            [&pSystem](const auto& crpSystem)
            {
                return crpSystem.get() == pSystem;
            }
        );

        if (Iter != m_pDisabledSystems.end())
        {
            m_pSystems.push_back(std::move(*Iter));
            m_pDisabledSystems.erase(Iter);
        }
    }

	TEntityView<>::TEntityView(const TEntityIterator<>& crBegin, const TEntityIterator<>& crEnd) :
		Begin(crBegin), End(crEnd)
	{
		if (
			Begin.GetEntity() == nullptr ||
			(Begin.GetEntity()->IsPendingDestroy() && !Begin.bIncludePendingDestroy)
		)
		{
			++Begin;
		}
	}

	TEntityIterator<>::TEntityIterator(FWorld* pWorld, UINT64 stComponentIndex, BOOL bIsLastComponent, BOOL bIncludePendingDestroy) :
		pWorld(pWorld), 
		stEntityIndex(stComponentIndex), 
		bIsLastEntity(bIsLastComponent), 
		bIncludePendingDestroy(bIsLastComponent)
	{
		if (stEntityIndex == pWorld->GetEntityNum() - 1) bIsLastEntity = true;
	}

	BOOL TEntityIterator<>::IsEnd() const
	{
		return bIsLastEntity || stEntityIndex >= pWorld->GetEntityNum();
	}

	FEntity* TEntityIterator<>::GetEntity() const
	{
		if (IsEnd()) return nullptr;
		return pWorld->GetEntity(stEntityIndex);
	}

	TEntityIterator<>& TEntityIterator<>::operator++()
	{
		stEntityIndex++;
		while (
			stEntityIndex < pWorld->GetEntityNum() &&
			(
				GetEntity() == nullptr ||
				(GetEntity()->IsPendingDestroy() && !bIncludePendingDestroy)
			)
		)
		{
			stEntityIndex++;
		}

		if (stEntityIndex >= pWorld->GetEntityNum()) bIsLastEntity = true;
		return *this;
	}


}