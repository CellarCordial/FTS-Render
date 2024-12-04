#define THREAD_GROUP_SIZE_X 0
#define THREAD_GROUP_SIZE_Y 0

#include "../Common.hlsli"
#include "../DDGICommon.hlsli"
#include "../SdfCommon.hlsli"
#include "../SkyCommon.hlsli"


cbuffer gPassConstants : register(b0)
{
    float4x4 RandomOrientation;
    
    float fSdfVoxelSize;
    float fSdfChunkSize;
    float fSceneGridSize;
    float fMaxGIDistance;

    uint dwSurfaceTextureRes;
    uint dwSurfaceAtlasRes;
    
    FDDGIVolumeData VolumeData;
    FGlobalSdfData SdfData;    
};

struct FSdfChunkData
{
    int dwModelIndexBegin;
    int dwModelIndexEnd;
};

struct FCardData
{
    float4x4 LocalMatrix;
    uint2 AtlasOffset;
    float3 Extent;
};

struct FModelSurfaceData
{
    float fHalfBoxDiagonal;
    float3 BoxUpper;
    float3 BoxLower;

    FCardData Cards[6];
};

Texture3D<float> gGlobalSDF : register(t0);
SamplerState gSDFSampler : register(s0);

Texture2D<float3> gSkyLUT : register(t1);
SamplerState gSkySampler : register(s1);

Texture2D<float3> gOutputRadianceTexture : register(t2);
Texture2D<float4> gOutputDirectionDistanceTexture : register(t3);

StructuredBuffer<FSdfChunkData> gSdfChunkDatas : register(t4);
StructuredBuffer<FModelSurfaceData> gModelSurfaceDatas : register(t4);

Texture2D<float4> gSurfaceNormalTexture : register(t5);
Texture2D<float4> gSurfaceLightTexture : register(t6);
Texture2D<float> gSurfaceDepthTexture : register(t7);
SamplerState gSurfaceSampler : register(s2);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void CS(uint3 ThreadID : SV_DispatchThreadID)
{
    uint dwRayIndex = ThreadID.x;
    uint dwProbeIndex = ThreadID.y;

    uint dwTotalProbesNum = VolumeData.ProbesNum.x * VolumeData.ProbesNum.y * VolumeData.ProbesNum.z;
    if (dwRayIndex >= VolumeData.dwRaysNum || dwProbeIndex >= dwTotalProbesNum) return;

    uint3 ProbeID = uint3(
        dwProbeIndex % VolumeData.ProbesNum.x,
        (dwProbeIndex / VolumeData.ProbesNum.x) % VolumeData.ProbesNum.y,
        dwProbeIndex / (VolumeData.ProbesNum.x * VolumeData.ProbesNum.y)
    );
    float3 RayOri = VolumeData.OriginPos + ProbeID * VolumeData.fProbeIntervalSize;
    float3 RayDir = mul(float4(SphericalFibonacci(dwRayIndex, VolumeData.dwRaysNum), 1.0f), RandomOrientation).xyz;

    FSdfHitData HitData = TraceGlobalSdf(RayOri, RayDir, SdfData, gGlobalSDF, gSDFSampler);

    float3 Radiance = float3(0.0f, 0.0f, 0.0f);
    float fDistance = 0.0f;

    if (HitData.fStepSize < 0.00001f)   // 没有击中.
    {
        Radiance = gSkyLUT.Sample(gSkySampler, GetSkyUV(RayDir));
        fDistance = fMaxGIDistance;
    }
    else
    {
        if (HitData.fSdf <= 0.0f)
        {
            fDistance = fMaxGIDistance;
        }
        else
        {
            float3 HitPos = RayOri + RayDir * HitData.fStepSize;

            uint dwChunkNumPerAxis = fSceneGridSize / fSdfChunkSize;
            float3 SceneGridOrigin = -fSceneGridSize * 0.5f;
            uint3 ChunkID = (HitPos - SceneGridOrigin) / fSdfChunkSize;
            uint dwChunkIndex = ChunkID.x + ChunkID.y * dwChunkNumPerAxis + ChunkID.z * dwChunkNumPerAxis * dwChunkNumPerAxis;
            FSdfChunkData ChunkData = gSdfChunkDatas[dwChunkIndex];
            
            // Chunk 不为空.
            if (ChunkData.dwModelIndexBegin != -1)
            {
                float fWeightSum = 0.0f;
                for (int ix = ChunkData.dwModelIndexBegin; ix < ChunkData.dwModelIndexEnd; ++ix)
                {
                    FModelSurfaceData SurfaceData = gModelSurfaceDatas[ix];

                    float3 Center = (SurfaceData.BoxUpper + SurfaceData.BoxLower) * 0.5f;
                    if (distance(Center, HitPos) > SurfaceData.fHalfBoxDiagonal) continue;

                    float3 ClampHitPos = clamp(HitPos, SurfaceData.BoxLower, SurfaceData.BoxUpper);
                    if (any(abs(ClampHitPos - HitPos) > fSdfVoxelSize)) continue;
                    
                    [unroll]
                    for (uint jx = 0; jx < 6; ++jx)
                    {
                        FCardData Card = SurfaceData.Cards[jx];
                        float3 LocalHitPos = mul(float4(HitPos, 1.0f), Card.LocalMatrix).xyz;
                        
                        float2 UV = saturate(LocalHitPos.xy / Card.Extent.xy + 0.5f);
                        UV.y = 1.0f - UV.y;

                        // 双线性插值权重.
                        float4 BilinearWeight = float4(
                            (1.0f - UV.x) * (1.0f - UV.y),
                            (UV.x)        * (1.0f - UV.y),
                            (1.0f - UV.x) * (UV.y),
                            (UV.x)        * (UV.y)
                        );

                        UV = (UV * dwSurfaceTextureRes + Card.AtlasOffset) / dwSurfaceAtlasRes;

                        // 击中点的深度与实际 surface 上的深度比值权重.
                        float4 SurfaceDepth = gSurfaceDepthTexture.Gather(gSurfaceSampler, UV) * Card.Extent.z;
                        float4 DepthWeight;
                        [unroll]
                        for (uint kx = 0; kx < 4; ++kx)
                        {
                            if (SurfaceDepth[kx] >= 1.0f) 
                            {
                                DepthWeight[kx] = 0.0f;
                            }
                            else 
                            {
                                // 如果 hit position 和其所对应的 surface depth 相隔超过 3 个体素距离, 则 depthVisibility = 0.0f, 
                                // 在 2 ~ 3 个体素之间 depthVisibility = 0 ~ 1,
                                // 在 2 个体素以下 depthVisibility = 1.
                                DepthWeight[kx] = 1.0f - clamp(abs(LocalHitPos.z - SurfaceDepth[kx]) - 2.0f * fSdfVoxelSize, 0.0f, 1.0f);
                            }
                        }

                        // 光线与击中点的夹角权重.
                        float3 HitNormal = gSurfaceNormalTexture.SampleLevel(gSurfaceSampler, UV, 0.0f);
                        float fNormalWeight = saturate(dot(HitNormal, RayDir));
                        
                        // 总权重.
                        float fSampleWeight = dot(DepthWeight, BilinearWeight) * fNormalWeight;
                        if (fSampleWeight <= 0.0f) continue;
                        else
                        {
                            Radiance += float3(
                                dot(gSurfaceLightTexture.GatherRed(gSurfaceSampler, UV), fSampleWeight), 
                                dot(gSurfaceLightTexture.GatherGreen(gSurfaceSampler, UV), fSampleWeight), 
                                dot(gSurfaceLightTexture.GatherBlue(gSurfaceSampler, UV), fSampleWeight)
                            );
                            fWeightSum += fSampleWeight;
                        }
                    }
                }
                Radiance /= max(fWeightSum, 0.0001f);
            }

            fDistance = max(HitData.fStepSize + 0.5f * fSdfVoxelSize, 0.0f);
        }
    }

    gOutputRadianceTexture[ThreadID.xy] = Radiance;
    gOutputDirectionDistanceTexture[ThreadID.xy] = float4(RayDir, fDistance);
}




#endif