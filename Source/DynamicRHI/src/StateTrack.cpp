#include "StateTrack.h"
#include "DX12/DX12Forward.h"
#include <memory>
#include <utility>
#include <vector>


namespace FTS 
{
    void FResourceStateTracker::SetTextureEnableUAVBarriers(ITextureStateTrack* pTexture, BOOL bEnableBarriers)
    {
        FTextureState* pStatesTrack = GetTextureStateTrack(pTexture);
        pStatesTrack->bEnableUAVBarriers = bEnableBarriers;
        pStatesTrack->bFirstUAVBarrierPlaced = false;
    }

    void FResourceStateTracker::SetBufferEnableUAVBarriers(IBufferStateTrack* pBuffer, BOOL bEnableBarriers)
    {
        FBufferState* pStatesTrack = GetBufferStateTrack(pBuffer);
        pStatesTrack->bEnableUAVBarriers = bEnableBarriers;
        pStatesTrack->bFirstUAVBarrierPlaced = false;
    }

    EResourceStates FResourceStateTracker::GetTextureSubresourceState(ITextureStateTrack* pTexture, UINT32 dwArraySlice, UINT32 dwMipLevel)
    {
        auto pTracking = GetTextureStateTrack(pTexture);

        FTextureDesc TextureDesc = pTexture->GetDesc();

        UINT32 dwSubresourceIndex = CalcTextureSubresource(dwMipLevel, dwArraySlice, TextureDesc);
        return pTracking->SubresourceStates[dwSubresourceIndex];
    }

    EResourceStates FResourceStateTracker::GetBufferSubresourceState(IBufferStateTrack* pBuffer)
    {
        return GetBufferStateTrack(pBuffer)->State;
    }

    BOOL FResourceStateTracker::RequireTextureState(ITextureStateTrack* pTexture, const FTextureSubresourceSet& crSubresourceSet, EResourceStates State)
    {
        FTextureDesc Desc = pTexture->GetDesc();
        FTextureSubresourceSet SubresourceSet = crSubresourceSet.Resolve(Desc, false);

        FTextureState* pTrackState = GetTextureStateTrack(pTexture);
        if (SubresourceSet.IsEntireTexture(Desc) && pTrackState->SubresourceStates.size() == 1)
        {
            BOOL bIsTransitionNecessary = pTrackState->State != State;
            BOOL bIsUavNecessary = ((State & EResourceStates::UnorderedAccess) == EResourceStates::UnorderedAccess) &&
                                   (pTrackState->bEnableUAVBarriers && !pTrackState->bFirstUAVBarrierPlaced);
            if (bIsTransitionNecessary || bIsUavNecessary)
            {
                FTextureBarrier Barrier;
                Barrier.pTexture = pTexture;
                Barrier.bEntireTexture = true;
                Barrier.StateBefore = pTrackState->State;
                Barrier.StateAfter = State;
                m_TextureBarriers.push_back(Barrier);
            }
            
            pTrackState->State = State;

            if (!bIsTransitionNecessary && bIsUavNecessary)
            {
                pTrackState->bFirstUAVBarrierPlaced = true;
            }
        }
        else 
        {
            BOOL bAnyUavBarrier = false;
            for (UINT32 dwArraySliceIndex = SubresourceSet.dwBaseArraySliceIndex; dwArraySliceIndex < SubresourceSet.dwBaseArraySliceIndex + SubresourceSet.dwArraySlicesNum; ++dwArraySliceIndex)
            {
                for (UINT32 dwMipLevelIndex = SubresourceSet.dwBaseMipLevelIndex; dwMipLevelIndex < SubresourceSet.dwBaseMipLevelIndex + SubresourceSet.dwMipLevelsNum; ++dwMipLevelIndex)
                {
                    UINT32 dwSubresourceIndex = CalcTextureSubresource(dwMipLevelIndex, dwArraySliceIndex, Desc);
                    EResourceStates PriorState = pTrackState->SubresourceStates[dwSubresourceIndex];
                    
                    BOOL bIsTransitionNecessary = PriorState != State;
                    BOOL bIsUavNecessary = ((State & EResourceStates::UnorderedAccess) != 0) && 
                                            !bAnyUavBarrier && 
                                            (pTrackState->bEnableUAVBarriers &&
                                            !pTrackState->bFirstUAVBarrierPlaced);

                    if (bIsTransitionNecessary || bIsUavNecessary)
                    {
                        FTextureBarrier Barrier;
                        Barrier.pTexture = pTexture;
                        Barrier.bEntireTexture = false;
                        Barrier.dwMipLevel = dwMipLevelIndex;
                        Barrier.dwArraySlice = dwArraySliceIndex;
                        Barrier.StateBefore = PriorState;
                        Barrier.StateAfter = State;
                        m_TextureBarriers.push_back(Barrier);
                    }
                    
                    pTrackState->SubresourceStates[dwSubresourceIndex] = State;

                    if (!bIsTransitionNecessary && bIsUavNecessary)
                    {
                        bAnyUavBarrier = true;
                        pTrackState->bFirstUAVBarrierPlaced = true;
                    }
                }
            }
        }
        return true;
    }
    
    BOOL FResourceStateTracker::RequireBufferState(IBufferStateTrack* pBuffer, EResourceStates State)
    {
        FBufferDesc Desc = pBuffer->GetDesc();

        // CPU-visible buffers 不能改变 state
        if (Desc.bIsVolatile || Desc.CpuAccess != ECpuAccessMode::None)
        {
            if (GetBufferStateTrack(pBuffer)->State == State)
            {
                return true;
            }
            else
            {
				LOG_ERROR("CPU-visible buffers can't change state.");
				return false;
            }
        }

        FBufferState* pTrack = GetBufferStateTrack(pBuffer);

        BOOL bIsTransitionNecessary = pTrack->State != State;
        if (bIsTransitionNecessary)
        {
            // 若 Barriers 中已经存在同 Buffer 的 Barrier.
            for (auto& rBarrier : m_BufferBarriers)
            {
                if (rBarrier.pBuffer == pBuffer)
                {
                    rBarrier.StateAfter = static_cast<EResourceStates>(rBarrier.StateAfter | State);
                    pTrack->State = rBarrier.StateAfter;
                }
            }
        }

        BOOL bIsUavNecessary = ((State & EResourceStates::UnorderedAccess) == EResourceStates::UnorderedAccess) && 
                               (pTrack->bEnableUAVBarriers || !pTrack->bFirstUAVBarrierPlaced);
        if (bIsTransitionNecessary || bIsUavNecessary)
        {
            FBufferBarrier Barrier;
            Barrier.pBuffer = pBuffer;
            Barrier.StateBefore = pTrack->State;
            Barrier.StateAfter = State;
            m_BufferBarriers.push_back(Barrier);
        }

        if (!bIsTransitionNecessary && bIsUavNecessary)
        {
            pTrack->bFirstUAVBarrierPlaced = true;
        }

        pTrack->State = State;
		return true;
	}

    
    const std::vector<FTextureBarrier>& FResourceStateTracker::GetTextureBarriers() const
    {
        return m_TextureBarriers;
    }

    const std::vector<FBufferBarrier>& FResourceStateTracker::GetBufferBarriers() const
    {
        return m_BufferBarriers;
    }

    void FResourceStateTracker::ClearBarriers()
    {
        m_TextureBarriers.clear();
        m_BufferBarriers.clear();
    }

	FTextureState* FResourceStateTracker::GetTextureStateTrack(ITextureStateTrack* pTexture)
    {
        auto Iter = m_TextureStateMap.find(pTexture);
        if (Iter != m_TextureStateMap.end()) return Iter->second.get();

        std::unique_ptr<FTextureState> pTrack = std::make_unique<FTextureState>();
        FTextureState* pTextureStateTrack = pTrack.get();
        m_TextureStateMap.insert(std::make_pair(pTexture, std::move(pTrack)));
        
        FTextureDesc Desc = pTexture->GetDesc();
        pTextureStateTrack->State = Desc.InitialState;
        pTextureStateTrack->SubresourceStates.resize(static_cast<SIZE_T>(Desc.dwMipLevels * Desc.dwArraySize), pTextureStateTrack->State);

        return pTextureStateTrack;
    }

    FBufferState* FResourceStateTracker::GetBufferStateTrack(IBufferStateTrack* pBuffer)
    {
        auto Iter = m_BufferStateMap.find(pBuffer);
        if (Iter != m_BufferStateMap.end()) return Iter->second.get();

        std::unique_ptr<FBufferState> pTrack = std::make_unique<FBufferState>();
        FBufferState* pBufferStateTrack = pTrack.get();
        m_BufferStateMap.insert(std::make_pair(pBuffer, std::move(pTrack)));
        
        FBufferDesc Desc = pBuffer->GetDesc();
        pBufferStateTrack->State = Desc.InitialState;

        return pBufferStateTrack;
    }

}