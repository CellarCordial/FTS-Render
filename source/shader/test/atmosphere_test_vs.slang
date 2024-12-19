#include "../atmosphere_properties.slang"

cbuffer pass_constant0 : register(b0)
{
    float4x4 view_proj;
    float4x4 world_matrix;
};


struct VertexInput
{
    float3 local_space_position : POSITION;
    float3 local_space_normal   : NORMAL;
    float4 local_space_tangent  : TANGENT;
    float2 uv        : TEXCOORD;
};

struct VertexOutput
{
    float4 sv_position    : SV_POSITION;
    float4 screen_space_position    : SCREEN_POSITION;
    float3 world_space_position    : POSITION;
    float3 world_space_normal      : NORMAL;
};

[shader("vertex")]
VertexOutput vertex_shader(VertexInput In)
{
    VertexOutput output;
    float4 world_space_position = mul(float4(In.local_space_position, 1.0f), world_matrix);
    output.sv_position = mul(world_space_position, view_proj);
    output.screen_space_position = output.sv_position;
    output.world_space_position = world_space_position.xyz;
    output.world_space_normal = normalize(mul(float4(In.local_space_normal, 1.0f), world_matrix)).xyz;

    return output;
}

