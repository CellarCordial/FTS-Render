#include "../Constants.hlsli"
#include "../SkyCommon.hlsli"
#include "../PostProcess.hlsli"

cbuffer gPassConstant : register(b0)
{
    float3 FrustumA; float PAD0;
    float3 FrustumB; float PAD1;
    float3 FrustumC; float PAD2;
    float3 FrustumD; float PAD3;
};


Texture2D<float3> gSkyLUT : register(t0);
SamplerState gSampler : register(s0);

struct FVertexOutput
{
    float4 PositionH : SV_Position;
    float2 UV        : TEXCOORD;
};

FVertexOutput vertex_shader(uint dwVertexID : SV_VertexID)
{
    // Full screen quad.
    FVertexOutput Out;
    Out.UV = float2((dwVertexID << 1) & 2, dwVertexID & 2);
    Out.PositionH = float4(Out.UV * float2(2, -2) + float2(-1, 1), 0.5f, 1.0f);
    return Out;
}


float4 pixel_shader(FVertexOutput In) : SV_Target0
{
    float3 Dir = normalize(lerp(
        lerp(FrustumA, FrustumB, In.UV.x),
        lerp(FrustumC, FrustumD, In.UV.x),
        In.UV.y
    ));

    float3 SkyColor = gSkyLUT.Sample(gSampler, GetSkyUV(Dir));
    SkyColor = PostProcess(In.UV, SkyColor);

    return float4(SkyColor, 1.0f);
}

