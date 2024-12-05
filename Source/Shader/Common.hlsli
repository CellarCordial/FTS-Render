#ifndef SHADER_COMMON_HLSLI
#define SHADER_COMMON_HLSLI

#include "Constants.hlsli"


float3 GetWorldPostionFromDepthNDC(float2 UV, float fDepthNDC, float4x4 InvViewProj)
{
    float2 ScreenPos = UV * 2.0f - 1.0f;
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




float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// 低差异序列.
float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}  


#endif