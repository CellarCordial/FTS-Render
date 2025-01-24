#include "../common/gbuffer.slang"

Texture2D<float4> base_color_texture : register(t0);
Texture2D<float3> normal_texture : register(t1);
Texture2D<float4> pbr_texture : register(t2);
Texture2D<float4> emissive_texture : register(t3);

SamplerState linear_clamp_sampler : register(s0);


struct VertexOutput
{
    float4 sv_position : SV_Position;

    float3 world_space_normal : NORMAL;
    float3 world_space_tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct PixelOutput
{
    float4 base_color : SV_Target0;
    float3 normal : SV_Target1;
    float4 pbr : SV_Target2;
    float4 emissive : SV_Target3;
    float3 light_cache : SV_Target4;
};

[shader("pixel")]
PixelOutput main(VertexOutput input)
{
    PixelOutput output;
    output.light_cache = float3(0.0f, 0.0f, 0.0f);
    output.base_color = base_color_texture.Sample(linear_clamp_sampler, input.uv);
    output.pbr = pbr_texture.Sample(linear_clamp_sampler, input.uv);
    output.normal = calculate_normal(
        normal_texture.Sample(linear_clamp_sampler, input.uv).xyz,
        normalize(input.world_space_normal),
        normalize(input.world_space_tangent)
    );
    output.emissive = emissive_texture.Sample(linear_clamp_sampler, input.uv);

    return output;
}