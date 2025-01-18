#include "../post_process.slang"
#include "../atmosphere_properties.slang"

cbuffer gPassConstant : register(b1)
{
	float4x4 WorldViewProj;
	float3 SunRadius;
	float sun_theta;
	float fCameraHeight;
    float3 pad;
};

struct VertexOutput
{
	float4 sv_position : SV_POSITION;
	float4 ClipPos	 : CLIP_POSITION;
	float3 transmittance : TRANSMITTANCE;
};

[shader("pixel")]
float4 main(VertexOutput In) : SV_Target0
{
	return float4(simple_post_process(In.ClipPos.xy / In.ClipPos.w, In.transmittance * SunRadius), 1.0f);
}