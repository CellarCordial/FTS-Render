#ifndef RHI_H
#define RHI_H

#include "../../Core/include/SysCall.h"

namespace FTS
{
 
    enum class EFormat : UINT8
    {
        UNKNOWN,

        R8_UINT,
        R8_SINT,
        R8_UNORM,
        R8_SNORM,
        RG8_UINT,
        RG8_SINT,
        RG8_UNORM,
        RG8_SNORM,
        R16_UINT,
        R16_SINT,
        R16_UNORM,
        R16_SNORM,
        R16_FLOAT,
        BGRA4_UNORM,
        B5G6R5_UNORM,
        B5G5R5A1_UNORM,
        RGBA8_UINT,
        RGBA8_SINT,
        RGBA8_UNORM,
        RGBA8_SNORM,
        BGRA8_UNORM,
        SRGBA8_UNORM,
        SBGRA8_UNORM,
        R10G10B10A2_UNORM,
        R11G11B10_FLOAT,
        RG16_UINT,
        RG16_SINT,
        RG16_UNORM,
        RG16_SNORM,
        RG16_FLOAT,
        R32_UINT,
        R32_SINT,
        R32_FLOAT,
        RGBA16_UINT,
        RGBA16_SINT,
        RGBA16_FLOAT,
        RGBA16_UNORM,
        RGBA16_SNORM,
        RG32_UINT,
        RG32_SINT,
        RG32_FLOAT,
        RGB32_UINT,
        RGB32_SINT,
        RGB32_FLOAT,
        RGBA32_UINT,
        RGBA32_SINT,
        RGBA32_FLOAT,

        D16,
        D24S8,
        X24G8_UINT,
        D32,
        D32S8,
        X32G8_UINT,

        BC1_UNORM,
        BC1_UNORM_SRGB,
        BC2_UNORM,
        BC2_UNORM_SRGB,
        BC3_UNORM,
        BC3_UNORM_SRGB,
        BC4_UNORM,
        BC4_SNORM,
        BC5_UNORM,
        BC5_SNORM,
        BC6H_UFLOAT,
        BC6H_SFLOAT,
        BC7_UNORM,
        BC7_UNORM_SRGB,

        NUM,
    };

    // 像素数据格式
    enum class EFormatKind : UINT8
    {
        Integer,
        Float,
        Normalized,     // 标准化格式，通常用于表示浮点数范围在0到1之间的值
        DepthStencil
    };

    struct FFormatInfo
    {
        EFormat Format;
        const CHAR* strName;
        UINT8 btBytesPerBlock;      // 数据块（像素）的字节大小
        UINT8 btBlockSize;          // 数据块（像素）中元素的数量    todo: 不确定
        EFormatKind FormatKind;
        BOOL bHasRed : 1;
        BOOL bHasGreen : 1;
        BOOL bHasBlue : 1;
        BOOL bHasAlpha : 1;
        BOOL bHasDepth : 1;
        BOOL bHasStencil : 1;
        BOOL bIsSigned : 1;
        BOOL bIsSRGB : 1;
    };

    FFormatInfo GetFormatInfo(EFormat Format);

    enum class EFormatSupport : UINT32
    {
        None            = 0,

        Buffer          = 0x00000001,
        IndexBuffer     = 0x00000002,
        VertexBuffer    = 0x00000004,

        Texture         = 0x00000008,
        DepthStencil    = 0x00000010,
        RenderTarget    = 0x00000020,
        Blendable       = 0x00000040,

        ShaderLoad      = 0x00000080,
        ShaderSample    = 0x00000100,
        ShaderUavLoad   = 0x00000200,
        ShaderUavStore  = 0x00000400,
        ShaderAtomic    = 0x00000800,
    };
}





#endif