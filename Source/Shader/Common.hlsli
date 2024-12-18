#ifndef SHADER_COMMON_HLSLI
#define SHADER_COMMON_HLSLI

#include "Constants.hlsli"


float3 GetWorldPostionFromDepthNDC(float2 uv, float fDepthNDC, float4x4 InvViewProj)
{
    float2 ScreenPos = uv * 2.0f - 1.0f;
    float3 PositionNDC = float3(ScreenPos, fDepthNDC);
    float4 WorldPos = mul(float4(PositionNDC, 1.0f), InvViewProj);
    WorldPos = WorldPos / WorldPos.w; 
    return WorldPos.xyz;
}


float3 SphericalFibonacci(uint ix, uint dwTotalNum)
{
    // Theta 是方向与 z 轴的夹角.
    // Phi 是方向在 x-y 平面的投影与 x 轴的夹角.

    // 黄金比例: ((sqrt(5.0f) + 1.0f) * 0.5f) 再减去 1.0f.
    float Phi = sqrt(5.0f) * 0.5f - 0.5;
    Phi = 2.0f * PI * frac(ix * Phi);

    float fThetaCos = ((float(ix) + 0.5f) / float(dwTotalNum)) * 2.0f - 1.0f;
    float fThetaSin = sqrt(1.0f - fThetaCos * fThetaCos);
    return float3(fThetaSin * cos(Phi), fThetaSin * sin(Phi), fThetaCos);
}


float3 CalcNormal(float3 TextureNormal, float3 VertexNormal, float4 VertexTangent)
{
    float3 UnpackedNormal = TextureNormal * 2.0f - 1.0f;
    float3 N = VertexNormal;
    float3 T = normalize(VertexTangent.xyz - N * dot(VertexTangent.xyz, N));
    float3 B = cross(N, T) * VertexTangent.w;
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(UnpackedNormal, TBN));
}





#endif