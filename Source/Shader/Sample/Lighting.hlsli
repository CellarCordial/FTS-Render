#ifndef SHADER_LIGHTING_HLSLI
#define SHADER_LIGHTING_HLSLI

#include "BRDF.hlsli"
#include "Material.hlsli"

struct FPointLight
{
    float4 Color;
    float3 Position;
    float fIntensity;
    float fRadius;
    float fFallOffStart;
    float fFallOffEnd;
};

struct FDirectionalLight
{
    float4 Color;
    float3 Direction;
    float fIntensity;
};

float CalcAttenuation(float fDistance, float fFallOffStart, float fFallOffEnd)
{
    return saturate((fFallOffEnd - fDistance) / (fFallOffEnd - fFallOffStart));
}

// LightRadiance = LightColor * LightAttenuation.
float3 MetallicPBRLighting(float3 LightRadiance, float3 LightVec, float3 View, float3 Normal, FMaterial Material)
{
    float3 N = normalize(Normal);
    float3 V = normalize(View);
    float3 L = normalize(LightVec);
    float3 H = normalize(V + L);

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, Material.fDiffuse.rgb, Material.fMetallic);

    float D = D_TrowbridgeReitz_GGX(N, H, Material.fRoughness);
    float G = G_Smith(N, V, L, Material.fRoughness);
    float3 F = F_FresnelSchlick(H, V, F0);

    float3 Nom = D * G * F;
    float fDenom = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.001f;
    float3 Specular = Nom / fDenom;

    float3 Ks = F;
    float3 Kd = 1.0f - Ks;
    Kd *= 1.0f - Material.fMetallic;

    return (Kd * Material.fDiffuse.xyz / PI + Specular) * LightRadiance * max(dot(N, L), 0.0f);
}









#endif