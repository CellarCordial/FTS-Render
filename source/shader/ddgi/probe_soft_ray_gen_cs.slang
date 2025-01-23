#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../common/ddgi.slang"
#include "../common/sdf_trace.slang"
#include "../common/sky.slang"


cbuffer pass_constants : register(b0)
{
    float4x4 random_orientation;
    
    float sdf_voxel_size;
    float sdf_chunk_size;
    float scene_grid_size;
    float max_gi_distance;

    uint32_t dwSurfaceTextureRes;
    uint32_t dwSurfaceAtlasRes;

    DDGIVolumeData VolumeData;
    GlobalSdfData sdf_data;    
};

struct SDFChunkData
{
    int32_t dwModelIndexBegin;
    int32_t dwModelIndexEnd;
};

struct FCardData
{
    float4x4 LocalMatrix;
    uint2 AtlasOffset;
    float3 extent;
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

StructuredBuffer<SDFChunkData> gSdfChunkDatas : register(t4);
StructuredBuffer<FModelSurfaceData> gModelSurfaceDatas : register(t4);

Texture2D<float4> gSurfaceNormalTexture : register(t5);
Texture2D<float4> gSurfaceLightTexture : register(t6);
Texture2D<float> gSurfaceDepthTexture : register(t7);
SamplerState gSurfaceSampler : register(s2);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    uint32_t dwRayIndex = ThreadID.x;
    uint32_t dwProbeIndex = ThreadID.y;

    uint32_t dwTotalProbesNum = VolumeData.probe_count.x * VolumeData.probe_count.y * VolumeData.probe_count.z;
    if (dwRayIndex >= VolumeData.ray_count || dwProbeIndex >= dwTotalProbesNum) return;

    uint3 ProbeID = uint3(
        dwProbeIndex % VolumeData.probe_count.x,
        (dwProbeIndex / VolumeData.probe_count.x) % VolumeData.probe_count.y,
        dwProbeIndex / (VolumeData.probe_count.x * VolumeData.probe_count.y)
    );
    float3 RayOri = VolumeData.origin_position + ProbeID * VolumeData.probe_interval_size;
    float3 RayDir = mul(float4(spherical_fibonacci(dwRayIndex, VolumeData.ray_count), 1.0f), random_orientation).xyz;

    SdfHitData HitData = trace_global_sdf(RayOri, RayDir, sdf_data, gGlobalSDF, gSDFSampler);

    float3 Radiance = float3(0.0f, 0.0f, 0.0f);
    float fDistance = 0.0f;

    if (HitData.step_size < 0.00001f)   // 没有击中.
    {
        Radiance = gSkyLUT.Sample(gSkySampler, get_sky_uv(RayDir));
        fDistance = max_gi_distance;
    }
    else
    {
        if (HitData.sdf <= 0.0f)
        {
            fDistance = max_gi_distance;
        }
        else
        {
            float3 HitPos = RayOri + RayDir * HitData.step_size;

            uint32_t dwChunkNumPerAxis = scene_grid_size / sdf_chunk_size;
            float3 scene_grid_origin = -scene_grid_size * 0.5f;
            uint3 ChunkID = (HitPos - scene_grid_origin) / sdf_chunk_size;
            uint32_t dwChunkIndex = ChunkID.x + ChunkID.y * dwChunkNumPerAxis + ChunkID.z * dwChunkNumPerAxis * dwChunkNumPerAxis;
            SDFChunkData ChunkData = gSdfChunkDatas[dwChunkIndex];
            
            // Chunk 不为空.
            if (ChunkData.dwModelIndexBegin != -1)
            {
                float fWeightSum = 0.0f;
                for (int32_t ix = ChunkData.dwModelIndexBegin; ix < ChunkData.dwModelIndexEnd; ++ix)
                {
                    FModelSurfaceData SurfaceData = gModelSurfaceDatas[ix];

                    float3 Center = (SurfaceData.BoxUpper + SurfaceData.BoxLower) * 0.5f;
                    if (distance(Center, HitPos) > SurfaceData.fHalfBoxDiagonal) continue;

                    float3 ClampHitPos = clamp(HitPos, SurfaceData.BoxLower, SurfaceData.BoxUpper);
                    if (any(abs(ClampHitPos - HitPos) > sdf_voxel_size)) continue;
                    
                    [unroll]
                    for (uint32_t jx = 0; jx < 6; ++jx)
                    {
                        FCardData Card = SurfaceData.Cards[jx];
                        float3 LocalHitPos = mul(float4(HitPos, 1.0f), Card.LocalMatrix).xyz;
                        
                        float2 uv = saturate(LocalHitPos.xy / Card.extent.xy + 0.5f);
                        uv.y = 1.0f - uv.y;

                        // 双线性插值权重.
                        float4 BilinearWeight = float4(
                            (1.0f - uv.x) * (1.0f - uv.y),
                            (uv.x)        * (1.0f - uv.y),
                            (1.0f - uv.x) * (uv.y),
                            (uv.x)        * (uv.y)
                        );

                        uv = (uv * dwSurfaceTextureRes + Card.AtlasOffset) / dwSurfaceAtlasRes;

                        // 击中点的深度与实际 surface 上的深度比值权重.
                        float4 SurfaceDepth = gSurfaceDepthTexture.Gather(gSurfaceSampler, uv) * Card.extent.z;
                        float4 DepthWeight;
                        [unroll]
                        for (uint32_t kx = 0; kx < 4; ++kx)
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
                                DepthWeight[kx] = 1.0f - clamp(abs(LocalHitPos.z - SurfaceDepth[kx]) - 2.0f * sdf_voxel_size, 0.0f, 1.0f);
                            }
                        }

                        // 光线与击中点的夹角权重.
                        float3 HitNormal = gSurfaceNormalTexture.SampleLevel(gSurfaceSampler, uv, 0.0f);
                        float fNormalWeight = saturate(dot(HitNormal, RayDir));
                        
                        // 总权重.
                        float fSampleWeight = dot(DepthWeight, BilinearWeight) * fNormalWeight;
                        if (fSampleWeight <= 0.0f) continue;
                        else
                        {
                            Radiance += float3(
                                dot(gSurfaceLightTexture.GatherRed(gSurfaceSampler, uv), fSampleWeight), 
                                dot(gSurfaceLightTexture.GatherGreen(gSurfaceSampler, uv), fSampleWeight), 
                                dot(gSurfaceLightTexture.GatherBlue(gSurfaceSampler, uv), fSampleWeight)
                            );
                            fWeightSum += fSampleWeight;
                        }
                    }
                }
                Radiance /= max(fWeightSum, 0.0001f);
            }

            fDistance = max(HitData.step_size + 0.5f * sdf_voxel_size, 0.0f);
        }
    }

    gOutputRadianceTexture[ThreadID.xy] = Radiance;
    gOutputDirectionDistanceTexture[ThreadID.xy] = float4(RayDir, fDistance);
}




#endif