#include "../common/sky.hlsl"
#include "../common/post_process.hlsl"

cbuffer gPassConstant : register(b0)
{
    float3 FrustumA; float pad0;
    float3 FrustumB; float pad1;
    float3 FrustumC; float pad2;
    float3 FrustumD; float pad3;
};


Texture2D<float3> gSkyLUT : register(t0);
SamplerState gSampler : register(s0);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float2 uv        : TEXCOORD;
};


float4 main(VertexOutput In) : SV_Target0
{
    float3 Dir = normalize(lerp(
        lerp(FrustumA, FrustumB, In.uv.x),
        lerp(FrustumC, FrustumD, In.uv.x),
        In.uv.y
    ));

    float3 SkyColor = gSkyLUT.Sample(gSampler, get_sky_uv(Dir));
    SkyColor = simple_post_process(In.uv, SkyColor);

    return float4(SkyColor, 1.0f);
}

