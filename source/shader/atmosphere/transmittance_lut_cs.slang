
// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1
// #define STEP_COUNT 1

#include "../common/atmosphere_properties.slang"
#include "../common/intersect.slang"

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(STEP_COUNT)

cbuffer gAtomsphereProperties : register(b0)
{
    AtmosphereProperties AP;
};

RWTexture2D<float4> transmittance_texture : register(u0);


[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_X, 1)]
void main(uint3 ThreadIndex : SV_DispatchThreadID)
{
    uint32_t dwWidth, dwHeight;
    transmittance_texture.GetDimensions(dwWidth, dwHeight);

    if (ThreadIndex.x >= dwWidth || ThreadIndex.y >= dwHeight) return;

    float theta = asin(lerp(-1.0f, 1.0f, (ThreadIndex.y + 0.5f) / dwHeight));      // [-π/2, π/2] θ 角度
    float height = lerp(0.0f, AP.atmosphere_radius - AP.planet_radius, (ThreadIndex.x + 0.5f) / dwWidth);    // 离地高度.

    float2 RayOri = float2(0.0f, height + AP.planet_radius);
    float2 RayDir = float2(cos(theta), sin(theta));      // 范围为右侧半圆.

    float fDistance = 0.0f;
    if (!IntersectRayCircle(RayOri, RayDir, AP.planet_radius, fDistance))
    {
        IntersectRayCircle(RayOri, RayDir, AP.atmosphere_radius, fDistance);
    }
    float2 RayEnd = RayOri + RayDir * fDistance;

    float3 TransmittanceSum = float3(0.0f, 0.0f, 0.0f);
    for (uint32_t ix = 0; ix < STEP_COUNT; ++ix)
    {
        float2 p = lerp(RayOri, RayEnd, (ix + 0.5f) / STEP_COUNT);
        float h = length(p) - AP.planet_radius;
        TransmittanceSum += get_transmittance(AP, h);
    }

    float dt = fDistance / STEP_COUNT;
    transmittance_texture[ThreadIndex.xy] = float4(exp(-TransmittanceSum * dt), 1.0f);

    // 指数衰减: N(t)=N_0 * e^(-λ*t).
}













#endif