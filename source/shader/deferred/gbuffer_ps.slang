#include "../octahedral.slang"
#include "../gbuffer.slang"

cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;

    float4x4 view_matrix;
    float4x4 PrevViewMatrix;

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

SamplerState sampler0 : register(s0);

Texture2D diffuse_texture           : register(t0);
Texture2D normal_texture            : register(t1);
Texture2D emissive_texture          : register(t2);
Texture2D occlusion_texture         : register(t3);
Texture2D metallic_roughness_texture : register(t4);
StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t5);

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

[shader("pixel")]
PixelOutput pixel_shader(VertexOutput input)
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

    output.base_color = diffuse_texture.Sample(sampler0, input.uv) * constant.diffuse;

    float4 metallic_roughness = metallic_roughness_texture.Sample(sampler0, input.uv);
    metallic_roughness.r *= constant.metallic;
    metallic_roughness.g *= constant.roughness;
    float fOcclusion = occlusion_texture.Sample(sampler0, input.uv).r * constant.occlusion;
    output.pbr = float4(metallic_roughness.rg, fOcclusion, 1.0f);

    output.emmisive = emissive_texture.Sample(sampler0, input.uv) * constant.emissive;

    output.view_space_velocity = float4(input.prev_view_space_position - input.view_space_position, 1.0f);

    return output;
}