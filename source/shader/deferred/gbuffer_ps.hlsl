#include "../common/gbuffer.hlsl"
#include "../common/octahedral.hlsl"


cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;

    float4x4 view_matrix;
    float4x4 PrevViewMatrix;

    uint geometry_constant_index;
    uint3 pad;
};

SamplerState sampler0 : register(s0);

Texture2D<float4> base_color_texture : register(t0);
Texture2D<float3> normal_texture : register(t1);
Texture2D<float> metallic_texture : register(t2);
Texture2D<float> roughness_texture : register(t3);
Texture2D<float4> emissive_texture : register(t4);
Texture2D<float> occlusion_texture : register(t5);

StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t6);


struct VertexOutput
{
    float4 sv_position : SV_Position;

    float3 world_space_position : WORLD_POSITION;

    float3 view_space_position : VIEW_POSITION;
    float3 prev_view_space_position : PREV_VIEW_POSITION;

    float3 world_space_normal : NORMAL;
    float3 world_space_tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct PixelOutput
{
    float4 world_position_view_depth : SV_TARGET0;
    float4 world_space_normal : SV_TARGET1;
    float4 base_color : SV_TARGET2;
    float4 pbr : SV_TARGET3;
    float4 emmisive : SV_TARGET4;
    float4 view_space_velocity : SV_TARGET5;
};


PixelOutput main(VertexOutput input)
{
    GeometryConstant constant = geometry_constant_buffer[geometry_constant_index];

    PixelOutput output;
    output.world_position_view_depth = float4(input.world_space_position, input.view_space_position.z);

    float3 normal = calculate_normal(
        normal_texture.Sample(sampler0, input.uv).xyz,
        normalize(input.world_space_normal),
        normalize(input.world_space_tangent)
    );
    output.world_space_normal = float4(normal, 1.0f);

    output.base_color = base_color_texture.Sample(sampler0, input.uv) * constant.base_color;

    float metallic = metallic_texture.Sample(sampler0, input.uv);
    float roughness = roughness_texture.Sample(sampler0, input.uv);
    float occlusion = occlusion_texture.Sample(sampler0, input.uv).r * constant.occlusion;
    output.pbr = float4(metallic, roughness, occlusion, 1.0f);

    output.emmisive = emissive_texture.Sample(sampler0, input.uv) * constant.emissive;

    output.view_space_velocity = float4(input.prev_view_space_position - input.view_space_position, 1.0f);

    return output;
}