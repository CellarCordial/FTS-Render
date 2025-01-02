#include "../octahedral.slang"

cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;

    float4x4 view_matrix;
    float4x4 prev_view_matrix;

    uint geometry_constant_index;
    uint3 pad;
};

struct GeometryConstant
{
    float4x4 world_matrix;
    float4x4 inv_trans_world;

    float4 diffuse;
    float4 emissive;
    float roughness;
    float metallic;
    float occlusion;
};

struct Cluster
{
    float3 bounding_sphere_center;
    float bounding_sphere_radius;

    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t vertex_offset;
    uint32_t index_offset;

    uint32_t mip_level;
    uint32_t group_id;
    uint geometry_constant_index;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 uv;
};

StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t5);

StructuredBuffer<Cluster> cluster_buffer : register(t6);
StructuredBuffer<Vertex> vertex_buffer : register(t7);
StructuredBuffer<uint32_t> index_buffer : register(t8);

struct VertexOutput
{
    float4 sv_position : SV_Position;

    float3 world_space_position : WORLD_POSITION;

    float3 view_space_position : VIEW_POSITION;
    float3 prev_view_space_position : PREV_VIEW_POSITION;
    
    float3 world_space_normal : NORMAL;
    float3 world_space_tangent  : TANGENT;
    float2 uv        : TEXCOORD;
};

[shader("vertex")]
VertexOutput vertex_shader(uint instance_id : SV_InstanceID, uint vertex_id : SV_VertexID)
{
    GeometryConstant constant = geometry_constant_buffer[geometry_constant_index];

    VertexOutput output;
    // float4 world_pos = mul(float4(input.local_space_position, 1.0f), constant.world_matrix);

    // output.sv_position = mul(world_pos, view_proj);

    // output.world_space_position = world_pos.xyz;

    // output.view_space_position = mul(world_pos, view_matrix).xyz;
    // output.prev_view_space_position = mul(world_pos, prev_view_matrix).xyz;

    // output.world_space_normal = normalize(mul(float4(input.local_space_normal, 1.0f), constant.inv_trans_world)).xyz;
    // output.world_space_tangent = normalize(mul(float4(input.local_space_tangent, 1.0f), constant.inv_trans_world)).xyz;
    // output.uv = input.uv;

    return output;
}

