#ifndef SHADER_RAY_TRACING_RESTIR_HELPER_SLANG
#define SHADER_RAY_TRACING_RESTIR_HELPER_SLANG

namespace restir
{
    struct Reservoir
    {
        __init(uint2 compressed_data)
        {
            _w_sum = 0;
            _payload = compressed_data.x;
            _M = f16tof32(compressed_data.y & 0xffff);
            _W = f16tof32((compressed_data.y >> 16) & 0xffff);
        }

        uint2 compress()
        {
            return uint2(_payload, (f32tof16(_W) << 16u) | f32tof16(_M));
        }

        float _M = 0.0f;
        float _W = 0.0f;
        uint _payload = 0;
        float _w_sum = 0.0f;
    };
}



#endif