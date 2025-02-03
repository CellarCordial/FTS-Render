#ifndef RHI_SHADER_H
#define RHI_SHADER_H

#include "../core/tools/log.h"
#include <cstdint>
#include <string>
#include <stdint.h>
#include <vector>
#include "../core/math/common.h"

namespace fantasy
{
    enum class ShaderType : uint16_t
    {
        None            = 0,

        Compute         = 0x01,

        Vertex          = 0x02,
        Hull            = 0x04,
        Domain          = 0x08,
        Geometry        = 0x10,
        Pixel           = 0x20,
        Graphics        = 0x7C,

        RayGeneration   = 0x40,
        AnyHit          = 0x80,
        ClosestHit      = 0x100,
        Miss            = 0x200,
        Intersection    = 0x400,
        Callable        = 0x800,
        RayTracing      = 0xFC0,

        All             = 0x1000,
    };
    ENUM_CLASS_FLAG_OPERATORS(ShaderType);

    struct ShaderDesc
    {
        std::string name;
        ShaderType shader_type = ShaderType::None;
        std::string entry;
    };

    struct ShaderByteCode
    {
        uint8_t* byte_code;
        uint64_t size;

        bool is_valid() { return byte_code != nullptr && size != 0; }
    };

    class Shader
    {
    public:
        Shader(ShaderDesc desc) : _desc(desc) {}

        bool initialize(const void* data, uint64_t size)
        {
            if (size == 0 || data == nullptr)
            {
                LOG_ERROR("Create Shader failed for using empty binary data.");
                return false;
            }

            _data.resize(size);
            memcpy(_data.data(), data, size);

            return true;
        }

        ShaderDesc get_desc() const { return _desc; }
        ShaderByteCode get_byte_code()
        {
            return ShaderByteCode{ .byte_code = _data.data(), .size = _data.size() };
        }

    private:
        ShaderDesc _desc;
        std::vector<uint8_t> _data;
    };


    inline Shader* create_shader(const ShaderDesc& desc, const void* data, uint64_t size)
    {
        Shader* shader = new Shader(desc);
        if (!shader->initialize(data, size))
        {
            LOG_ERROR("Create shader failed.");
            delete shader;
            return nullptr;
        }
        return shader;
    }
}


#endif
