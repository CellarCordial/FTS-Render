#ifndef SHADER_BRDF_HLSLI
#define SHADER_BRDF_HLSLI

#include "Constants.hlsli"

float D_TrowbridgeReitz_GGX(float3 Normal, float3 HalfVec, float fRoughness)
{
    float fRoughness2 = fRoughness * fRoughness;
    float fNDotH = max(dot(Normal, HalfVec), 0.0f);
    float fNDotH2 = fNDotH * fNDotH;

    float fNom = fRoughness2;
    float fDenom = fNDotH2 * (fRoughness2 - 1.0f) + 1.0f;
    fDenom = PI * fDenom * fDenom;

    return fNom / fDenom;
}

float G_Schlick_GGX(float3 Normal, float3 View, float k)
{
    float fNDotV = max(dot(Normal, View), 0.0f);
    float fNom = fNDotV;
    float fDenom = fNDotV * (1.0f - k) + k;

    return fNom / fDenom;
}


float G_Smith(float3 Normal, float3 View, float3 LightVec, float k)
{
    return G_Schlick_GGX(Normal, View, k) * G_Schlick_GGX(Normal, LightVec, k);
}

float3 F_FresnelSchlick(float3 HalfVec, float3 View, float3 F0)
{
    float fCosTheta = max(dot(HalfVec, View), 0.0f);
    return F0 + (1.0f - F0) * pow(clamp(1.0f - fCosTheta, 0.0f, 1.0f), 5.0f);
}

















#endif