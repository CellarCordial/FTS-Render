// #define THREAD_GROUP_SIZE_X 0 
// #define THREAD_GROUP_SIZE_Y 0
// #define THREAD_GROUP_SIZE_Z 0


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(THREAD_GROUP_SIZE_Z)

cbuffer gPassConstant : register(b0)
{
    float fGIMaxDistance;
    float3 PAD;
};

RWTexture3D<float> gGlobalSdfTexture : register(u0);

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, THREAD_GROUP_SIZE_Z)]
void CS(uint3 ThreadID : SV_DispatchThreadID)
{
    gGlobalSdfTexture[ThreadID] = fGIMaxDistance + 1.0f;
}

#endif