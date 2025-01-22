// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1
// #define DIRECTION_SAMPLE_COUNT 1

#include "../common/atmosphere_properties.slang"
#include "../common/intersect.slang"

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(DIRECTION_SAMPLE_COUNT)

cbuffer gAtomsphereProperties : register(b0)
{
    AtmosphereProperties AP;
};


cbuffer gPassConstant : register(b1)
{
    float3 SunIntensity;
    int32_t dwRayMarchStepCount;

    float3 GroundAlbedo;
    float pad;
};

Texture2D<float4> transmittance_texture : register(t0);
StructuredBuffer<float2> gDirSamples : register(t1);
RWTexture2D<float4> gMultiScatterTexture : register(u0);
SamplerState gSampler : register(s0);

float3 EstimateMultiScatter(float sun_theta, float height);
float3 UnitSphereDir(float r1, float r2);
void LightIntegate(float3 WorldOri, float3 WorldDir, float SunTheta, float3 SunDir, out float3 L2, out float3 F);

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 ThreadIndex : SV_DispatchThreadID)
{
    uint32_t dwWidth, dwHeight;
    gMultiScatterTexture.GetDimensions(dwWidth, dwHeight);

    if (ThreadIndex.x >= dwWidth || ThreadIndex.y >= dwHeight) return;

    float theta = asin(lerp(-1.0f, 1.0f, (ThreadIndex.y + 0.5f) / dwHeight));
    float height = lerp(0.0f, AP.atmosphere_radius - AP.planet_radius, (ThreadIndex.x + 0.5f) / dwWidth);

    gMultiScatterTexture[ThreadIndex.xy] = float4(EstimateMultiScatter(theta, height), 1.0f);
}

float3 EstimateMultiScatter(float theta, float height)
{
    float3 WorldOri = float3(0.0f, AP.planet_radius + height, 0.0f);
    float3 SunDir = float3(cos(theta), sin(theta), 0.0f);

    float3 L2Sum = float3(0.0f, 0.0f, 0.0f);
    float3 FSum = float3(0.0f, 0.0f, 0.0f);
    for (uint32_t ix = 0; ix < DIRECTION_SAMPLE_COUNT; ++ix)
    {
        float2 DirSample = gDirSamples[ix];
        float3 WorldDir = UnitSphereDir(DirSample.x, DirSample.y);
        
        float3 L2, F;
        LightIntegate(WorldOri, WorldDir, theta, SunDir, L2, F);

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


void LightIntegate(float3 WorldOri, float3 WorldDir, float sun_theta, float3 SunDir, out float3 L2, out float3 F)
{
    float fDistance = 0.0f;
    bool bGroundIntersect = intersect_ray_sphere(WorldOri, WorldDir, AP.planet_radius, fDistance);
    if (!bGroundIntersect)
    {
        intersect_ray_sphere(WorldOri, WorldDir, AP.atmosphere_radius, fDistance);
    }

    float dt = fDistance / dwRayMarchStepCount;

    float3 fTransmittanceSum = float3(0.0f, 0.0f, 0.0f);
    float3 L2Sum = fTransmittanceSum;
    float3 FSum = fTransmittanceSum;

    float theta = dot(WorldDir, SunDir);
    float t = 0.0f;
    for (uint32_t ix = 0; ix < dwRayMarchStepCount; ++ix)
    {
        float fMidT = t + 0.5f * dt;
        t += dt;

        float3 p = WorldOri + WorldDir * fMidT;
        float h = length(p) - AP.planet_radius;

        float3 scatter, transmittance;
        get_scatter_transmittance(AP, h, scatter, transmittance);

        float3 DeltaTransmittance = dt * transmittance;
        float3 TotalTransmittance = exp(-fTransmittanceSum - 0.5f * DeltaTransmittance);

        if (!intersect_ray_sphere(p, SunDir, AP.planet_radius))
        {
            float3 PhaseFunc = estimate_phase_func(AP, h, theta);
            float2 uv = get_transmittance_uv(AP, h, sun_theta);
            float4 SunTransmittance = transmittance_texture.SampleLevel(gSampler, uv, 0);

            L2Sum += dt * SunTransmittance.xyz * (TotalTransmittance * scatter * PhaseFunc) * SunIntensity;
        }
        FSum += dt * TotalTransmittance * scatter;
        fTransmittanceSum += DeltaTransmittance;
    }

    // 地面反射.
    if (bGroundIntersect)
    {
        float3 transmittance = exp(-fTransmittanceSum);
        float2 uv = get_transmittance_uv(AP, 0.0f, sun_theta);
        float4 SunTransmittance = transmittance_texture.SampleLevel(gSampler, uv, 0);

        L2Sum += transmittance * SunTransmittance.xyz * max(0.0f, SunDir.y) * SunIntensity * (GroundAlbedo / PI);
    }

    L2 = L2Sum;
    F = FSum;
}




#endif







