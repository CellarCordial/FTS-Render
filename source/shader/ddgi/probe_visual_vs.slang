#include "../DDGI.slang"


cbuffer pass_constants : register(b0)
{
    FDDGIVolumeData VolumeData;
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
    uint dwProbeIndex : PROBE_INDEX;
};

[shader("vertex")]
VertexOutput vertex_shader(VertexInput In, uint dwProbeIndex : SV_InstanceID)
{
    VertexOutput Out;

    uint3 ProbeID = uint3(
        dwProbeIndex % VolumeData.ProbesNum.x,
        (dwProbeIndex / VolumeData.ProbesNum.x) % VolumeData.ProbesNum.y,
        dwProbeIndex / (VolumeData.ProbesNum.x * VolumeData.ProbesNum.y)
    );

    float3 ProbePos = VolumeData.OriginPos + VolumeData.fProbeIntervalSize * float3(ProbeID);

    Out.sv_position = mul(float4(In.position * fProbeScale, 1.0f), view_proj);
    Out.normal = In.normal;
    Out.dwProbeIndex = dwProbeIndex;

    return Out;
}