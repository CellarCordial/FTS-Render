#ifndef SHADER_COMPILER_H
#define SHADER_COMPILER_H


#include <basetsd.h>
#include <string>
#include <vector>

namespace fantasy 
{
    enum class ShaderPlatform : uint8_t
    {
        DXIL,
        SPIRV
    };

    enum class ShaderTarget : uint16_t
    {
        None            = 0x0000,

        Compute         = 0x0020,

        Vertex          = 0x0001,
        Hull            = 0x0002,
        Domain          = 0x0004,
        Geometry        = 0x0008,
        Pixel           = 0x0010,

        Num             = 0x3FFF,
    };
    
    struct ShaderCompileDesc
    {
        std::string shader_name;     // Need the file extension.
        std::string entry_point;
        ShaderTarget target = ShaderTarget::None;
        std::vector<std::string> defines;
    };

    struct ShaderData
    {
        std::vector<uint8_t> _data;
        std::vector<std::string> _include_shader_files;

        uint64_t size() const { return _data.size(); }
        uint8_t* data() { return _data.data(); }

        void set_byte_code(const void* data, uint64_t size)
        {
            if (data)
            {
                _data.resize(size);
                memcpy(_data.data(), data, size);
            }
        }

        bool is_valid() const { return !_data.empty(); }
    };

    void set_shader_platform(ShaderPlatform platform);
    ShaderData compile_shader(const ShaderCompileDesc& desc);
}















#endif







