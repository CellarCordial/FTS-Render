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
    
    FDDGIVolumeData VolumeData;
    FGlobalSdfData SdfData;    
};

struct FSdfChunkData
{
    int dwModelIndexBegin;
    int dwModelIndexEnd;
};

struct FModelSurfaceData
{
    float4x4 LocalMatrix;
    float3 Position;
    float3 BoxExtent;
    float fBoxDiagonal;

    uint2 SurfaceCardOffset;
};

Texture3D<float> gGlobalSDF : register(t0);
SamplerState gSDFSampler : register(s0);

Texture2D<float3> gSkyLUT : register(t1);
SamplerState gSkySampler : register(s1);

Texture2D<float3> gRadianceTexture : register(t2);
Texture2D<float4> gDirectionDistanceTexture : register(t3);

StructuredBuffer<FSdfChunkData> gSdfChunkDatas : register(t4);
StructuredBuffer<FModelSurfaceData> gModelSurfaceDatas : register(t4);


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

    FSdfHitData HitData = TraceGlobalSdf(RayOri, RayDir, SdfData, true, gGlobalSDF, gSDFSampler);

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
            
            if (ChunkData.dwModelIndexBegin != -1)
            {
                for (int ix = ChunkData.dwModelIndexBegin; ix < ChunkData.dwModelIndexEnd; ++ix)
                {
                    FModelSurfaceData SurfaceData = gModelSurfaceDatas[ix];
                    if (distance(SurfaceData.Position, HitPos) > SurfaceData.fBoxDiagonal) continue;

                    float3 LocalHitPos = mul(float4(HitPos, 1.0f), SurfaceData.LocalMatrix).xyz;
                    if (any(abs(LocalHitPos) > SurfaceData.BoxExtent * 0.5f + fSdfVoxelSize)) continue;
                    
                    
                }
            }

        }
    }

    gRadianceTexture[ThreadID.xy] = Radiance;
    gDirectionDistanceTexture[ThreadID.xy] = float4(RayDir, fDistance);
}




#endif