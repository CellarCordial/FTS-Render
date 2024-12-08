// #define THREAD_GROUP_NUM_X 1
// #define THREAD_GROUP_NUM_Y 1
// #define TEMPORAL

cbuffer gPassConstants : register(b0)
{
    float4x4 ProjMatrix;

    uint2 SSAOTextureRes;
    float fSampleRadius;
    float fSampleBias;

    uint dwTangentNoiseTextureRes;
    uint dwSampleNum;
    uint dwMaxSampleNum;
    uint PAD;
};

Texture2D<float3> gPositionVTexture : register(t0);
Texture2D<float3> gNormalVTexture : register(t1);
Texture2D<float3> gTangentNoiseTexture : register(t2);
SamplerState gSampler : register(s0);

StructuredBuffer<float3> gSampleDirectionsT : register(t3);

#ifdef TEMPORAL
Texture2D<float3> gVelocityTexture : register(t4);
Texture2D<float2> gReprojectionTexture : register(t5);
Texture2D<float2> gHistorySSAOTexture : register(t6);
#endif

RWTexture2D<float2> gOutputSSAOTexture : register(u0);

#if defined(THREAD_GROUP_NUM_X) && defined(THREAD_GROUP_NUM_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, THREAD_GROUP_NUM_Y, 1)]
void CS(uint3 ThreadID: SV_DispatchThreadID)
{
    float3 PositionV = gPositionVTexture[ThreadID.xy];
    float3 NormalV = gNormalVTexture[ThreadID.xy];
    float3 RandomTangent = gTangentNoiseTexture[ThreadID.xy * (SSAOTextureRes / dwTangentNoiseTextureRes)];

#ifdef TEMPORAL
    float3 Velocity = gVelocityTexture[ThreadID.xy];
    float3 PrevPositionV = PositionV + Velocity;

    bool bDiscardHistorySampleAccumulation = false;
#endif

    // Gramm-Schmidt Process.
    float3 Tangent = normalize(RandomTangent - NormalV * dot(NormalV, RandomTangent));
    float3 Bitangent = cross(NormalV, Tangent);
    float3x3 TBN = float3x3(Tangent, Bitangent, NormalV);

    float fOcclusionSum = 0.0f;
    for (uint ix = 0; ix < dwSampleNum; ++ix)
    {
        float3 SampleDirV = mul(gSampleDirectionsT[ix], TBN);
        float3 SamplePosV = PositionV + SampleDirV * fSampleRadius;

        float4 SamplePosP = mul(float4(SamplePosV, 1.0f), ProjMatrix);
        float2 SamplePosUV = ((SamplePosP.xy / SamplePosP.w) * float2(0.5f, -0.5f) + 0.5f);

        float fSampleDepth = gPositionVTexture.Sample(gSampler, SamplePosUV).z;
        float fRangeCheck = smoothstep(0.0f, 1.0f, fSampleRadius / abs(PositionV.z - fSampleDepth));

        fOcclusionSum += (fSampleDepth <= SamplePosV.z + fSampleBias ? 1.0f : 0.0f) * fRangeCheck;

#ifdef TEMPORAL
        //
        // ||s_i - p| - |s_iold - p_old|| < epsilon         (Func0)
        //
        float3 PrevSamplePosV = SamplePosV + Velocity;
        if (abs(length(SamplePosV - PositionV) - length(PrevSamplePosV - PrevPositionV)) < 1e-5)
        {
            bDiscardHistorySampleAccumulation = true;
        }
#endif
    }

#ifdef TEMPORAL
    float2 UVOffset = gReprojectionTexture[ThreadID.xy];
    float2 HistoryUV = UVOffset + float2(
        (ThreadID.x + 0.5f) / SSAOTextureRes.x,
        (ThreadID.y + 0.5f) / SSAOTextureRes.y
    );

    //        d_new
    // |1 - --------- | < epsilon               (Func1)
    //        d_old
    if (abs(1.0f - PositionV.z / PrevPositionV.z) < 1e-5)
    {
        bDiscardHistorySampleAccumulation = true;
    }

    // 若 (Func0) 或 (Func1) 达成条件, 则舍弃之前积累的 Occlusion. (Occlusion.y = n_t(p) / n_max)
    float2 HistoryOcclusion = gHistorySSAOTexture.Sample(gSampler, HistoryUV);
    HistoryOcclusion.y *= bDiscardHistorySampleAccumulation ? 0 : dwMaxSampleNum;

    // n_{t+1}(p) = min(n_t(p) + k * n_max)
    float fNewSampleNumAccumulation = min(dwMaxSampleNum, HistoryOcclusion.y + dwSampleNum);

    //                 n_t(p) * AO_t(p_old) + kC_{t+1}(p)
    // AO_{t+1}(p) = --------------------------------------
    //                          n_t(p) + k
    float fOcclusion = (HistoryOcclusion.x * HistoryOcclusion.y + fOcclusionSum * dwSampleNum) / fNewSampleNumAccumulation;

    gOutputSSAOTexture[ThreadID.xy] = float2(fOcclusion, fNewSampleNumAccumulation / dwMaxSampleNum);

#else
    gOutputSSAOTexture[ThreadID.xy] = float2(fOcclusionSum, 0.0f);

#endif
}
 





#endif