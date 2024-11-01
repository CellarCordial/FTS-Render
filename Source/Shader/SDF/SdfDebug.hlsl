#include "Intersect.hlsli"


struct FPassConstants
{
    float3 FrustumA;        uint dwMaxTraceSteps;
    float3 FrustumB;        float AbsThreshold;
    float3 FrustumC;        float Pad0;
    float3 FrustumD;        float Pad1;
    float3 CameraPosition;  float Pad2;
    float3 SdfLower;        float Pad3;
    float3 SdfUpper;        float Pad4;
    float3 SdfExtent;       float Pad5;
};

ConstantBuffer<FPassConstants> gPassCB : register(b0);
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
    float3 o = gPassCB.CameraPosition;
    float3 d = lerp(
        lerp(gPassCB.FrustumA, gPassCB.FrustumB, In.UV.x),
        lerp(gPassCB.FrustumC, gPassCB.FrustumD, In.UV.x),
        In.UV.y
    );

    float fStep = 0.0f;
    if (!IntersectRayBox(o, d, gPassCB.SdfLower, gPassCB.SdfUpper, fStep)) return float4(0.0f, 0.0f, 0.0f, 1.0f);

    uint ix = 0;
    for (; ix < gPassCB.dwMaxTraceSteps; ++ix)
    {
        float3 p = o + fStep * d;
        float3 uvw = (p - gPassCB.SdfLower) / gPassCB.SdfExtent;
        if (any(saturate(uvw) != uvw)) break;

        float3 Sdf = gSdf.SampleLevel(gSampler, uvw, 0);
        float3 Udf = abs(Sdf);
        if (Udf <= gPassCB.AbsThreshold) break;

        fStep += Udf;
    }

    float Color = float(ix) / float(gPassCB.dwMaxTraceSteps - 1);
    Color = pow(Color, 1 / 2.2f);
    return float4(Color.xxx, 1.0f);
}