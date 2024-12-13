#define THREAD_GROUP_SIZE_X 0
#define THREAD_GROUP_SIZE_Y 0

#include "../octahedral.hlsli"

cbuffer gPassConstants : register(b0)
{
    uint dwIrradianceTextureRes;
    uint dwRayNumPerProbe;
    float fHistoryAlpha;
    float fHistoryGamma;

    uint bFirstFrame;
    float fDepthSharpness;
};

Texture2D<float3> gRadianceTexture : register(t0);
Texture2D<float4> gDirectionDistanceTexture : register(t1);

RWTexture2D<float3> gOutputIrradianceTexture : register(u0);



#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void CS(uint3 ThreadID : SV_DispatchThreadID, uint3 GroupID : SV_GroupID, uint dwGroupIndex : SV_GroupIndex)
{
    if (any(ThreadID.xy >= dwIrradianceTextureRes)) return;
    uint2 StandaloneIrradianceUV = ThreadID + 1;
    uint2 IrradianceUV = GroupID.xy * (dwIrradianceTextureRes + 2) + StandaloneIrradianceUV; 

    uint dwProbeIndex = dwGroupIndex;
    float fWeightSum = 0.0f;
    float3 IrradianceSum = float3(0.0f, 0.0f, 0.0f);
    for (uint ix = 0; ix < dwRayNumPerProbe; ++ix)
    {
        uint2 UV = uint2(ix, dwProbeIndex);
        float3 Radiance = gRadianceTexture[UV];
        float3 RayDirection = gDirectionDistanceTexture[UV].xyz;

        float2 NormalizedIrradianceUV = ((float2(StandaloneIrradianceUV) + 0.5f) / float(dwIrradianceTextureRes)) * 2.0f - 1.0f;
        float3 PixelDirection = OctahedronToUnitVector(NormalizedIrradianceUV);

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