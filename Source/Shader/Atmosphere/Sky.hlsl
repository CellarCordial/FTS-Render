#include "../Constants.hlsli"

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
    float3 Dir = normalize(lerp(
        lerp(FrustumA, FrustumB, In.UV.x),
        lerp(FrustumC, FrustumD, In.UV.x),
        In.UV.y
    ));

    float fPhi = atan2(Dir.z, Dir.x);   // [-π, π]
    float fTheta = asin(Dir.y);         // [-π/2, π/2]
    float u = fPhi / (2.0f * PI);
    float v = 0.5f + 0.5f * sign(fTheta) * sqrt(abs(fTheta) / (PI / 2.0f));
    // 对归一化结果取平方根，能够调整输出值的曲线，使得小角度（接近 0）的变化更加平滑，而大角度（接近 π/2）的变化较快.

    float3 SkyColor = gSkyLUT.Sample(gSampler, float2(u, v));
    SkyColor = PostProcess(In.UV, SkyColor);

    return float4(SkyColor, 1.0f);
}

