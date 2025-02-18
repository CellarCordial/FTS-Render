#define THREAD_GROUP_NUM_X 1
#define THREAD_GROUP_NUM_Y 1

Texture2D<float2> reprojection_texture : register(t0);
Texture2D<float4> temporal_history_texture : register(t1);

RWTexture2D<float4> history_reprojection_texture : register(u0);

#if defined(THREAD_GROUP_NUM_X) && defined(THREAD_GROUP_NUM_Y)


[numthreads(THREAD_GROUP_NUM_X, THREAD_GROUP_NUM_Y, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    
}

#endif