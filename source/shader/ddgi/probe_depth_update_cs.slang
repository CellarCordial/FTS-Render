// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1

#include "../octahedral.slang"

cbuffer pass_constants : register(b0)
{
    uint32_t dwDepthTextureRes;
    uint32_t dwRayNumPerProbe;
    float fHistoryAlpha;
    float fHistoryGamma;

    uint32_t bFirstFrame;
    float fDepthSharpness;
};

Texture2D<float3> gRadianceTexture : register(t0);
Texture2D<float4> gDirectionDistanceTexture : register(t1);

RWTexture2D<float2> gOutputDepthTexture : register(u0);



#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void compute_shader(uint3 ThreadID : SV_DispatchThreadID, uint3 GroupID : SV_GroupID, uint32_t dwGroupIndex : SV_GroupIndex)
{
    if (any(ThreadID.xy >= dwDepthTextureRes)) return;
    uint2 StandaloneDepthUV = ThreadID.xy + 1;
    uint2 DepthUV = GroupID.xy * (dwDepthTextureRes + 2) + StandaloneDepthUV; 

    uint32_t dwProbeIndex = dwGroupIndex;
    float fWeightSum = 0.0f;
    float2 DepthSum = float2(0.0f, 0.0f);
    for (uint32_t ix = 0; ix < dwRayNumPerProbe; ++ix)
    {
        uint2 uv = uint2(ix, dwProbeIndex);
        float4 RayDirectionDistance = gDirectionDistanceTexture[uv];

        float2 normalizedDepthUV = ((float2(StandaloneDepthUV) + 0.5f) / float(dwDepthTextureRes)) * 2.0f - 1.0f;
        float3 PixelDirection = OctahedronToUnitVector(normalizedDepthUV);

        float fWeight = pow(max(0.0f, dot(PixelDirection, RayDirectionDistance.xyz)), fDepthSharpness);
        DepthSum += float2(RayDirectionDistance.w * fWeight, RayDirectionDistance.w * RayDirectionDistance.w * fWeight);
        fWeightSum += fWeight;
    }

    if (fWeightSum > 0.00001f)
    {
        DepthSum *= 1.0f / (2.0f * fWeightSum);
    }
    if (bFirstFrame == 0)
    {
        float fInvHistoryGamma = 1.0f / fHistoryGamma;
        float2 OldIrrandiance = pow(gOutputDepthTexture[DepthUV], fInvHistoryGamma);
        DepthSum = pow(DepthSum, fInvHistoryGamma);
        DepthSum = pow(lerp(DepthSum, OldIrrandiance, fHistoryAlpha), fHistoryGamma);
    }

    gOutputDepthTexture[DepthUV] = DepthSum;
}




#endif