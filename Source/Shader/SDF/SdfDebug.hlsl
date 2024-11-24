#include "../Intersect.hlsli"


cbuffer gPassConstants : register(b0)
{
    float3 FrustumA;        uint dwMaxTraceSteps;
    float3 FrustumB;        float AbsThreshold;
    float3 FrustumC;        float fChunkSize;
    float3 FrustumD;        uint dwChunkNumPerAxis;
    float3 CameraPosition;  float fSceneGridSize;
    float3 SceneGridOrigin; float fMaxGIDistance;
    float fDefaultMarch;   float3 PAD;
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

    float3 p = o;
    float fInitStep;
    float3 SceneGridEnd = SceneGridOrigin + fSceneGridSize;
    float3 ClampInitPos = clamp(p, SceneGridOrigin, SceneGridEnd);
    if (length(ClampInitPos - p) > 0.0001f && IntersectRayBox(p, d, SceneGridOrigin, SceneGridEnd, fInitStep))
    {
        p += d * fInitStep;
    }

    float fInvChunkSize = 1.0f / fChunkSize;
    int3 ChunkIndex = floor((p - SceneGridOrigin) * fInvChunkSize);
    ChunkIndex = clamp(ChunkIndex, 0, dwChunkNumPerAxis);
    
    float3 SdfLower = SceneGridOrigin + ChunkIndex * fChunkSize;
    float3 SdfUpper = SdfLower + fChunkSize;

    uint ix = 0;
    for (; ix < dwMaxTraceSteps; ++ix)
    {
        float3 uvw = (p - SceneGridOrigin) / fSceneGridSize;
        if (any(saturate(uvw) != uvw)) { break; }

        float Sdf = gSdf.Sample(gSampler, uvw);

        // 若发现为空 chunk, 则加速前进.
        float Udf = abs(Sdf) < 0.00001f ? fDefaultMarch : Sdf;
        if (abs(Udf) <= AbsThreshold) break;

        p += Udf * d;
    }

    float Color = float(ix) / float(dwMaxTraceSteps - 1);
    Color = pow(Color, 1 / 2.2f);
    return float4(Color.xxx, 1.0f);
}