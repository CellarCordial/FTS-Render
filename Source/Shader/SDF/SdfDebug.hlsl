#include "../SdfCommon.hlsli"

cbuffer gPassConstants : register(b0)
{
    float3 FrustumA;        float PAD0;
    float3 FrustumB;        float PAD1;
    float3 FrustumC;        float PAD2;
    float3 FrustumD;        float PAD3;
    float3 CameraPosition;  float PAD4;

    FGlobalSdfData GlobalSdfData;
    float2 PAD5;
};

Texture3D<float> gSdf : register(t0);
SamplerState gSampler : register(s0);


struct FVertexOutput
{
    float4 PositionH : SV_Position;
    float2 UV        : TEXCOORD;
};

FVertexOutput VS(uint dwVertexID : SV_VertexID)
{
    // Full screen quad.
    FVertexOutput Out;
    Out.UV = float2((dwVertexID << 1) & 2, dwVertexID & 2);
    Out.PositionH = float4(Out.UV * float2(2, -2) + float2(-1, 1), 0.5f, 1.0f);
    return Out;
}

float4 PS(FVertexOutput In) : SV_Target0
{
    float3 o = CameraPosition;
    float3 d = lerp(
        lerp(FrustumA, FrustumB, In.UV.x),
        lerp(FrustumC, FrustumD, In.UV.x),
        In.UV.y
    );

    FSdfHitData HitData = TraceGlobalSdf(o, d, GlobalSdfData, false, gSdf, gSampler);

    float Color = float(HitData.dwStepCount) / float(GlobalSdfData.dwMaxTraceSteps - 1);
    Color = pow(Color, 1 / 2.2f);
    return float4(Color.xxx, 1.0f);
}