
// #define THREAD_GROUP_SIZE_X 0
// #define THREAD_GROUP_SIZE_Y 0
// #define STEP_COUNT 0

#include "../Intersect.hlsli"
#include "../Medium.hlsli"


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(STEP_COUNT)

cbuffer gAtomsphereProperties : register(b0)
{
    FAtmosphereProperties AP;
};

RWTexture2D<float4> gTransmittanceTexture : register(u0);


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_X, 1)]
void CS(uint3 ThreadIndex : SV_DispatchThreadID)
{
    uint dwWidth, dwHeight;
    gTransmittanceTexture.GetDimensions(dwWidth, dwHeight);

    if (ThreadIndex.x >= dwWidth || ThreadIndex.y >= dwHeight) return;

    float fTheta = asin(lerp(-1.0f, 1.0f, (ThreadIndex.y + 0.5f) / dwHeight));      // [-π/2, π/2] θ 角度
    float fHeight = lerp(0.0f, AP.fAtmosphereRadius - AP.fPlanetRadius, (ThreadIndex.x + 0.5f) / dwWidth);    // 离地高度.

    float2 RayOri = float2(0.0f, fHeight + AP.fPlanetRadius);
    float2 RayDir = float2(cos(fTheta), sin(fTheta));      // 范围为右侧半圆.

    float fDistance = 0.0f;
    if (!IntersectRayCircle(RayOri, RayDir, AP.fPlanetRadius, fDistance))
    {
        IntersectRayCircle(RayOri, RayDir, AP.fAtmosphereRadius, fDistance);
    }
    float2 RayEnd = RayOri + RayDir * fDistance;

    float3 TransmittanceSum = float3(0.0f, 0.0f, 0.0f);
    for (uint ix = 0; ix < STEP_COUNT; ++ix)
    {
        float2 p = lerp(RayOri, RayEnd, (ix + 0.5f) / STEP_COUNT);
        float h = length(p) - AP.fPlanetRadius;
        TransmittanceSum += GetTransmittance(AP, h);
    }

    float dt = fDistance / STEP_COUNT;
    gTransmittanceTexture[ThreadIndex.xy] = float4(exp(-TransmittanceSum * dt), 1.0f);

    // 指数衰减: N(t)=N_0 * e^(-λ*t).
}













#endif