#define THREAD_GROUP_NUM_X 1
#define THREAD_GROUP_NUM_Y 1

#include "BicubicTextureFiltering.slang"

cbuffer gPassConstants : register(b0)
{
    uint2 ShadowMapRes;
    float2 InvShadowMapRes;

    float4x4 InvViewMatrix;
    float4x4 InvProjMatrix;
    float4x4 ReprojMatrix;

    float3 CameraPos;
    float fMaxShadowMomentsVariance;

    uint2 ShadowMetaDataTextureRes;
    uint bIsFirstFrame;

    uint2 ShadowMomentsTextureRes;
};

Texture2D<float4> gShadowMapTexture : register(t0);
Texture2D<float4> gPrevShadowMapTexture : register(t1);
Texture2D<uint> gBitPackedShadowMaskTexture : register(t2);
Texture2D<float4> gPrevShadowMomentsTexture : register(t3);

RWTexture2D<uint> gOutputShadowMetaTexture : register(u0);
RWTexture2D<float4> gOutputShadowMomentsTexture : register(u1);
RWTexture2D<float2> gOutputShadowTemproalFilteringTexture : register(u2);

float4x4 FFX_DNSR_Shadows_GetViewProjectionInverse() { return InvViewMatrix; }
float2 FFX_DNSR_Shadows_GetInvBufferDimensions() { return InvShadowMapRes; }
float4x4 FFX_DNSR_Shadows_GetProjectionInverse() { return InvProjMatrix; }
float4x4 FFX_DNSR_Shadows_GetReprojectionMatrix() { return ReprojMatrix; }
uint2 FFX_DNSR_Shadows_GetBufferDimensions() { return ShadowMapRes; }
uint FFX_DNSR_Shadows_IsFirstFrame() { return bIsFirstFrame; }
float3 FFX_DNSR_Shadows_GetEye() { return CameraPos; }

float FFX_DNSR_Shadows_ReadDepth() { return 0.5f; }
bool FFX_DNSR_Shadows_IsShadowReciever(uint2 ID) { return true; }
float FFX_DNSR_Shadows_ReadPreviousDepth(uint2 ID) { return 0.5f; }
float2 FFX_DNSR_Shadows_ReadVelocity(uint2 ID) { return float2(0.0f, 0.0f); }
float3 FFX_DNSR_Shadows_ReadNormals(uint2 ID) { return float3(0.0f, 0.0f, 1.0f); }

void FFX_DNSR_Shadows_WriteMetadata(uint dwTileIndex, uint dwMask)
{
    uint2 TileID = uint2(
        dwTileIndex % ShadowMetaDataTextureRes.x,
        dwTileIndex / ShadowMetaDataTextureRes.x
    );
    gOutputShadowMetaTexture[TileID] = dwMask;
}

uint FFX_DNSR_Shadows_ReadRaytracedShadowMask(uint dwTileIndex)
{
    uint2 TileID = uint2(
        dwTileIndex % ShadowMetaDataTextureRes.x,
        dwTileIndex / ShadowMetaDataTextureRes.x
    );
    return gBitPackedShadowMaskTexture[TileID];
}

void FFX_DNSR_Shadows_WriteMoments(uint2 PixelID, float4 ShadowMoments)
{
    ShadowMoments.z = min(ShadowMoments.z, fMaxShadowMomentsVariance);
    gOutputShadowMomentsTexture[PixelID] = ShadowMoments;
}

void FFX_DNSR_Shadows_WriteReprojectionResults(uint2 PixelID, float2 ShadowVariance)
{
    gOutputShadowTemproalFilteringTexture[PixelID] = ShadowVariance;
}

float FFX_DNSR_Shadows_HitsLight(uint2 PixelID)
{
    return gShadowMapTexture[PixelID].r;
}

float4 FFX_DNSR_Shadows_ReadPreviousMomentsBuffer(float2 UV)
{
    float4 Moments = BicubicHermiteTextureSample(UV, gPrevShadowMomentsTexture, ShadowMomentsTextureRes);
    Moments.yz = max(float2(0.0f, 0.0f), Moments.yz);
    return Moments;
}

float FFX_DNSR_Shadows_ReadHistory(float2 UV)
{
    return BicubicHermiteTextureSample(UV, gPrevShadowMapTexture, ShadowMapRes).r;
}


#include "../../External/ffx-shadows-dnsr/ffx_denoiser_shadows_tileclassification.slang"

#if defined(THREAD_GROUP_NUM_X) && defined(THREAD_GROUP_NUM_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, THREAD_GROUP_NUM_Y, 1)]
void CS(uint GroupIndex: SV_GroupIndex, uint2 GroupID : SV_GroupID)
{
    FFX_DNSR_Shadows_TileClassification(GroupIndex, GroupID);
}


#endif