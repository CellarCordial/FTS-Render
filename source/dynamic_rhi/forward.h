#ifndef RHI_FORWARD_H
#define RHI_FORWARD_H


#include <stdint.h>


namespace fantasy
{
    struct DeviceInterface;
    
    inline const uint32_t MAX_RENDER_TARGETS = 8U;
    inline const uint32_t MAX_VIEWPORTS = 16U;
    inline const uint32_t MAX_VERTEX_ATTRIBUTES = 16U;
    inline const uint32_t MAX_BINDING_LAYOUTS = 5U;
    inline const uint32_t MAX_BINDINGS_PER_LAYOUT = 128U;
    inline const uint32_t MAX_VOLATILE_CONSTANT_BUFFERS_PER_LAYOUT = 6U;
    inline const uint32_t MAX_VOLATILE_CONSTANT_BUFFERS = 32U;         
    inline const uint32_t MAX_PUSH_CONSTANT_SIZE = 128U;  // D3D12: 256 bytes max, Vulkan: 128 bytes max, 所以用 128.
    inline const uint32_t CONSTANT_BUFFER_OFFSET_SIZE_ALIGMENT = 256U;
    
    struct Color
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;

        bool operator==(const Color& crColor) const
        {
            return  a == crColor.a &&
                    r == crColor.r &&
                    g == crColor.g &&
                    b == crColor.b;
        }

        bool operator!=(const Color& crColor) const
        {
            return !((*this) == crColor);
        }
    };

    struct Rect
    {
        uint32_t min_x = 0u, max_x = 0u;
        uint32_t min_y = 0u, max_y = 0u;


        uint32_t get_width() const
        {
            return max_x - min_x;
        }

        uint32_t get_height() const
        {
            return max_y - min_y;
        }

        bool operator==(const Rect& rect) const
        {
            return  max_x == rect.max_x &&
                    max_y == rect.max_y && 
                    min_x == rect.min_x && 
                    min_y == rect.min_y; 
        }

        bool operator!=(const Rect& rect) const
        {
            return !((*this) == rect); 
        }

    };
    
    struct Viewport
    {
        float min_x = 0.0f, max_x = 0.0f;
        float min_y = 0.0f, max_y = 0.0f;
        float min_z = 0.0f, max_z = 0.0f;


        float get_width() const
        {
            return max_x - min_x;
        }

        float get_height() const
        {
            return max_y - min_y;
        }

        bool operator==(const Viewport& viewport) const
        {
            return  max_x == viewport.max_x && 
                    max_y == viewport.max_y && 
                    max_z == viewport.max_z && 
                    min_x == viewport.min_x && 
                    min_y == viewport.min_y && 
                    min_z == viewport.min_z;
        }

        bool operator!=(const Viewport& crViewport) const
        {
            return !((*this) == crViewport);
        }
    };


    enum class ViewType : uint8_t
    {
        DX12_RenderTargetView,
        DX12_DepthStencilView,

        DX12_GPU_Texture_SRV,
        DX12_GPU_Texture_UAV,
		DX12_GPU_TypedBuffer_SRV,
		DX12_GPU_TypedBuffer_UAV,
		DX12_GPU_StructuredBuffer_SRV,
		DX12_GPU_StructuredBuffer_UAV,
		DX12_GPU_RawBuffer_SRV,
		DX12_GPU_RawBuffer_UAV,
    };

    enum class GraphicsAPI : uint8_t
    {
        D3D12,
        Vulkan
    };

    enum class HeapType : uint8_t
    {
        Default,
        Upload,
        Readback
    };

    struct MemoryRequirements
    {
        uint64_t alignment;
        uint64_t size;
    };

    struct HeapDesc
    {
        uint64_t capacity = 0;
        HeapType type;
    };

    struct HeapInterface
    {
        virtual const HeapDesc& get_desc() const = 0;
        
		virtual ~HeapInterface() = default;
    };


}















#endif