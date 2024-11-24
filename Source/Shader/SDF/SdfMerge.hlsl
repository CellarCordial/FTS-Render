// #define THREAD_GROUP_SIZE_X 0 
// #define THREAD_GROUP_SIZE_Y 0
// #define THREAD_GROUP_SIZE_Z 0

struct FModelSdfData
{
    float4x4 CoordMatrix;

    float3 SdfLower;
    float3 SdfUpper;

    uint dwMeshSdfIndex;
};

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(THREAD_GROUP_SIZE_Z)
    
cbuffer gPassConstant : register(b0)
{
    float4x4 VoxelWorldMatrix;

    uint3 VoxelOffset;  
    float fGIMaxDistance;

    uint dwMeshSdfBegin; 
    uint dwMeshSdfEnd;
    uint dwVoxelNumExtent;
    uint dwVoxelNumPerAxis;
	uint bSurroundChunkUpdated;
};


StructuredBuffer<FModelSdfData> gModelSdfDatas : register(t0);
Texture3D<float> gModelSdfTextures[] : register(t1);
SamplerState gSampler : register(s0);

RWTexture3D<float> gGlobalSdfTexture : register(u0);

float CalcSdf(float fMinSdf, uint dwSdfIndex, float3 VoxelWorldPos);
float ReadSdf(uint3 ThreadID, uint3 VoxelID, float fMinSdf);

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, THREAD_GROUP_SIZE_Z)]
void CS(uint3 ThreadID : SV_DispatchThreadID)
{
    if (any(ThreadID >= dwVoxelNumPerAxis)) return;

    uint3 VoxelID = VoxelOffset + ThreadID;
    float3 VoxelWorldPos = mul(float4(VoxelID, 1.0f), VoxelWorldMatrix).xyz;

    float fMinSdf = fGIMaxDistance;

    for (uint ix = dwMeshSdfBegin; ix < dwMeshSdfEnd; ++ix)
    {
        fMinSdf = CalcSdf(fMinSdf, ix, VoxelWorldPos);
    }

    gGlobalSdfTexture[VoxelID] = fMinSdf;
}

float ReadSdf(uint3 ThreadID, uint3 VoxelID, float fMinSdf)
{
    bool bRead = 
        (ThreadID.x < dwVoxelNumExtent && (bSurroundChunkUpdated & 0x000001)) ||
        (ThreadID.y < dwVoxelNumExtent && (bSurroundChunkUpdated & 0x000010)) ||
        (ThreadID.z < dwVoxelNumExtent && (bSurroundChunkUpdated & 0x000100)) ||
        (ThreadID.x >= dwVoxelNumPerAxis - dwVoxelNumExtent && (bSurroundChunkUpdated & 0x001000)) ||
        (ThreadID.y >= dwVoxelNumPerAxis - dwVoxelNumExtent && (bSurroundChunkUpdated & 0x010000)) ||
        (ThreadID.z >= dwVoxelNumPerAxis - dwVoxelNumExtent && (bSurroundChunkUpdated & 0x100000));

    return bRead ? min(fMinSdf, gGlobalSdfTexture[VoxelID]) : fMinSdf;
}

float CalcSdf(float fMinSdf, uint dwSdfIndex, float3 VoxelWorldPos)
{
    FModelSdfData SdfData = gModelSdfDatas[dwSdfIndex];

    float3 WorldPosClamped = clamp(VoxelWorldPos, SdfData.SdfLower, SdfData.SdfUpper);

    float fDistanceToSdf = length(VoxelWorldPos - WorldPosClamped);
    // 到 sdf 包围盒的距离已经大于当前最小距离.
    if (fMinSdf <= fDistanceToSdf) return fMinSdf;

    float3 uvw = mul(float4(WorldPosClamped, 1.0f), SdfData.CoordMatrix).xyz;
    uvw.y = 1.0f - uvw.y;

    float sdf = gModelSdfTextures[SdfData.dwMeshSdfIndex].SampleLevel(gSampler, uvw, 0);
    // Voxel 在 MeshSdf 内.
    if (fDistanceToSdf < 0.001f) return min(sdf, fMinSdf);

    // 精度非常低.
    return min(fMinSdf, fDistanceToSdf + sdf);
}

#endif