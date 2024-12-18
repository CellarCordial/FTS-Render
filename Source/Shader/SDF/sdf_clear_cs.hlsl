// #define THREAD_GROUP_SIZE_X 0 
// #define THREAD_GROUP_SIZE_Y 0
// #define THREAD_GROUP_SIZE_Z 0


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(THREAD_GROUP_SIZE_Z)
    
cbuffer gPassConstant : register(b0)
{
    float4x4 VoxelWorldMatrix;

    uint3 VoxelOffset;  
    float fGIMaxDistance;
    
    uint dwModelSdfBegin; 
    uint dwModelSdfEnd;
};

RWTexture3D<float> gGlobalSdfTexture : register(u0);

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, THREAD_GROUP_SIZE_Z)]
void compute_shader(uint3 ThreadID : SV_DispatchThreadID)
{
    uint3 VoxelID = VoxelOffset + ThreadID;
    gGlobalSdfTexture[uint3(VoxelID.x, VoxelID.y, VoxelID.z)] = 0.0f;
}

#endif
