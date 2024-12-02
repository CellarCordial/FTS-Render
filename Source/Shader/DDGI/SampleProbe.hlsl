// #define THREAD_GROUP_SIZE_X 0
// #define THREAD_GROUP_SIZE_Y 0

#include "../Common.hlsli"
#include "../DDGICommon.hlsli"


cbuffer gPassConstants : register(b0)
{
    float4x4 InvViewProj;

    float3 CameraPos;
    float PAD;

    FDDGIVolumeData VolumeData;
};

Texture2D<float3> gIrradianceTexture : register(t0);
Texture2D<float2> gDepthTexture : register(t1);

Texture2D<float2> gGBufferNormal : register(t2);
Texture2D<float> gGBufferDepth : register(t3);

RWTexture2D<float4> gOutputTexture : register(u0);

SamplerState gSampler : register(s0);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void CS(uint3 ThreadID : SV_DispatchThreadID)
{
    uint2 UV = ThreadID.xy;
    float fDepth = gDepthTexture[UV];
    
    if (abs(fDepth - 1.0f) <= 0.0001f)
    {
        gOutputTexture[UV] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }
    
    float3 PixelWorldPos = GetWorldPostionFromDepthNDC(UV, fDepth, InvViewProj);
    float3 PixelWorldNormal = OctahedronToUnitVector(gGBufferNormal[UV]);
    float3 PixelToCamera = normalize(CameraPos - PixelWorldPos);

    float3 Irradiance = SampleProbeIrradiance(
        VolumeData, 
        PixelWorldPos, 
        PixelWorldNormal, 
        PixelToCamera, 
        gIrradianceTexture, 
        gDepthTexture, 
        gSampler
    );

    gOutputTexture[ThreadID.xy] = float4(Irradiance, 1.0f);
}














#endif