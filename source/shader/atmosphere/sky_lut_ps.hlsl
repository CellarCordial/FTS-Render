#include "../common/atmosphere_properties.hlsl"
#include "../common/intersect.hlsl"

cbuffer gAtomsphereProperties : register(b0)
{
    AtmosphereProperties AP;
};

cbuffer gPassConstant : register(b1)
{
    float3 CameraPosition;  
    int dwMarchStepCount;

    float3 SunDir;
    uint bEnableMultiScattering;

    float3 SunIntensity;
    float pad;
};

Texture2D<float3> gmulti_scattering_texture : register(t0);
Texture2D<float3> transmittance_texture : register(t1);
SamplerState gMTSampler : register(s0);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float2 uv        : TEXCOORD;
};

void RayMarching(float fPhaseU, float3 o, float3 d, float t, float dt, inout float3 TransmittanceSum, inout float3 InScatteringSum)
{
    float fMidT = t + 0.5f * dt;

    float3 Pos = float3(0.0f, o.y + AP.planet_radius, 0.0f) + d * fMidT;
    float height = length(Pos) - AP.planet_radius;

    float3 transmittance;
    float3 InScatter;
    get_scatter_transmittance(AP, height, InScatter, transmittance);

    float3 DeltaTransmittance = dt * transmittance;
    float3 EyeTransmittance = exp(-TransmittanceSum - 0.5f * DeltaTransmittance);

    float sun_theta = PI / 2 - acos(dot(-SunDir, normalize(Pos)));

    float2 uv = get_transmittance_uv(AP, height, sun_theta);

    if (!intersect_ray_sphere(Pos, -SunDir, AP.planet_radius))
    {
        float3 Phase = estimate_phase_func(AP, height, fPhaseU);
        float3 SunTransmittance = transmittance_texture.SampleLevel(gMTSampler, uv, 0);
        InScatteringSum += dt * (EyeTransmittance * InScatter * Phase) * SunTransmittance;
    }

    if (bool(bEnableMultiScattering))
    {
        float3 MultiScattering = gmulti_scattering_texture.SampleLevel(gMTSampler, uv, 0);
        InScatteringSum += dt * EyeTransmittance * InScatter * MultiScattering;
    }

    TransmittanceSum += DeltaTransmittance;
}


float4 main(VertexOutput In) : SV_Target0
{
    float fPhi = 2 * PI * In.uv.x;   // [0, 2PI] 在 x-z 平面的投影与 x 轴的夹角.
    float vm = 2 * In.uv.y - 1;
    float theta = sign(vm) * (PI / 2) * vm * vm;    // [-PI / 2, PI / 2] 与 x-z 平面的夹角, 平方运算 (vm * vm) 的目的是让垂直分布更接近半球面投影.
    float fSinTheta = sin(theta); 
    float fCosTheta = cos(theta);

    float3 Ori = CameraPosition;
    float3 Dir = float3(fCosTheta *cos(fPhi), fSinTheta, fCosTheta * sin(fPhi));

    float2 WorldOri = float2(0, Ori.y + AP.planet_radius);
    float2 WorldDir = float2(fCosTheta, fSinTheta);        // 仰角, 垂直于地面的平面上的仰角.

    float fDistance = 0.0f;
    if (!IntersectRayCircle(WorldOri, WorldDir, AP.planet_radius, fDistance))
    {
        IntersectRayCircle(WorldOri, WorldDir, AP.atmosphere_radius, fDistance);
    }

    float fPhaseU = dot(-SunDir, Dir);

    float t = 0;
    float3 TransmittanceSum = float3(0.0f, 0.0f, 0.0f);
    float3 InScatterSum = float3(0.0f, 0.0f, 0.0f);

    float dt = fDistance / dwMarchStepCount;
    for (uint ix = 0; ix < dwMarchStepCount; ++ix)
    {
        RayMarching(fPhaseU, Ori, Dir, t, dt, TransmittanceSum, InScatterSum);
        t += dt;
    }

    return float4(InScatterSum * SunIntensity, 1.0f);
}