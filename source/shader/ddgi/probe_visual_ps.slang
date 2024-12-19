#include "../DDGI.slang"

cbuffer pass_constants : register(b0)
{
    FDDGIVolumeData VolumeData;
    float4x4 view_proj;
    float fProbeScale;
};

Texture2D gIrradianceTexture : register(t0);
SamplerState gSampler : register(s0);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float3 normal : NORMAL;
    uint dwProbeIndex : PROBE_INDEX;
};

[shader("pixel")]
float4 pixel_shader(VertexOutput In) : SV_Target0
{
    float2 uv = GetProbeTextureUV(
        normalize(In.normal),
        In.dwProbeIndex,
        VolumeData.dwIrradianceTexturesWidth, 
        VolumeData.dwIrradianceTexturesHeight, 
        VolumeData.dwSingleDepthTextureSize
    );

    return gIrradianceTexture.Sample(gSampler, uv);
}