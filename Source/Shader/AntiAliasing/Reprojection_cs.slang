#define THREAD_GROUP_NUM_X 1
#define THREAD_GROUP_NUM_Y 1

cbuffer gPassConstants : register(b0)
{
    float4x4 PrevViewToClip;
    float2 FinalTextureSize;
};

Texture2D<float3> gVelocityTexture : register(t0);
Texture2D<float3> gPositionVTexture : register(t1);

RWTexture2D<float2> gOutputReprojectionTexture : register(u0);


#if defined(THREAD_GROUP_NUM_X) && defined(THREAD_GROUP_NUM_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, THREAD_GROUP_NUM_Y, 1)]
void CS(uint3 ThreadID : SV_DispatchThreadID)
{
    float2 UV = float2(
        (ThreadID.x + 0.5f) / FinalTextureSize.x,
        (ThreadID.y + 0.5f) / FinalTextureSize.y
    );

    float3 PositionV = gPositionVTexture[ThreadID.xy];
    float3 PrevPositionV = PositionV + gVelocityTexture[ThreadID.xy];
    float2 PrevPositionC = mul(float4(PrevPositionV, 1.0f), PrevViewToClip).xy;
    float2 PrevUV = PrevPositionC * float2(0.5f, -0.5f) + 0.5f;

    // 确保 UV 坐标在纹理精度 R16G16_SNORM 的范围内.
    float2 UVOffset = floor((PrevUV - UV) * 32767.0 + 0.5) / 32767.0;

    gOutputReprojectionTexture[ThreadID.xy] = UVOffset;
}


#endif