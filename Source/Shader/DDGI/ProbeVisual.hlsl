#include "../DDGICommon.hlsli"

cbuffer gPassConstants : register(b0)
{
    FDDGIVolumeData VolumeData;
    float4x4 ViewProj;
    float fProbeScale;
};

Texture2D gIrradianceTexture : register(t0);
SamplerState gSampler : register(s0);

struct FVertexInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
};

struct FVertexOutput
{
    float4 PositionH  : SV_Position;
    float3 Normal     : NORMAL;
    uint dwProbeIndex : PROBE_INDEX;
};

FVertexOutput VS(FVertexInput In, uint dwProbeIndex : SV_InstanceID)
{
    FVertexOutput Out;

    uint3 ProbeID = uint3(
        dwProbeIndex % VolumeData.ProbesNum.x,
        (dwProbeIndex / VolumeData.ProbesNum.x) % VolumeData.ProbesNum.y,
        dwProbeIndex / (VolumeData.ProbesNum.x * VolumeData.ProbesNum.y)
    );

    float3 ProbePos = VolumeData.OriginPos + VolumeData.fProbeIntervalSize * float3(ProbeID);

    Out.PositionH = mul(float4(In.Position * fProbeScale, 1.0f), ViewProj);
    Out.Normal = In.Normal;
    Out.dwProbeIndex = dwProbeIndex;

    return Out;
}

float4 PS(FVertexOutput In) : SV_Target0
{
    float2 UV = GetProbeTextureUV(
        normalize(In.Normal),
        In.dwProbeIndex,
        VolumeData.dwIrradianceTexturesWidth, 
        VolumeData.dwIrradianceTexturesHeight, 
        VolumeData.dwSingleDepthTextureSize
    );

    return gIrradianceTexture.Sample(gSampler, UV);
}