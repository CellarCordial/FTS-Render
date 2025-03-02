#ifndef SHADER_RAY_TRACING_HELPER_GBUFFER_TRACING_HLSL
#define SHADER_RAY_TRACING_HELPER_GBUFFER_TRACING_HLSL

#include "../common/math.hlsl"

struct GeometryConstant
{
    float4x4 world_matrix;
    float4x4 inv_trans_world;

    float4 base_color;
    float occlusion;
    float roughness;
    float metallic;
    float4 emissive;

    uint texture_resolution;
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

    uint mip_level;
    uint group_id;
    float lod_error;

    uint vertex_offset;
    uint triangle_offset;
    uint triangle_count;

    uint geometry_id;
};

struct MeshClusterGroup
{
    float4 bounding_sphere;

    uint mip_level;
    uint cluster_count;
    uint cluster_index_offset;
    float max_parent_lod_error;

    uint max_mip_level;
};


#endif