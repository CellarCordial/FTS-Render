// #define THREAD_GROUP_SIZE_X 0
// #define THREAD_GROUP_SIZE_Y 0

#include "../DDGI.slang"


cbuffer pass_constants : register(b0)
{
    float4x4 InvViewProj;

    float3 camera_position;
    float pad;

    FDDGIVolumeData VolumeData;
};

Texture2D<float3> gIrradianceTexture : register(t0);
Texture2D<float2> gDepthTexture : register(t1);

Texture2D<float2> gGBufferNormal : register(t2);
Texture2D<float> gGBufferDepth : register(t3);

RWTexture2D<float4> gOutputTexture : register(u0);

SamplerState gSampler : register(s0);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void compute_shader(uint3 ThreadID : SV_DispatchThreadID)
{
    uint2 uv = ThreadID.xy;
    float depth = gDepthTexture[uv];
    
    if (abs(depth - 1.0f) <= 0.0001f)
    {
        gOutputTexture[uv] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }
    
    float3 PixelWorldPos = get_world_pos_from_depth_ndc(uv, depth, InvViewProj);
    float3 PixelWorldNormal = OctahedronToUnitVector(gGBufferNormal[uv]);
    float3 PixelToCamera = normalize(camera_position - PixelWorldPos);

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