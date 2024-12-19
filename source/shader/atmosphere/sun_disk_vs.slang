#include "../post_process.slang"
#include "../atmosphere_properties.slang"

cbuffer gAtomsphereProperties : register(b0)
{
    AtmosphereProperties AP;    
};

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

Texture2D<float3> transmittance_texture : register(t0);
SamplerState gSampler : register(s0);

[shader("vertex")]
VertexOutput vertex_shader(float2 Pos : POSITION)
{
	VertexOutput Out;
	Out.sv_position = mul(float4(Pos, 0.0f, 1.0f), WorldViewProj);
	Out.sv_position.z = Out.sv_position.w;
	Out.ClipPos = Out.sv_position;

    float2 uv = get_transmittance_uv(AP, fCameraHeight, sun_theta);
	Out.transmittance = transmittance_texture.SampleLevel(gSampler, uv, 0);

	return Out;
}