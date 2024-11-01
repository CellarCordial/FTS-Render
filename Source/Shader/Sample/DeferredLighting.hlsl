#include "Lighting.hlsli"
#include "Intersect.hlsli"


struct FPassConstants
{
    uint dwPointLightNum; uint dwDirectionalLightNum; uint2 RenderTargetResolution;
    float3 CameraPosition;  uint dwMaxTraceStep;
    float3 SdfLower;        float fShadowSharp;
    float3 SdfUpper;        float fAbsThreshold;
    float3 SdfExtent;       float fDirectionalRayOriOffset;
};

ConstantBuffer<FPassConstants> gPassCB : register(b0);

Texture2D gDiffuse                         : register(t0);
Texture2D gEmissive                        : register(t1);
Texture2D gMetallicRoughnessOcclusionDepth : register(t2);
Texture2D gNormal                          : register(t3);
Texture2D gPosition                        : register(t4);

StructuredBuffer<FPointLight> gPointLights : register(t5);
StructuredBuffer<FDirectionalLight> gDirectionalLights : register(t6);

Texture3D<float> gSdf                      : register(t7);

SamplerState gSampler : register(s0);

RWTexture2D<float4> gRenderTarget : register(u0);


float SoftShadow(float3 o, float3 d)
{
    float fStep = 0.0f;
    if (!IntersectRayBox(o, d, gPassCB.SdfLower, gPassCB.SdfUpper, fStep)) return 1.0f;

    float fResult = 1.0f;
    float ph = 1e20;

    for (uint ix = 0; ix < gPassCB.dwMaxTraceStep; ++ix)
    {
        float3 p = o + fStep * d;
        float3 uvw = (p - gPassCB.SdfLower) / gPassCB.SdfExtent;
        float udf = abs(gSdf.SampleLevel(gSampler, uvw, 0));
        if (udf < gPassCB.fAbsThreshold) return 0.0f;

        float y = udf * udf / (2.0f * ph);
        float m = sqrt(udf * udf - y * y);
        fResult = min(fResult, gPassCB.fShadowSharp * m / max(0.0f, fStep - y));

        ph = udf;
        fStep += udf;
    }

    return fResult;
}



[numthreads(GROUP_THREAD_NUM_X, GROUP_THREAD_NUM_Y, 1)]
void CS(uint3 ThreadID : SV_DispatchThreadID)
{
    float2 UV = (float2(ThreadID.xy) + 0.5f) / float2(gPassCB.RenderTargetResolution);

    float3 Normal = normalize(gNormal.SampleLevel(gSampler, UV, 0).xyz);
    float3 Position = gPosition.SampleLevel(gSampler, UV, 0).xyz;
    float4 MROD = gMetallicRoughnessOcclusionDepth.Sample(gSampler, UV);;

    FMaterial Material;
    Material.fDiffuse = gDiffuse.SampleLevel(gSampler, UV, 0);
    Material.fEmissive = gEmissive.SampleLevel(gSampler, UV, 0);
    Material.fMetallic = MROD.r;
    Material.fRoughness = MROD.g;
    Material.fOcclusion = MROD.b;

    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    for (uint ix = 0; ix < gPassCB.dwPointLightNum; ++ix)
    {
        FPointLight Light = gPointLights[ix];
        float LightAttenuation = CalcAttenuation(length(Position - Light.Position), Light.fFallOffStart, Light.fFallOffEnd);
        float3 LightRadiance = Light.Color.xyz * LightAttenuation * Light.fIntensity;

        float3 LightVec = normalize(Light.Position - Position);
        float fShadowFactor = SoftShadow(Light.Position, -LightVec);

        float3 View = normalize(gPassCB.CameraPosition - Position);
        Lo += MetallicPBRLighting(LightRadiance, LightVec, View, Normal, Material) * fShadowFactor;
    }
    for (uint ij = 0; ij < gPassCB.dwDirectionalLightNum; ++ij)
    {
        FDirectionalLight Light = gDirectionalLights[ij];
        float3 LightRadiance = Light.Color.xyz * Light.fIntensity;

        float3 LightFakePosition = Position + gPassCB.fDirectionalRayOriOffset * Normal;
        float fShadowFactor = SoftShadow(LightFakePosition, Light.Direction);

        float3 LightVec = normalize(-Light.Direction);
        float3 View = normalize(gPassCB.CameraPosition - Position);
        Lo += MetallicPBRLighting(LightRadiance, LightVec, View, Normal, Material) * fShadowFactor;
    }

    float3 Ambient = 0.03f * Material.fDiffuse.xyz * Material.fOcclusion;
    float3 Color = Ambient + Lo;

    Color = Color / (Color + 1.0f);
    Color = pow(Color, 1.0f / 2.2f);

    gRenderTarget[ThreadID.xy] = float4(Color, 1.0f);
}