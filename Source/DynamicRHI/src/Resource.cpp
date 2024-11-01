#include "../include/Resource.h"
#include <algorithm>


namespace FTS
{
    FTextureSlice FTextureSlice::Resolve(const FTextureDesc& crDesc) const
    {
        FTextureSlice Ret(*this);

#ifndef NDEBUG
        assert(dwMipLevel < crDesc.dwMipLevels);
#endif
        if (dwWidth == static_cast<UINT32>(-1))
            Ret.dwWidth = std::max(crDesc.dwWidth >> dwMipLevel, 1u);

        if (dwHeight == static_cast<UINT32>(-1))
            Ret.dwHeight = std::max(crDesc.dwHeight >> dwMipLevel, 1u);

        if (dwDepth == static_cast<UINT32>(-1))
        {
            if (crDesc.Dimension == ETextureDimension::Texture3D)
            {
                Ret.dwDepth = std::max(crDesc.dwDepth >> dwMipLevel, 1u);
                
            }
            else
            {
                Ret.dwDepth = 1;
            }
        }

        return Ret;
    }


    FTextureSubresourceSet FTextureSubresourceSet::Resolve(const FTextureDesc& Desc, BOOL bIsSingleMipLevel) const
    {
        FTextureSubresourceSet Ret(*this);
        if (bIsSingleMipLevel)
        {
            Ret.dwMipLevelsNum = 1;
        }
        else
        {
            const UINT32 dwLastMipLevelPlusOne = std::min(
                dwBaseMipLevelIndex + dwMipLevelsNum,
                Desc.dwMipLevels
            );
            Ret.dwMipLevelsNum = std::max(0u, dwLastMipLevelPlusOne - dwBaseMipLevelIndex);
        }

        switch (Desc.Dimension)
        {
        case ETextureDimension::Texture1DArray:
        case ETextureDimension::Texture2DArray:
        case ETextureDimension::TextureCube:
        case ETextureDimension::TextureCubeArray:
        case ETextureDimension::Texture2DMSArray:
            {
                Ret.dwBaseArraySliceIndex = dwBaseArraySliceIndex;
                
                // 使用 *this 和 crDesc 中较小的 ArraySlicesNum
                const UINT32 dwLastArraySlicePlusOne = std::min(
                    dwBaseArraySliceIndex + dwArraySlicesNum,
                    Desc.dwArraySize
                );
                Ret.dwArraySlicesNum = std::max(0u, dwLastArraySlicePlusOne - dwBaseArraySliceIndex);
            }
            break;
        default:
            Ret.dwBaseArraySliceIndex = 0;
            Ret.dwArraySlicesNum = 1;
        }

        return Ret;
    }

    BOOL FTextureSubresourceSet::IsEntireTexture(const FTextureDesc& crDesc) const
    {
        if (dwBaseMipLevelIndex > 0 || dwBaseMipLevelIndex + dwMipLevelsNum < crDesc.dwMipLevels)
        {
            return false;
        }

        switch (crDesc.Dimension)
        {
        case ETextureDimension::Texture1DArray:
        case ETextureDimension::Texture2DArray:
        case ETextureDimension::TextureCube:
        case ETextureDimension::TextureCubeArray:
        case ETextureDimension::Texture2DMSArray:
            if (dwBaseArraySliceIndex > 0 ||
                dwBaseArraySliceIndex + dwArraySlicesNum < crDesc.dwArraySize)
            {
                return false;
            }
            break;
        default:
            return true;
        }
        return true;
    }

    
    FBufferRange FBufferRange::Resolve(const struct FBufferDesc& crDesc) const
    {   
        FBufferRange Ret;

        // 若 stByteOffset 超过了 crDesc 中所描述的总大小，则选择后者
        Ret.stByteOffset = std::min(stByteOffset, crDesc.stByteSize);

        // 若没有指定 stByteSize, 则用总大小减去偏移
        // 若已经指定 stByteSize, 则在 已经指定的值 和 用总大小减去偏移的结果 之间选择较小值
        if (stByteSize == 0)
        {
            Ret.stByteSize = crDesc.stByteSize - Ret.stByteOffset;
        }
        else
        {
            Ret.stByteSize = std::min(stByteSize, crDesc.stByteSize - Ret.stByteOffset);
        }

        return Ret;
    }

    BOOL FBufferRange::IsEntireBuffer(const struct FBufferDesc& crDesc) const
    {
        return  stByteOffset == 0 && 
                stByteSize == static_cast<UINT64>(-1) ||
                stByteSize == crDesc.stByteSize;

    }


}
