#ifndef SHADER_RAY_TRACING_HELPER_GBUFFER_TRACING_SLANG
#define SHADER_RAY_TRACING_HELPER_GBUFFER_TRACING_SLANG

#include "ray_cone.slang"
#include "../math_helper.slang"

float3 calculate_normal(float3 texture_normal, float3 vertex_normal, float4 vertex_tangent)
{
    float3 unpacked_normal = texture_normal * 2.0f - 1.0f;
    float3 N = vertex_normal;
    float3 T = normalize(vertex_tangent.xyz - N * dot(vertex_tangent.xyz, N));
    float3 B = cross(N, T) * vertex_tangent.w;
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(unpacked_normal, TBN));
}

namespace ray_tracing
{
    struct GBufferData
    {
        float3 view_space_position = float3(0, 0, 0);
        float3 view_space_normal = float3(0, 0, 0);
        float4 base_color = float4(0, 0, 0, 0);
        float4 pbr = float4(0, 0, 0, 0);
    };

    struct GBufferRayPayload
    {
        __init(float hit_time, uint path_length, RayCone ray_cone)
        {
            _hit_time = hit_time;
            _path_length = path_length;
            _ray_cone = ray_cone;
            _data = GBufferData();
        }

        bool is_hit()
        {
            return _hit_time != MAX_FLOAT;
        }

        float _hit_time = MAX_FLOAT;
        uint _path_length = 0;
        RayCone _ray_cone = RayCone(0.0f);
        GBufferData _data;
    };

    struct GBufferTraceResult
    {
        bool is_hit = false;
        float3 hit_position = float3(0, 0, 0);
        float ray_hit_time = 0.0f;
        GBufferData data;
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
                res.data = payload._data;
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