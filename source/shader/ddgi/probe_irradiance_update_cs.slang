#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../common/octahedral.slang"

cbuffer pass_constants : register(b0)
{
    uint32_t dwIrradianceTextureRes;
    uint32_t dwRayNumPerProbe;
    float fHistoryAlpha;
    float fHistoryGamma;

    uint32_t bFirstFrame;
    float fDepthSharpness;
};

Texture2D<float3> ddgi_radiance_texture : register(t0);
Texture2D<float4> ddgi_direction_distance_texture : register(t1);

RWTexture2D<float3> ddgi_volume_irradiance_texture : register(u0);



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
        float3 Radiance = ddgi_radiance_texture[uv];
        float3 RayDirection = ddgi_direction_distance_texture[uv].xyz;

        float2 normalizedIrradianceUV = ((float2(StandaloneIrradianceUV) + 0.5f) / float(dwIrradianceTextureRes)) * 2.0f - 1.0f;
        float3 PixelDirection = octahedron_to_unit_vector(normalizedIrradianceUV);

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
        float3 OldIrrandiance = pow(ddgi_volume_irradiance_texture[IrradianceUV], fInvHistoryGamma);
        IrradianceSum = pow(IrradianceSum, fInvHistoryGamma);
        IrradianceSum = pow(lerp(IrradianceSum, OldIrrandiance, fHistoryAlpha), fHistoryGamma);
    }

    ddgi_volume_irradiance_texture[IrradianceUV] = IrradianceSum;
}




#endif