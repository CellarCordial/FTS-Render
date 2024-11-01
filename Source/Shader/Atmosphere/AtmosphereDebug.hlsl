#include "../PostProcess.hlsli"
#include "../Medium.hlsli"

cbuffer gPassConstant0 : register(b0)
{
    float4x4 ViewProj;
    float4x4 WorldMatrix;
};

cbuffer gPassConstant1: register(b1)
{
    float3 SunDirection;   
    float fSunTheta;
    
    float3 SunIntensity;   
    float fMaxAerialDistance;
    
    float3 CameraPos;      
    float fWorldScale;
    
    float4x4 ShadowViewProj;
    
    float2 JitterFactor;   
    float2 BlueNoiseUVFactor;

    float3 CameraPosition;
    float PAD;
};

cbuffer gAtomsphereProperties : register(b2)
{
    FAtmosphereProperties AP;
};

struct FVertexInput
{
    float3 PositionL : POSITION;
    float3 NormalL   : NORMAL;
    float4 TangentL  : TANGENT;
    float2 UV        : TEXCOORD;
};

struct FVertexOutput
{
    float4 PositionH    : SV_POSITION;
    float4 ScreenPos    : SCREEN_POSITION;
    float3 PositionW    : POSITION;
    float3 NormalW      : NORMAL;
};

Texture2D<float3> gTransmittanceTexture : register(t0);
Texture3D<float4> gAerialLUTTexture : register(t1);
SamplerState gTASampler : register(s0);

Texture2D<float> gShadowMapTexture : register(t2);
SamplerState gShadowMapSampler : register(s1);

Texture2D<float2> gBlueNoiseTexture : register(t3);
SamplerState gBlueNoiseSampler : register(s2);

FVertexOutput VS(FVertexInput In)
{
    FVertexOutput Out;
    float4 WorldPosition = mul(float4(In.PositionL, 1.0f), WorldMatrix);
    Out.PositionH = mul(WorldPosition, ViewProj);
    Out.ScreenPos = Out.PositionH;
    Out.PositionW = WorldPosition.xyz;
    Out.NormalW = normalize(mul(float4(In.NormalL, 1.0f), WorldMatrix)).xyz;

    return Out;
}

float4 PS(FVertexOutput In) : SV_Target0
{
    float2 ScreenPos = In.ScreenPos.xy / In.ScreenPos.w;
    ScreenPos = 0.5f + float2(0.5f, -0.5f) * ScreenPos;

    float2 BlueNoiseUV = ScreenPos * BlueNoiseUVFactor;
    float2 BlueNoise = gBlueNoiseTexture.Sample(gBlueNoiseSampler, BlueNoiseUV);

    // bn.x 控制偏移强度，bn.y 决定随机方向的角度，通过余弦和正弦函数将角度转换为2D方向矢量.
    float2 Jitter = JitterFactor * BlueNoise.x * float2(cos(2.0f * PI * BlueNoise.y), sin(2.0f * PI * BlueNoise.y));

    float fAerialPerspectiveZ = fWorldScale * distance(In.PositionW, CameraPos) / fMaxAerialDistance;
    float4 AerialPerspective = gAerialLUTTexture.Sample(gTASampler, float3(ScreenPos + Jitter, saturate(fAerialPerspectiveZ)));

    float3 InScatter = AerialPerspective.xyz;
    float fEyeTransmittance = AerialPerspective.w;

    float2 TransmittanceUV = GetTransmittanceUV(AP, fWorldScale * In.PositionW.y, fSunTheta);
	float3 SunTransmittance = gTransmittanceTexture.Sample(gTASampler, TransmittanceUV);

    float3 SunRadiance = CameraPosition * max(0.0f, dot(In.NormalW, -SunDirection));

    // 将物体位置沿着法线稍微偏移 0.03 个单位。这种偏移可以避免阴影失真 z-fighting.
    float4 ShadowClip = mul(float4(In.PositionW + 0.03 * In.NormalW, 1.0f), ShadowViewProj);
    float2 ShadowNDC = ShadowClip.xy / ShadowClip.w;
    float2 ShadowUV = 0.5f + float2(0.5f, -0.5f) * ShadowNDC;

    float fShadowFactor = 1.0f;
    if (all(saturate(ShadowUV) == ShadowUV))
    {
        float fDepth = gShadowMapTexture.Sample(gShadowMapSampler, ShadowUV);
        fShadowFactor = ShadowClip.z <= fDepth;
    }

    float3 OutColor = SunIntensity * (fShadowFactor * SunRadiance * SunTransmittance * fEyeTransmittance + InScatter);
    OutColor = PostProcess(ScreenPos, OutColor);
    return float4(OutColor, 1.0f); 
}

