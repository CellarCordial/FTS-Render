#ifndef RHI_FORWARD_H
#define RHI_FORWARD_H


#include "../../Core/include/ComIntf.h"


namespace FTS
{
    // Forward Declaration
    struct IDevice;

    /**
     * @brief       Simple color struct. 
     * 
     */
    struct FColor
    {
        FLOAT r = 1.0f;
        FLOAT g = 1.0f;
        FLOAT b = 1.0f;
        FLOAT a = 1.0f;

        BOOL operator==(const FColor& crColor) const
        {
            return  a == crColor.a &&
                    r == crColor.r &&
                    g == crColor.g &&
                    b == crColor.b;
        }

        BOOL operator!=(const FColor& crColor) const
        {
            return !((*this) == crColor);
        }
    };

    /**
     * @brief       Simple rectangle struct. 
     * 
     */
    struct FRect
    {
        UINT32 dwMinX = 0u, dwMaxX = 0u;
        UINT32 dwMinY = 0u, dwMaxY = 0u;


        UINT32 GetWidth() const
        {
            return dwMaxX - dwMinX;
        }

        UINT32 GetHeight() const
        {
            return dwMaxY - dwMinY;
        }

        BOOL operator==(const FRect& crRect) const
        {
            return  dwMaxX == crRect.dwMaxX &&
                    dwMaxY == crRect.dwMaxY && 
                    dwMinX == crRect.dwMinX && 
                    dwMinY == crRect.dwMinY; 
        }

        BOOL operator!=(const FRect& crRect) const
        {
            return !((*this) == crRect); 
        }

    };
    
    /**
     * @brief       Simple viewport struct. 
     * 
     */
    struct FViewport
    {
        FLOAT fMinX = 0.0f, fMaxX = 0.0f;
        FLOAT fMinY = 0.0f, fMaxY = 0.0f;
        FLOAT fMinZ = 0.0f, fMaxZ = 0.0f;


        FLOAT GetWidth() const
        {
            return fMaxX - fMinX;
        }

        FLOAT GetHeight() const
        {
            return fMaxY - fMinY;
        }

        BOOL operator==(const FViewport& crViewport) const
        {
            return  fMaxX == crViewport.fMaxX && 
                    fMaxY == crViewport.fMaxY && 
                    fMaxZ == crViewport.fMaxZ && 
                    fMinX == crViewport.fMinX && 
                    fMinY == crViewport.fMinY && 
                    fMinZ == crViewport.fMinZ;
        }

        BOOL operator!=(const FViewport& crViewport) const
        {
            return !((*this) == crViewport);
        }
    };

    // Global Variables. 

    inline const UINT32 gdwMaxRenderTargets = 8;
    inline const UINT32 gdwMaxViewports = 16;
    inline const UINT32 gdwMaxVertexAttributes = 16;
    inline const UINT32 gdwMaxBindingLayouts = 5;
    inline const UINT32 gdwMaxBindingsPerLayout = 128;
    inline const UINT32 gdwMaxVolatileConstantBuffersPerLayout = 6;
    inline const UINT32 gdwMaxVolatileConstantBuffers = 32;         
    inline const UINT32 gdwMaxPushConstantSize = 128;            /**< D3D12: 256 bytes max, Vulkan: 128 bytes max, so use 128. */
    inline const UINT32 gdwConstantBufferOffsetSizeAlignment = 256;
    


    enum class EViewType : UINT32
    {
        DX12_RenderTargetView,
        DX12_DepthStencilView,
        DX12_GPU_ShaderResourceView,
        DX12_GPU_UnorderedAccessView
    };

    enum class EGraphicsAPI : UINT8
    {
        D3D12,
        Vulkan
    };

    enum class EHeapType : UINT8
    {
        Default,
        Upload,
        Readback
    };

    struct FHeapDesc
    {
        UINT64 stCapacity = 0;
        EHeapType Type;
    };

    struct FMemoryRequirements
    {
        UINT64 stAlignment;
        UINT64 stSize;
    };


    extern const IID IID_IHeap;

    struct IHeap : public IUnknown
    {
        virtual FHeapDesc GetDesc() const = 0;
        
		virtual ~IHeap() = default;
    };


    
}















#endif