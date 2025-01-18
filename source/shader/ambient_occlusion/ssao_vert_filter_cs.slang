// #define THREAD_GROUP_NUM_Y 1
// #define BLUR_RADIUS 1
// #define TEMPORAL

cbuffer pass_constants : register(b0)
{
    float w0;
    float w1;
    float w2;
    float w3;
    float w4;
    float w5;
    float w6;
    float w7;
    float w8;
    float w9;
    float w10;
    float CAdaptive;
};

Texture2D<float2> gTempSSAOTexture : register(t0);
Texture2D<float3> gPositionVTexture : register(t1);
Texture2D<float3> gNormalVTexture : register(t2);

RWTexture2D<float2> gOutputSSAOTexture : register(u0);

#if defined(THREAD_GROUP_NUM_Y) && defined(BLUR_RADIUS)

groupshared float2 gOcclusionCache[THREAD_GROUP_NUM_Y + 2 * BLUR_RADIUS];

[shader("compute")]
[numthreads(1, THREAD_GROUP_NUM_Y, 1)]
void main(uint3 ThreadID: SV_ThreadID, uint3 GroupThreadID: SV_GroupThreadID)
{
    uint2 SSAOTextureRes;
    gTempSSAOTexture.GetDimensions(SSAOTextureRes.x, SSAOTextureRes.y);

    if (any(ThreadID.xy >= SSAOTextureRes)) return;

    float2 Occlusion = gTempSSAOTexture[ThreadID.xy];

#ifdef TEMPORAL
    //          max(C_adaptive - conv(p), 0)
    // s(p) = ---------------------------------
    //                  C_adaptive
    float fShrinkingFactor = (max(CAdaptive - Occlusion.y, 0.0f) / CAdaptive);
    if (fShrinkingFactor == 0.0f)
    {
        gOutputSSAOTexture[ThreadID.xy] = Occlusion;
        return;
    }

#endif

    float fWeights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };

    if (GroupThreadID.y < BLUR_RADIUS)
    {
        uint32_t y = max(ThreadID.y - BLUR_RADIUS, 0);
        gOcclusionCache[GroupThreadID.y] = gTempSSAOTexture[uint2(ThreadID.x, y)];
    }
    if (GroupThreadID.y >= THREAD_GROUP_NUM_Y - BLUR_RADIUS)
    {
        uint32_t y = min(ThreadID.y + BLUR_RADIUS, SSAOTextureRes.y - 1);
        gOcclusionCache[GroupThreadID.y + 2 * BLUR_RADIUS] = gTempSSAOTexture[uint2(ThreadID.x, y)];
    }

    gOcclusionCache[GroupThreadID.y + BLUR_RADIUS] = Occlusion;

    GroupMemoryBarrierWithGroupSync();

    float fCenterDepth = gPositionVTexture[ThreadID.xy].z;
    float3 CenterNormal = gNormalVTexture[ThreadID.xy];

    float fOcclusionBlur = 0.0f;

#ifdef TEMPORAL
    float fNormalizationWeightSum = 0.0f;
#else
    float fDepthNormalWeightSum = 0.0f;
#endif

    for (int32_t i = -BLUR_RADIUS; i <= BLUR_RADIUS; ++i)
    {
        uint32_t k = GroupThreadID.y + BLUR_RADIUS + i;

#ifdef TEMPORAL
        float fNormalizationWeight = fWeights[i + BLUR_RADIUS] * gOcclusionCache[k].y;
        fOcclusionBlur += fNormalizationWeight * gOcclusionCache[k].x;
        fNormalizationWeightSum += fNormalizationWeight;
#else
        float fNeighborDepth = gPositionVTexture[uint2(ThreadID.x, clamp(ThreadID.y + i, 0, SSAOTextureRes.y))].z;
        float3 NeighborNormal = gNormalVTexture[uint2(ThreadID.x, clamp(ThreadID.y + i, 0, SSAOTextureRes.y))];

        float fDepthWeight = exp2(-200.0f * abs(1.0f - fCenterDepth / fNeighborDepth));
        float fNormalWeight = max(0.0f, dot(CenterNormal, NeighborNormal));
        fNormalWeight *= fNormalWeight * fNormalWeight;

        float fDepthNormalWeight = fDepthWeight * fNormalWeight;

        fOcclusionBlur += fWeights[i + BLUR_RADIUS] * gOcclusionCache[k].x;
        fDepthNormalWeightSum += fDepthNormalWeight;
#endif
    }

#ifdef TEMPORAL
    gOutputSSAOTexture[ThreadID.xy] = float2(fOcclusionBlur / fNormalizationWeightSum, Occlusion.y);
#else
    gOutputSSAOTexture[ThreadID.xy] = float2(fOcclusionBlur / fDepthNormalWeightSum, Occlusion.y);
#endif
}

#endif