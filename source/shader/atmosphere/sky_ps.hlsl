#include "../common/sky.hlsl"
#include "../common/post_process.hlsl"

cbuffer pass_constants : register(b0)
{
    float3 frustum_A; float pad0;
    float3 frustum_B; float pad1;
    float3 frustum_C; float pad2;
    float3 frustum_D; float pad3;
};


Texture2D<float3> sky_lut_texture : register(t0);
SamplerState sampler_ : register(s0);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float2 uv        : TEXCOORD;
};


float4 main(VertexOutput input) : SV_Target0
{
    float3 dir = normalize(lerp(
        lerp(frustum_A, frustum_B, input.uv.x),
        lerp(frustum_C, frustum_D, input.uv.x),
        input.uv.y
    ));

    float3 sky_color = sky_lut_texture.Sample(sampler_, get_sky_uv(dir));
    sky_color = simple_post_process(input.uv, sky_color);

    return float4(sky_color, 1.0f);
}

