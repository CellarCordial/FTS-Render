#include "../material.slang"
#include "../octahedral.slang"

cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;

    float4x4 view_matrix;
    float4x4 prev_view_matrix;
};

cbuffer geometry_constants : register(b1)
{
    float4x4 world_matrix;
    float4x4 inv_trans_world;

    uint material_index;
    uint3 pad;
};

struct VertexInput
{
    float3 local_space_position : POSITION;
    float3 local_space_normal : NORMAL;
    float4 local_space_tangent  : TANGENT;
    float2 uv : TEXCOORD;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;

    float3 world_space_position : WORLD_POSITION;

    float3 view_space_position : VIEW_POSITION;
    float3 prev_view_space_position : PREV_VIEW_POSITION;
    
    float3 world_space_normal : NORMAL;
    float4 world_space_tangent  : TANGENT;
    float2 uv        : TEXCOORD;
};

[shader("vertex")]
VertexOutput vertex_shader(VertexInput input)
{
    VertexOutput out;
    float4 world_pos = mul(float4(input.local_space_position, 1.0f), world_matrix);

    out.sv_position = mul(world_pos, view_proj);

    out.world_space_position = world_pos.xyz;

    out.view_space_position = mul(world_pos, view_matrix).xyz;
    out.prev_view_space_position = mul(world_pos, prev_view_matrix).xyz;

    out.world_space_normal = normalize(mul(float4(input.local_space_normal, 1.0f), inv_trans_world)).xyz;
    out.world_space_tangent = normalize(mul(input.local_space_tangent, inv_trans_world));
    out.uv = input.uv;

    return out;
}

