#include "Format.h"
#include <cassert>

namespace fantasy
{
    // Format mapping table. The rows must be in the exactly same order as Format enum members are defined.
    static constexpr FormatInfo format_infos[] = {
    //        Format                   Name             Bytes      FormatKind            red    green  blue   alpha  depth  stencil signed srgb
        { Format::UNKNOWN,           "UNKNOWN",           0,   FormatKind::Integer,      false, false, false, false, false, false,  false, false },
        { Format::R8_UINT,           "R8_UINT",           1,   FormatKind::Integer,      true,  false, false, false, false, false,  false, false },
        { Format::R8_SINT,           "R8_SINT",           1,   FormatKind::Integer,      true,  false, false, false, false, false,  true,  false },
        { Format::R8_UNORM,          "R8_UNORM",          1,   FormatKind::normalized,   true,  false, false, false, false, false,  false, false },
        { Format::R8_SNORM,          "R8_SNORM",          1,   FormatKind::normalized,   true,  false, false, false, false, false,  true,  false },
        { Format::RG8_UINT,          "RG8_UINT",          2,   FormatKind::Integer,      true,  true,  false, false, false, false,  false, false },
        { Format::RG8_SINT,          "RG8_SINT",          2,   FormatKind::Integer,      true,  true,  false, false, false, false,  true,  false },
        { Format::RG8_UNORM,         "RG8_UNORM",         2,   FormatKind::normalized,   true,  true,  false, false, false, false,  false, false },
        { Format::RG8_SNORM,         "RG8_SNORM",         2,   FormatKind::normalized,   true,  true,  false, false, false, false,  true,  false },
        { Format::R16_UINT,          "R16_UINT",          2,   FormatKind::Integer,      true,  false, false, false, false, false,  false, false },
        { Format::R16_SINT,          "R16_SINT",          2,   FormatKind::Integer,      true,  false, false, false, false, false,  true,  false },
        { Format::R16_UNORM,         "R16_UNORM",         2,   FormatKind::normalized,   true,  false, false, false, false, false,  false, false },
        { Format::R16_SNORM,         "R16_SNORM",         2,   FormatKind::normalized,   true,  false, false, false, false, false,  true,  false },
        { Format::R16_FLOAT,         "R16_FLOAT",         2,   FormatKind::Float,        true,  false, false, false, false, false,  true,  false },
        { Format::BGRA4_UNORM,       "BGRA4_UNORM",       2,   FormatKind::normalized,   true,  true,  true,  true,  false, false,  false, false },
        { Format::B5G6R5_UNORM,      "B5G6R5_UNORM",      2,   FormatKind::normalized,   true,  true,  true,  false, false, false,  false, false },
        { Format::B5G5R5A1_UNORM,    "B5G5R5A1_UNORM",    2,   FormatKind::normalized,   true,  true,  true,  true,  false, false,  false, false },
        { Format::RGBA8_UINT,        "RGBA8_UINT",        4,   FormatKind::Integer,      true,  true,  true,  true,  false, false,  false, false },
        { Format::RGBA8_SINT,        "RGBA8_SINT",        4,   FormatKind::Integer,      true,  true,  true,  true,  false, false,  true,  false },
        { Format::RGBA8_UNORM,       "RGBA8_UNORM",       4,   FormatKind::normalized,   true,  true,  true,  true,  false, false,  false, false },
        { Format::RGBA8_SNORM,       "RGBA8_SNORM",       4,   FormatKind::normalized,   true,  true,  true,  true,  false, false,  true,  false },
        { Format::BGRA8_UNORM,       "BGRA8_UNORM",       4,   FormatKind::normalized,   true,  true,  true,  true,  false, false,  false, false },
        { Format::SRGBA8_UNORM,      "SRGBA8_UNORM",      4,   FormatKind::normalized,   true,  true,  true,  true,  false, false,  false, true  },
        { Format::SBGRA8_UNORM,      "SBGRA8_UNORM",      4,   FormatKind::normalized,   true,  true,  true,  true,  false, false,  false, false },
        { Format::R10G10B10A2_UNORM, "R10G10B10A2_UNORM", 4,   FormatKind::normalized,   true,  true,  true,  true,  false, false,  false, false },
        { Format::R11G11B10_FLOAT,   "R11G11B10_FLOAT",   4,   FormatKind::Float,        true,  true,  true,  false, false, false,  false, false },
        { Format::RG16_UINT,         "RG16_UINT",         4,   FormatKind::Integer,      true,  true,  false, false, false, false,  false, false },
        { Format::RG16_SINT,         "RG16_SINT",         4,   FormatKind::Integer,      true,  true,  false, false, false, false,  true,  false },
        { Format::RG16_UNORM,        "RG16_UNORM",        4,   FormatKind::normalized,   true,  true,  false, false, false, false,  false, false },
        { Format::RG16_SNORM,        "RG16_SNORM",        4,   FormatKind::normalized,   true,  true,  false, false, false, false,  true,  false },
        { Format::RG16_FLOAT,        "RG16_FLOAT",        4,   FormatKind::Float,        true,  true,  false, false, false, false,  true,  false },
        { Format::R32_UINT,          "R32_UINT",          4,   FormatKind::Integer,      true,  false, false, false, false, false,  false, false },
        { Format::R32_SINT,          "R32_SINT",          4,   FormatKind::Integer,      true,  false, false, false, false, false,  true,  false },
        { Format::R32_FLOAT,         "R32_FLOAT",         4,   FormatKind::Float,        true,  false, false, false, false, false,  true,  false },
        { Format::RGBA16_UINT,       "RGBA16_UINT",       8,   FormatKind::Integer,      true,  true,  true,  true,  false, false,  false, false },
        { Format::RGBA16_SINT,       "RGBA16_SINT",       8,   FormatKind::Integer,      true,  true,  true,  true,  false, false,  true,  false },
        { Format::RGBA16_FLOAT,      "RGBA16_FLOAT",      8,   FormatKind::Float,        true,  true,  true,  true,  false, false,  true,  false },
        { Format::RGBA16_UNORM,      "RGBA16_UNORM",      8,   FormatKind::normalized,   true,  true,  true,  true,  false, false,  false, false },
        { Format::RGBA16_SNORM,      "RGBA16_SNORM",      8,   FormatKind::normalized,   true,  true,  true,  true,  false, false,  true,  false },
        { Format::RG32_UINT,         "RG32_UINT",         8,   FormatKind::Integer,      true,  true,  false, false, false, false,  false, false },
        { Format::RG32_SINT,         "RG32_SINT",         8,   FormatKind::Integer,      true,  true,  false, false, false, false,  true,  false },
        { Format::RG32_FLOAT,        "RG32_FLOAT",        8,   FormatKind::Float,        true,  true,  false, false, false, false,  true,  false },
        { Format::RGB32_UINT,        "RGB32_UINT",        12,  FormatKind::Integer,      true,  true,  true,  false, false, false,  false, false },
        { Format::RGB32_SINT,        "RGB32_SINT",        12,  FormatKind::Integer,      true,  true,  true,  false, false, false,  true,  false },
        { Format::RGB32_FLOAT,       "RGB32_FLOAT",       12,  FormatKind::Float,        true,  true,  true,  false, false, false,  true,  false },
        { Format::RGBA32_UINT,       "RGBA32_UINT",       16,  FormatKind::Integer,      true,  true,  true,  true,  false, false,  false, false },
        { Format::RGBA32_SINT,       "RGBA32_SINT",       16,  FormatKind::Integer,      true,  true,  true,  true,  false, false,  true,  false },
        { Format::RGBA32_FLOAT,      "RGBA32_FLOAT",      16,  FormatKind::Float,        true,  true,  true,  true,  false, false,  true,  false },
        { Format::D16,               "D16",               2,   FormatKind::DepthStencil, false, false, false, false, true,  false,  false, false },
        { Format::D24S8,             "D24S8",             4,   FormatKind::DepthStencil, false, false, false, false, true,  true,   false, false },
        { Format::X24G8_UINT,        "X24G8_UINT",        4,   FormatKind::Integer,      false, false, false, false, false, true,   false, false },
        { Format::D32,               "D32",               4,   FormatKind::DepthStencil, false, false, false, false, true,  false,  false, false },
        { Format::D32S8,             "D32S8",             8,   FormatKind::DepthStencil, false, false, false, false, true,  true,   false, false },
        { Format::X32G8_UINT,        "X32G8_UINT",        8,   FormatKind::Integer,      false, false, false, false, false, true,   false, false },
    };


    FormatInfo get_format_info(Format format)
    {
        // 若不存在该 Format, 则返回 UNKNOWN
        if (static_cast<uint32_t>(format) > static_cast<uint32_t>(Format::NUM))
        {
            return format_infos[0];     // Format::UNKNOWN
        }

		FormatInfo ret = format_infos[static_cast<uint32_t>(format)];

        assert(format == ret.format && "get_format_info() failed.");
        return ret;
    }
}
