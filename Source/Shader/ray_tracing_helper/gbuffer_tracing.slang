#ifndef SHADER_RAY_TRACING_HELPER_GBUFFER_TRACING_SLANG
#define SHADER_RAY_TRACING_HELPER_GBUFFER_TRACING_SLANG

#include "ray_cone.slang"
#include "../math_helper.slang"

namespace ray_tracing
{
    struct GBufferRayPayload
    {
        __init(float hit_time, uint path_length, RayCone ray_cone)
        {
            _hit_time = hit_time;
            _path_length = path_length;
            _ray_cone = ray_cone;
        }

        bool is_hit()
        {
            return _hit_time != MAX_FLOAT;
        }

        float _hit_time = MAX_FLOAT;
        uint _path_length = 0;
        RayCone _ray_cone = RayCone(0.0f);
    };

    struct GBufferTraceResult
    {
        bool is_hit;
        float3 hit_position;
        float ray_hit_time;
    };

    struct GBufferTracing
    {
        GBufferTraceResult trace(RaytracingAccelerationStructure accel_struct)
        {
            GBufferRayPayload payload = GBufferRayPayload(MAX_FLOAT, _path_length, _ray_cone);

            uint trace_flags = 0;
            if (_cull_back_face) trace_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

            TraceRay(accel_struct, trace_flags, 0xff, 0, 0, 0, _ray_desc, payload);

            GBufferTraceResult res;
            if (payload.is_hit())
            {
                res.is_hit = true;
                res.ray_hit_time = payload._hit_time;
                res.hit_position = _ray_desc.Origin + _ray_desc.Direction * payload._hit_time;
            }
            else
            {
                res.is_hit = false;
                res.ray_hit_time = MAX_FLOAT;
            }
            return res;
        }

        RayCone _ray_cone;
        RayDesc _ray_desc;
        uint _path_length;
        bool _cull_back_face;
    };

}




#endif