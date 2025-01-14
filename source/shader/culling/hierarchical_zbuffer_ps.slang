#include "../math_helper.slang"

cbuffer pass_constants : register(b0)
{
    uint2 client_resolution;
    uint32_t hzb_resolution;
    uint32_t last_mip_level;
};

Texture2D<float> hierarchical_zbuffer : register(t0);
SamplerState linear_clamp_sampler : register(s0);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float2 uv : TEXCOORD;
};

[shader("pixel")]
float pixel_shader(VertexOutput input) : SV_Target0
{
    uint2 pos = uint2(input.sv_position.xy);
    if (last_mip_level == 0) pos = pos / hzb_resolution * client_resolution;
    else pos *= 2;

    float offset = 1.0f / hzb_resolution;
    float2 uv = (pos + 0.5f) / hzb_resolution;
    float z0 = hierarchical_zbuffer.Sample(linear_clamp_sampler, uv).r;
    float z1 = hierarchical_zbuffer.Sample(linear_clamp_sampler, uv + float2(offset, 0)).r;
    float z2 = hierarchical_zbuffer.Sample(linear_clamp_sampler, uv + float2(offset, offset)).r;
    float z3 = hierarchical_zbuffer.Sample(linear_clamp_sampler, uv + float2(0, offset)).r;

    return min4(z0, z1, z2, z3);
}