#ifndef SHADER_RAY_TRACING_HELPER_GBUFFER_TRACING_SLANG
#define SHADER_RAY_TRACING_HELPER_GBUFFER_TRACING_SLANG

#include "ray_cone.slang"
#include "../common/math.slang"

struct GeometryConstant
{
    float4x4 world_matrix;
    float4x4 inv_trans_world;

    float4 base_color;
    float4 emissive;
    float roughness;
    float metallic;
    float occlusion;

    uint2 texture_resolution;
};

float3 calculate_normal(float3 texture_normal, float3 vertex_normal, float3 vertex_tangent)
{
    float3 unpacked_normal = texture_normal * 2.0f - 1.0f;
    float3 N = vertex_normal;
    float3 T = normalize(vertex_tangent.xyz - N * dot(vertex_tangent.xyz, N));
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(unpacked_normal, TBN));
}

struct MeshCluster
{
    float4 bounding_sphere;
    float4 lod_bounding_sphere;

    uint32_t mip_level;
    uint32_t group_id;
    float lod_error;

    uint32_t vertex_offset;
    uint32_t triangle_offset;
    uint32_t triangle_count;

    uint32_t geometry_id;
};

struct MeshClusterGroup
{
    float4 bounding_sphere;

    uint32_t cluster_count;
    uint32_t cluster_index_offset;
    float max_parent_lod_error;
};

// namespace ray_tracing
// {
//     struct GBufferData
//     {
//         float3 view_space_position = float3(0, 0, 0);
//         float3 view_space_normal = float3(0, 0, 0);
//         float4 base_color = float4(0, 0, 0, 0);
//         float4 pbr = float4(0, 0, 0, 0);
//     };

//     struct GBufferRayPayload
//     {
//         __init(float hit_time, uint32_t path_length, RayCone ray_cone)
//         {
//             _hit_time = hit_time;
//             _path_length = path_length;
//             _ray_cone = ray_cone;
//             _data = GBufferData();
//         }

//         bool is_hit()
//         {
//             return _hit_time != MAX_FLOAT;
//         }

//         float _hit_time = MAX_FLOAT;
//         uint32_t _path_length = 0;
//         RayCone _ray_cone = RayCone(0.0f);
//         GBufferData _data;
//     };

//     struct GBufferTraceResult
//     {
//         bool is_hit = false;
//         float3 hit_position = float3(0, 0, 0);
//         float ray_hit_time = 0.0f;
//         GBufferData data;
//     };

//     struct GBufferTracing
//     {
//         GBufferTraceResult trace(RaytracingAccelerationStructure accel_struct)
//         {
//             GBufferRayPayload payload = GBufferRayPayload(MAX_FLOAT, _path_length, _ray_cone);

//             uint32_t trace_flags = 0;
//             if (_cull_back_face) trace_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

//             TraceRay(accel_struct, trace_flags, 0xff, 0, 0, 0, _ray_desc, payload);

//             GBufferTraceResult res;
//             if (payload.is_hit())
//             {
//                 res.is_hit = true;
//                 res.ray_hit_time = payload._hit_time;
//                 res.hit_position = _ray_desc.Origin + _ray_desc.Direction * payload._hit_time;
//                 res.data = payload._data;
//             }
//             else
//             {
//                 res.is_hit = false;
//                 res.ray_hit_time = MAX_FLOAT;
//             }
//             return res;
//         }

//         RayCone _ray_cone;
//         RayDesc _ray_desc;
//         uint32_t _path_length;
//         bool _cull_back_face;
//     };
// }




#endif