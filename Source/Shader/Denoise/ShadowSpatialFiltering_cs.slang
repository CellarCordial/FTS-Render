#define THREAD_GROUP_NUM_X 1
#define THREAD_GROUP_NUM_Y 1

cbuffer gPassConstants : register(b0)
{
    uint2 ShadowMapRes;
    float2 InvShadowMapRes;

    float4x4 InvViewMatrix;
    float4x4 InvProjMatrix;

    uint2 ShadowMetaDataTextureRes;
    float fDepthSimilaritySigma;
    uint dwStepSize;
};

Texture2D<float> gDepthTexture : register(t0);
Texture2D<float> gNormalVTexture : register(t1);
Texture2D<uint> gShadowMetaTexture : register(t2);
Texture2D<float2> gShadowTemproalFilteringTexture : register(t3);

RWTexture2D<float2> gOutputDenoisedShadowTexture : register(u0);

float4x4 FFX_DNSR_Shadows_GetViewProjectionInverse() { return InvViewMatrix; }
float2 FFX_DNSR_Shadows_GetInvBufferDimensions() { return InvShadowMapRes; }
float4x4 FFX_DNSR_Shadows_GetProjectionInverse() { return InvProjMatrix; }
float FFX_DNSR_Shadows_GetDepthSimilaritySigma() { return fDepthSimilaritySigma; }

bool FFX_DNSR_Shadows_IsShadowReciever(uint2 PixelID) { return gDepthTexture[PixelID] != 0.0f; }
float FFX_DNSR_Shadows_ReadDepth(uint2 PixelID) { return gDepthTexture[PixelID]; }
float3 FFX_DNSR_Shadows_ReadNormals(uint2 PixelID) { return gNormalVTexture[PixelID]; }

float16_t2 FFX_DNSR_Shadows_ReadInput(uint2 PixelID) { return float16_t2(gShadowTemproalFilteringTexture[PixelID]);}

uint FFX_DNSR_Shadows_ReadTileMetaData(uint dwTileIndex)
{
    uint2 TileID = uint2(
        dwTileIndex % ShadowMetaDataTextureRes.x,
        dwTileIndex / ShadowMetaDataTextureRes.x
    );
    return gShadowMetaTexture[TileID];
}

#include "../../External/ffx-shadows-dnsr/ffx_denoiser_shadows_filter.slang"

#if defined(THREAD_GROUP_NUM_X) && defined(THREAD_GROUP_NUM_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, THREAD_GROUP_NUM_Y, 1)]
void CS(uint3 ThreadID : SV_DispatchThreadID, uint GroupIndex: SV_GroupIndex, uint2 GroupID: SV_GroupID, uint2 GroupThreadID : SV_GroupThreadID)
{
    bool bWriteResults = true;
    float2 Result = FFX_DNSR_Shadows_FilterSoftShadowsPass(GroupID, GroupThreadID, ThreadID.xy, bWriteResults, 0, dwStepSize);
    if (bWriteResults) gOutputDenoisedShadowTexture[ThreadID.xy] = Result;
}

#endif