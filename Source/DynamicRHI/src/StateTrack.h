#ifndef RHI_STATE_TRACKER_H
#define RHI_STATE_TRACKER_H

/**
 * *****************************************************************************
 * @file        StateTrack.h
 * @brief       
 * @author      CellarCordial (591885295@qq.com)
 * @date        2024-05-29
 * @copyright Copyright (c) 2024
 * *****************************************************************************
 */

#include "../include/Resource.h"
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

 namespace FTS 
 {
    struct FTextureState
    {
        EResourceStates State = EResourceStates::Common;
        std::vector<EResourceStates> SubresourceStates;
        BOOL bEnableUAVBarriers = false;
        BOOL bFirstUAVBarrierPlaced = false;
    };

    struct FBufferState
    {
        EResourceStates State = EResourceStates::Common;
        BOOL bEnableUAVBarriers = false;
        BOOL bFirstUAVBarrierPlaced = false;
    };

    struct FTextureBarrier
    {
        ITextureStateTrack* pTexture = nullptr;
        UINT32 dwMipLevel = 0;
        UINT32 dwArraySlice = 0;
        BOOL bEntireTexture = false;

        EResourceStates StateBefore = EResourceStates::Common;
        EResourceStates StateAfter = EResourceStates::Common;
    };

    struct FBufferBarrier
    {
        IBufferStateTrack* pBuffer = nullptr;

        EResourceStates StateBefore = EResourceStates::Common;
        EResourceStates StateAfter = EResourceStates::Common;
    };

    class FResourceStateTracker
    {
    public:
        void SetTextureEnableUAVBarriers(ITextureStateTrack* pTexture, BOOL bEnableBarriers);
        void SetBufferEnableUAVBarriers(IBufferStateTrack* pBuffer, BOOL bEnableBarriers);

        EResourceStates GetTextureSubresourceState(ITextureStateTrack* pTexture, UINT32 dwArraySlice, UINT32 dwMipLevel);
        EResourceStates GetBufferSubresourceState(IBufferStateTrack* pBuffer);

        BOOL RequireTextureState(ITextureStateTrack* pTexture, const FTextureSubresourceSet& crSubresourceSet, EResourceStates State);
        BOOL RequireBufferState(IBufferStateTrack* pBuffer, EResourceStates State);

        const std::vector<FTextureBarrier>& GetTextureBarriers() const;
        const std::vector<FBufferBarrier>& GetBufferBarriers() const;

        void ClearBarriers();

    private:
        FTextureState* GetTextureStateTrack(ITextureStateTrack* pTexture);
        FBufferState* GetBufferStateTrack(IBufferStateTrack* pBuffer);


    private:
        std::unordered_map<ITextureStateTrack*, std::unique_ptr<FTextureState>> m_TextureStateMap;
        std::unordered_map<IBufferStateTrack*, std::unique_ptr<FBufferState>> m_BufferStateMap;

        std::vector<FTextureBarrier> m_TextureBarriers;
        std::vector<FBufferBarrier> m_BufferBarriers;
    };
 }

 #endif