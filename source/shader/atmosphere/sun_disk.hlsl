#include "../PostProcess.hlsli"
#include "../Medium.hlsli"

cbuffer gAtomsphereProperties : register(b0)
{
    FAtmosphereProperties AP;    
};

cbuffer gPassConstant : register(b1)
{
	float4x4 WorldViewProj;
	float3 SunRadius;
	float fSunTheta;
	float fCameraHeight;
    float3 PAD;
};

Texture2D<float3> gTransmittanceTexture : register(t0);
SamplerState gSampler : register(s0);

struct FVertexOutput
{
	float4 PositionH : SV_POSITION;
	float4 ClipPos	 : CLIP_POSITION;
	float3 Transmittance : TRANSMITTANCE;
};

FVertexOutput vertex_shader(float2 Pos : POSITION)
{
	FVertexOutput Out;
	Out.PositionH = mul(float4(Pos, 0.0f, 1.0f), WorldViewProj);
	Out.PositionH.z = Out.PositionH.w;
	Out.ClipPos = Out.PositionH;

    float2 UV = GetTransmittanceUV(AP, fCameraHeight, fSunTheta);
	Out.Transmittance = gTransmittanceTexture.SampleLevel(gSampler, UV, 0);

	return Out;
}

float4 pixel_shader(FVertexOutput In) : SV_Target0
{
	return float4(PostProcess(In.ClipPos.xy / In.ClipPos.w, In.Transmittance * SunRadius), 1.0f);
}