// #define THREAD_GROUP_SIZE_X 0
// #define THREAD_GROUP_SIZE_Y 0

#include "../Intersect.hlsli"
#include "../Medium.hlsli"

cbuffer gAtomsphereProperties : register(b0)
{
    FAtmosphereProperties AP;
};


cbuffer gPassConstant : register(b1)
{
    float3 SunDir;          float fSunTheta;
    float3 FrustumA;        float fMaxDistance;
    float3 FrustumB;        int dwPerSliceMarchStepCount;
    float3 FrustumC;        float fAtmosEyeHeight;
    float3 FrustumD;        uint bEnableMultiScattering;
    float3 CameraPosiiton;  uint bEnableShadow;
    float fWorldScale;      float3 PAD;
    float4x4 ShadowViewProj;
};


Texture2D<float3> gMultiScatteringTexture : register(t0);
Texture2D<float3> gTransmittanceTexture : register(t1);
Texture2D<float> gShadowMap : register(t2);

SamplerState gMTSampler : register(s0);
SamplerState gShdowMapSampler : register(s1);

RWTexture3D<float4> gAerialPerspectiveLUT : register(u0);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)


float RelativeLuminance(float3 c)
{
    return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
}


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void CS(uint3 ThreadID : SV_DispatchThreadID)
{
    uint dwWidth, dwHeight, dwDepth;
    gAerialPerspectiveLUT.GetDimensions(dwWidth, dwHeight, dwDepth);

    if (ThreadID.x >= dwWidth || ThreadID.y >= dwHeight) return;

    float u = (ThreadID.x + 0.5f) / dwWidth;
    float v = (ThreadID.y + 0.5f) / dwHeight;

    float3 Ori = float3(0.0f, fAtmosEyeHeight, 0.0f);
    float3 Dir = normalize(lerp(
        lerp(FrustumA, FrustumB, u),
        lerp(FrustumC, FrustumD, u),
        v
    ));

    float fCosTheta = dot(-SunDir, Dir);

    float fDistance = 0.0f;
    float3 WorldOri = Ori + float3(0.0f, AP.fPlanetRadius, 0.0f);
    if (!IntersectRaySphere(WorldOri, Dir, AP.fPlanetRadius, fDistance))
    {
        IntersectRaySphere(WorldOri, Dir, AP.fAtmosphereRadius, fDistance);
    }

    float fSliceDepth = fMaxDistance / dwDepth;
    float fBegin = 0.0f, fEnd = min(0.5f * fSliceDepth, fDistance);

    float3 TransmittanceSum = float3(0.0f, 0.0f, 0.0f);
    float3 InScatterSum = float3(0.0f, 0.0f, 0.0f);

    float fRandom = frac(sin(dot(float2(u, v), float2(12.9898f, 78.233f) * 2.0f)) * 43758.5453f);

    for (uint z = 0; z < dwDepth; ++z)
    {
        float dt = (fEnd - fBegin) / dwPerSliceMarchStepCount;
        float t = fBegin;
        for (uint ix = 0; ix < dwPerSliceMarchStepCount; ++ix)
        {
            float fNextT = t + dt;
            float fMidT = lerp(t, fNextT, fRandom);

            float3 Pos = WorldOri + Dir * fMidT;
            float fHeight = length(Pos) - AP.fPlanetRadius;

            float3 Transmittance;
            float3 InScatter;
            GetScatterTransmittance(AP, fHeight, InScatter, Transmittance);

            float3 DeltaTransmittance = dt * Transmittance;
            float3 EyeTransmittance = exp(-TransmittanceSum - 0.5f * DeltaTransmittance);
            float2 UV = GetTransmittanceUV(AP, fHeight, fSunTheta);

            if (!IntersectRaySphere(Pos, -SunDir, AP.fPlanetRadius))
            {
                float3 ShadowPos = CameraPosiiton + Dir * fMidT / fWorldScale;
                float4 ShadowClip = mul(float4(ShadowPos, 1.0f), ShadowViewProj);
                float2 ShadowNDC = ShadowClip.xy / ShadowClip.w;
                float2 ShadowUV = 0.5f + float2(0.5f, -0.5f) * ShadowNDC;

                bool bInShdow = bEnableShadow;
                if (bEnableShadow && all(saturate(ShadowUV) == ShadowUV))
                {
                    float fRayZ = ShadowClip.z;
                    float fShadowMapZ = gShadowMap.SampleLevel(gShdowMapSampler, ShadowUV, 0);
                    bInShdow = fRayZ >= fShadowMapZ;
                }

                if (!bInShdow)
                {
                    float3 Phase = EstimatePhaseFunc(AP, fHeight, fCosTheta);
                    float3 SunTransmittance = gTransmittanceTexture.SampleLevel(gMTSampler, UV, 0);

                    InScatterSum += dt * (EyeTransmittance * InScatter * Phase) * SunTransmittance;
                }
            }

            if (bEnableMultiScattering)
            {
                float3 MultiScattering = gMultiScatteringTexture.SampleLevel(gMTSampler, UV, 0);
                InScatterSum += dt * EyeTransmittance * InScatter * MultiScattering;
            }

            TransmittanceSum += DeltaTransmittance;
            t = fNextT;
        }

        gAerialPerspectiveLUT[uint3(ThreadID.xy, z)] = float4(InScatterSum, RelativeLuminance(exp(-TransmittanceSum)));

        fBegin = fEnd;
        fEnd = min(fEnd + fSliceDepth, fDistance);
    }
}






#endif