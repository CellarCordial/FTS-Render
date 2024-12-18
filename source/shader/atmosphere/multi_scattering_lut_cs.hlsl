// #define THREAD_GROUP_SIZE_X 0
// #define THREAD_GROUP_SIZE_Y 0
// #define DIRECTION_SAMPLE_COUNT 0

#include "../Intersect.hlsli"
#include "../Medium.hlsli"

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(DIRECTION_SAMPLE_COUNT)

cbuffer gAtomsphereProperties : register(b0)
{
    FAtmosphereProperties AP;
};


cbuffer gPassConstant : register(b1)
{
    float3 SunIntensity;
    int dwRayMarchStepCount;

    float3 GroundAlbedo;
    float PAD;
};

Texture2D<float4> gTransmittanceTexture : register(t0);
StructuredBuffer<float2> gDirSamples : register(t1);
RWTexture2D<float4> gMultiScatterTexture : register(u0);
SamplerState gSampler : register(s0);

float3 EstimateMultiScatter(float fSunTheta, float fHeight);
float3 UnitSphereDir(float r1, float r2);
void LightIntegate(float3 WorldOri, float3 WorldDir, float SunTheta, float3 SunDir, out float3 L2, out float3 F);

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void compute_shader(uint3 ThreadIndex : SV_DispatchThreadID)
{
    uint dwWidth, dwHeight;
    gMultiScatterTexture.GetDimensions(dwWidth, dwHeight);

    if (ThreadIndex.x >= dwWidth || ThreadIndex.y >= dwHeight) return;

    float fTheta = asin(lerp(-1.0f, 1.0f, (ThreadIndex.y + 0.5f) / dwHeight));
    float fHeight = lerp(0.0f, AP.fAtmosphereRadius - AP.fPlanetRadius, (ThreadIndex.x + 0.5f) / dwWidth);

    gMultiScatterTexture[ThreadIndex.xy] = float4(EstimateMultiScatter(fTheta, fHeight), 1.0f);
}

float3 EstimateMultiScatter(float fTheta, float fHeight)
{
    float3 WorldOri = float3(0.0f, AP.fPlanetRadius + fHeight, 0.0f);
    float3 SunDir = float3(cos(fTheta), sin(fTheta), 0.0f);

    float3 L2Sum = float3(0.0f, 0.0f, 0.0f);
    float3 FSum = float3(0.0f, 0.0f, 0.0f);
    for (uint ix = 0; ix < DIRECTION_SAMPLE_COUNT; ++ix)
    {
        float2 DirSample = gDirSamples[ix];
        float3 WorldDir = UnitSphereDir(DirSample.x, DirSample.y);
        
        float3 L2, F;
        LightIntegate(WorldOri, WorldDir, fTheta, SunDir, L2, F);

        L2Sum += L2;
        FSum += F;
    }

    // 平均.
    float3 L2 = L2Sum / DIRECTION_SAMPLE_COUNT;
    float3 F = FSum / DIRECTION_SAMPLE_COUNT;

    return L2 / ( 1.0f - F);
}

// u1 控制 Dir 的 z 值, u2 控制 Dir 投影在 xy 平面的与 x 轴的角度.
float3 UnitSphereDir(float u1, float u2)
{
    float z = 1.0f - 2.0f * u1;     // [0, 1] -> [-1, 1].
    float r = sqrt(max(0.0f, 1.0f - z * z));    // r^2 + z^2 = 1. r 为 Dir 投影 xy 平面的半径长度.
    float phi = 2.0f * PI * u2;     // [0, 2PI].
    return float3(r * cos(phi), r * sin(phi), z);
}


void LightIntegate(float3 WorldOri, float3 WorldDir, float fSunTheta, float3 SunDir, out float3 L2, out float3 F)
{
    float fDistance = 0.0f;
    bool bGroundIntersect = IntersectRaySphere(WorldOri, WorldDir, AP.fPlanetRadius, fDistance);
    if (!bGroundIntersect)
    {
        IntersectRaySphere(WorldOri, WorldDir, AP.fAtmosphereRadius, fDistance);
    }

    float dt = fDistance / dwRayMarchStepCount;

    float3 fTransmittanceSum = float3(0.0f, 0.0f, 0.0f);
    float3 L2Sum = fTransmittanceSum;
    float3 FSum = fTransmittanceSum;

    float fTheta = dot(WorldDir, SunDir);
    float t = 0.0f;
    for (uint ix = 0; ix < dwRayMarchStepCount; ++ix)
    {
        float fMidT = t + 0.5f * dt;
        t += dt;

        float3 p = WorldOri + WorldDir * fMidT;
        float h = length(p) - AP.fPlanetRadius;

        float3 Scatter, Transmittance;
        GetScatterTransmittance(AP, h, Scatter, Transmittance);

        float3 DeltaTransmittance = dt * Transmittance;
        float3 TotalTransmittance = exp(-fTransmittanceSum - 0.5f * DeltaTransmittance);

        if (!IntersectRaySphere(p, SunDir, AP.fPlanetRadius))
        {
            float3 PhaseFunc = EstimatePhaseFunc(AP, h, fTheta);
            float2 UV = GetTransmittanceUV(AP, h, fSunTheta);
            float4 SunTransmittance = gTransmittanceTexture.SampleLevel(gSampler, UV, 0);

            L2Sum += dt * SunTransmittance.xyz * (TotalTransmittance * Scatter * PhaseFunc) * SunIntensity;
        }
        FSum += dt * TotalTransmittance * Scatter;
        fTransmittanceSum += DeltaTransmittance;
    }

    // 地面反射.
    if (bGroundIntersect)
    {
        float3 Transmittance = exp(-fTransmittanceSum);
        float2 UV = GetTransmittanceUV(AP, 0.0f, fSunTheta);
        float4 SunTransmittance = gTransmittanceTexture.SampleLevel(gSampler, UV, 0);

        L2Sum += Transmittance * SunTransmittance.xyz * max(0.0f, SunDir.y) * SunIntensity * (GroundAlbedo / PI);
    }

    L2 = L2Sum;
    F = FSum;
}




#endif







