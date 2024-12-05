#define THREAD_GROUP_NUM_X 0
#define THREAD_GROUP_NUM_Y 0

#include "../Common.hlsli"
#include "../BrdfCommon.hlsli"

cbuffer gPassConstants : register(b0)
{
    uint2 dwBrdfLUTRes;
};

RWTexture2D<float2> gOutputBrdfLUTTexture : register(u0);

float2 IntegrateBrdf(float fNdotV, float fRoughness);

#if defined(THREAD_GROUP_NUM_X) && defined(THREAD_GROUP_NUM_Y)

[numthreads(THREAD_GROUP_NUM_X, THREAD_GROUP_NUM_Y, 1)]
void CS(uint3 ThreadID : SV_DispatchThreadID)
{
    float fNdotV = ((ThreadID.x + 0.5f) / dwBrdfLUTRes.x) * (1.0 - 1e-3) + 1e-3;
    float fRoughness = max((ThreadID.y + 0.5f) / dwBrdfLUTRes.y, 1e-5);

    gOutputBrdfLUTTexture[ThreadID.xy] = IntegrateBrdf(fNdotV, fRoughness);
}

float2 IntegrateBrdf(float fNdotV, float fRoughness)
{
    float3 ViewDir = float3(
        sqrt(1.0f - fNdotV * fNdotV),
        0.0f,
        fNdotV
    );

    float fRed = 0.0f;
    float fGreen = 0.0f;

    const uint SAMPLE_COUNT = 1024u;
    for(uint ix = 0; ix < SAMPLE_COUNT; ++ix)
    {
        float2 Random = Hammersley(ix, SAMPLE_COUNT);
        FBrdfSample Sample = ImportanceSampleGGX(fRoughness, ViewDir, Random);

        if (fNdotV > 0.0f)
        {
            
        }
    }
    return float2(fRed, fGreen);
}









#endif