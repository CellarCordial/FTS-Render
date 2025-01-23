#include "../common/ddgi.slang"

cbuffer pass_constants : register(b0)
{
    DDGIVolumeData VolumeData;
    float4x4 view_proj;
    float fProbeScale;
};

Texture2D gIrradianceTexture : register(t0);
SamplerState gSampler : register(s0);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float3 normal : NORMAL;
    uint32_t dwProbeIndex : PROBE_INDEX;
};

[shader("pixel")]
float4 main(VertexOutput In) : SV_Target0
{
    float2 uv = get_probe_texture_uv(
        normalize(In.normal),
        In.dwProbeIndex,
        VolumeData.irradiance_texture_width, 
        VolumeData.irradiance_texture_height, 
        VolumeData.single_depth_texture_size
    );

    return gIrradianceTexture.Sample(gSampler, uv);
}