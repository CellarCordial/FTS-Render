#ifndef RHI_SHADER_H
#define RHI_SHADER_H

#include "../core/tools/log.h"
#include <cstdint>
#include <string>
#include <stdint.h>
#include <vector>

namespace fantasy
{
    enum class ShaderType : uint16_t
    {
        None            = 0,

        Compute         = 1 << 1,

        Vertex          = 1 << 2,
        Hull            = 1 << 3,
        Domain          = 1 << 4,
        Geometry        = 1 << 5,
        Pixel           = 1 << 6,

        RayGeneration   = 1 << 7,
        AnyHit          = 1 << 8,
        ClosestHit      = 1 << 9,
        Miss            = 1 << 10,
        Intersection    = 1 << 11,
        Callable        = 1 << 12,
        RayTracing      = 1 << 13,

        All             = 1 << 14,
    };

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
