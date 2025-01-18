// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1

#include "../octahedral.slang"

cbuffer pass_constants : register(b0)
{
    uint32_t dwIrradianceTextureRes;
    uint32_t dwRayNumPerProbe;
    float fHistoryAlpha;
    float fHistoryGamma;

    uint32_t bFirstFrame;
    float fDepthSharpness;
};

Texture2D<float3> gRadianceTexture : register(t0);
Texture2D<float4> gDirectionDistanceTexture : register(t1);

RWTexture2D<float3> gOutputIrradianceTexture : register(u0);



#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 ThreadID : SV_DispatchThreadID, uint3 GroupID : SV_GroupID, uint32_t dwGroupIndex : SV_GroupIndex)
{
    if (any(ThreadID.xy >= dwIrradianceTextureRes)) return;
    uint2 StandaloneIrradianceUV = ThreadID.xy + 1;
    uint2 IrradianceUV = GroupID.xy * (dwIrradianceTextureRes + 2) + StandaloneIrradianceUV; 

    uint32_t dwProbeIndex = dwGroupIndex;
    float fWeightSum = 0.0f;
    float3 IrradianceSum = float3(0.0f, 0.0f, 0.0f);
    for (uint32_t ix = 0; ix < dwRayNumPerProbe; ++ix)
    {
        uint2 uv = uint2(ix, dwProbeIndex);
        float3 Radiance = gRadianceTexture[uv];
        float3 RayDirection = gDirectionDistanceTexture[uv].xyz;

        float2 normalizedIrradianceUV = ((float2(StandaloneIrradianceUV) + 0.5f) / float(dwIrradianceTextureRes)) * 2.0f - 1.0f;
        float3 PixelDirection = OctahedronToUnitVector(normalizedIrradianceUV);

        float fWeight = max(0.0f, dot(PixelDirection, RayDirection));
        IrradianceSum += Radiance * fWeight;
        fWeightSum += fWeight;
    }

    if (fWeightSum > 0.00001f)
    {
        IrradianceSum *= 1.0f / (2.0f * fWeightSum);
    }
    if (bFirstFrame == 0)
    {
        float fInvHistoryGamma = 1.0f / fHistoryGamma;
        float3 OldIrrandiance = pow(gOutputIrradianceTexture[IrradianceUV], fInvHistoryGamma);
        IrradianceSum = pow(IrradianceSum, fInvHistoryGamma);
        IrradianceSum = pow(lerp(IrradianceSum, OldIrrandiance, fHistoryAlpha), fHistoryGamma);
    }

    gOutputIrradianceTexture[IrradianceUV] = IrradianceSum;
}




#endif