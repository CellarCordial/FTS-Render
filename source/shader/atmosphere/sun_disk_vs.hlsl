#include "../common/post_process.hlsl"
#include "../common/atmosphere_properties.hlsl"

cbuffer atomsphere_properties : register(b0)
{
    AtmosphereProperties AP;    
};

cbuffer pass_constants : register(b1)
{
	float4x4 WorldViewProj;
	float3 SunRadius;
	float sun_theta;
	float camera_height;
    float3 pad;
};

struct VertexOutput
{
	float4 sv_position : SV_POSITION;
	float4 clip_pos	 : CLIP_POSITION;
	float3 transmittance : TRANSMITTANCE;
};

Texture2D<float3> transmittance_texture : register(t0);
SamplerState sampler_: register(s0);


VertexOutput main(float2 Pos : POSITION)
{
	VertexOutput output;
	output.sv_position = mul(float4(Pos, 0.0f, 1.0f), WorldViewProj);
	output.sv_position.z = output.sv_position.w;
	output.clip_pos = output.sv_position;

    float2 uv = get_transmittance_uv(AP, camera_height, sun_theta);
	output.transmittance = transmittance_texture.SampleLevel(sampler_, uv, 0);

	return output;
}