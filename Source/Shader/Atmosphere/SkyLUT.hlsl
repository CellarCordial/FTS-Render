#include "../Medium.hlsli"
#include "../Intersect.hlsli"

cbuffer gAtomsphereProperties : register(b0)
{
    FAtmosphereProperties AP;
};

cbuffer gPassConstant : register(b1)
{
    float3 CameraPosition;  
    int dwMarchStepCount;

    float3 SunDir;
    uint bEnableMultiScattering;

    float3 SunIntensity;
    float PAD;
};

Texture2D<float3> gMultiScatteringTexture : register(t0);
Texture2D<float3> gTransmittanceTexture : register(t1);
SamplerState gMTSampler : register(s0);

struct FVertexOutput
{
    float4 PositionH : SV_Position;
    float2 UV        : TEXCOORD;
};

FVertexOutput VS(uint dwVertexID : SV_VertexID)
{
    // Full screen quad.
    FVertexOutput Out;
    Out.UV = float2((dwVertexID << 1) & 2, dwVertexID & 2);
    Out.PositionH = float4(Out.UV * float2(2, -2) + float2(-1, 1), 0.5f, 1.0f);
    return Out;
}


void RayMarching(float fPhaseU, float3 o, float3 d, float t, float dt, inout float3 TransmittanceSum, inout float3 InScatteringSum)
{
    float fMidT = t + 0.5f * dt;

    float3 Pos = float3(0.0f, o.y + AP.fPlanetRadius, 0.0f) + d * fMidT;
    float fHeight = length(Pos) - AP.fPlanetRadius;

    float3 Transmittance;
    float3 InScatter;
    GetScatterTransmittance(AP, fHeight, InScatter, Transmittance);

    float3 DeltaTransmittance = dt * Transmittance;
    float3 EyeTransmittance = exp(-TransmittanceSum - 0.5f * DeltaTransmittance);

    float fSunTheta = PI / 2 - acos(dot(-SunDir, normalize(Pos)));

    float2 UV = GetTransmittanceUV(AP, fHeight, fSunTheta);

    if (!IntersectRaySphere(Pos, -SunDir, AP.fPlanetRadius))
    {
        float3 Phase = EstimatePhaseFunc(AP, fHeight, fPhaseU);
        float3 SunTransmittance = gTransmittanceTexture.SampleLevel(gMTSampler, UV, 0);
        InScatteringSum += dt * (EyeTransmittance * InScatter * Phase) * SunTransmittance;
    }

    if (bEnableMultiScattering)
    {
        float3 MultiScattering = gMultiScatteringTexture.SampleLevel(gMTSampler, UV, 0);
        InScatteringSum += dt * EyeTransmittance * InScatter * MultiScattering;
    }

    TransmittanceSum += DeltaTransmittance;
}

float4 PS(FVertexOutput In) : SV_Target0
{
    float fPhi = 2 * PI * In.UV.x;   // [0, 2PI] 在 x-z 平面的投影与 x 轴的夹角.
    float vm = 2 * In.UV.y - 1;
    float fTheta = sign(vm) * (PI / 2) * vm * vm;    // [-PI / 2, PI / 2] 与 x-z 平面的夹角, 平方运算 (vm * vm) 的目的是让垂直分布更接近半球面投影.
    float fSinTheta = sin(fTheta); 
    float fCosTheta = cos(fTheta);

    float3 Ori = CameraPosition;
    float3 Dir = float3(fCosTheta *cos(fPhi), fSinTheta, fCosTheta * sin(fPhi));

    float2 WorldOri = float2(0, Ori.y + AP.fPlanetRadius);
    float2 WorldDir = float2(fCosTheta, fSinTheta);        // 仰角, 垂直于地面的平面上的仰角.

    float fDistance = 0.0f;
    if (!IntersectRayCircle(WorldOri, WorldDir, AP.fPlanetRadius, fDistance))
    {
        IntersectRayCircle(WorldOri, WorldDir, AP.fAtmosphereRadius, fDistance);
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