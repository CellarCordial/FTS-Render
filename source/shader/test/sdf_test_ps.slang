#include "../common/sdf_trace.slang"

cbuffer gPassConstants : register(b0)
{
    float3 frustum_A;        float pad0;
    float3 frustum_B;        float pad1;
    float3 frustum_C;        float pad2;
    float3 frustum_D;        float pad3;
    float3 camera_pos;  float pad4;

    GlobalSdfData global_sdf_data;
    float2 pad5;
};

Texture3D<float> gSdf : register(t0);
SamplerState gSampler : register(s0);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float2 uv : TEXCOORD;
};

[shader("pixel")]
float4 main(VertexOutput input) : SV_Target0
{
    float3 o = camera_pos;
    float3 d = lerp(
        lerp(frustum_A, frustum_B, input.uv.x),
        lerp(frustum_C, frustum_D, input.uv.x),
        input.uv.y
    );

    SDFHitData hit_data = trace_global_sdf(o, d, global_sdf_data, gSdf, gSampler);

    float color = float(hit_data.step_count) / float(global_sdf_data.max_trace_steps - 1);
    color = pow(color, 1 / 2.2f);
    return float4(color.xxx, 1.0f);
}