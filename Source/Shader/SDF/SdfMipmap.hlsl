// #define THREAD_GROUP_SIZE_X 0 
// #define THREAD_GROUP_SIZE_Y 0
// #define THREAD_GROUP_SIZE_Z 0

cbuffer gPassConstant : register(b0)
{
    uint gGlobalSdfRes;
    uint dwMipmapScale;
};

Texture3D<float> gGlobalSdfTexture : register(t0);
RWTexture3D<float> gGlobalSdfTextureMip : register(u0);
SamplerState gSampler : register(s0);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(THREAD_GROUP_SIZE_Z)

float SampleSdf(uint3 uvw, int3 Offset)
{
    uint3 uvw = clamp(uvw + Offset, uint3(0, 0, 0), uint3(gGlobalSdfRes, gGlobalSdfRes, gGlobalSdfRes));
    return gGlobalSdfTexture.SampleLevel(gSampler, uvw, 0);
}

[threadsnum(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, THREAD_GROUP_SIZE_Z)]
void CS(uint3 ThreadID : SV_DispatchThreadID)
{
    uint3 uvw = ThreadID * dwMipmapScale;

    float sdf = SampleSdf(uvw, int3(0, 0, 0));

    int3 Offsets[6] = 
    {
        int3(1, 0, 0),
        int3(0, 1, 0),
        int3(0, 0, 1),
        int3(-1, 0, 0),
        int3(0, -1, 0),
        int3(0, 0, -1)
    };

    [unroll]
    for (uint ix = 0; ix < 6; ++ix)
    {
        sdf = min(sdf, SampleSdf(uvw, Offsets[ix]));
    }

    gGlobalSdfTextureMip[ThreadID] = sdf;
}

#endif