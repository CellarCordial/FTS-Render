#ifndef DYNAMIC_RHI_FORMAT_H
#define DYNAMIC_RHI_FORMAT_H

#include <stdint.h>

namespace fantasy
{
    enum class Format : uint32_t
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

        NUM,
    };

    enum class FormatKind : uint8_t
    {
        Integer,
        Float,
        normalized,
        DepthStencil
    };

    struct FormatInfo
    {
        Format format;
        const char* name;
        uint8_t size;
        FormatKind format_kind;
        bool has_red : 1;
        bool has_green : 1;
        bool has_blue : 1;
        bool has_alpha : 1;
        bool has_depth : 1;
        bool has_stencil : 1;
        bool is_signed : 1;
        bool is_sRGB : 1;
    };

    FormatInfo get_format_info(Format format);
}





#endif