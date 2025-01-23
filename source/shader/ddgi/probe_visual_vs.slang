#include "../common/ddgi.slang"


cbuffer pass_constants : register(b0)
{
    DDGIVolumeData VolumeData;
    float4x4 view_proj;
    float fProbeScale;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
};

struct VertexOutput
{
    float4 sv_position  : SV_Position;
    float3 normal     : NORMAL;
    uint32_t dwProbeIndex : PROBE_INDEX;
};

[shader("vertex")]
VertexOutput main(VertexInput In, uint32_t dwProbeIndex : SV_InstanceID)
{
    VertexOutput Out;

    uint3 ProbeID = uint3(
        dwProbeIndex % VolumeData.probe_count.x,
        (dwProbeIndex / VolumeData.probe_count.x) % VolumeData.probe_count.y,
        dwProbeIndex / (VolumeData.probe_count.x * VolumeData.probe_count.y)
    );

    float3 ProbePos = VolumeData.origin_position + VolumeData.probe_interval_size * float3(ProbeID);

    Out.sv_position = mul(float4(In.position * fProbeScale, 1.0f), view_proj);
    Out.normal = In.normal;
    Out.dwProbeIndex = dwProbeIndex;

    return Out;
}