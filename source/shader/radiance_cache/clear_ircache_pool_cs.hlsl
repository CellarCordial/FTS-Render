#define THREAD_GROUP_NUM_X 1

#include "../common/vxgi_helper.hlsl"

RWStructuredBuffer<uint> ircache_pool_buffer : register(u0);
RWStructuredBuffer<uint> ircache_life_buffer : register(u1);

#if defined(THREAD_GROUP_NUM_X)


[numthreads(THREAD_GROUP_NUM_X, 1, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    ircache_pool_buffer[thread_id.x] = thread_id.x;
    ircache_life_buffer[thread_id.x] = IRCACHE_ENTRY_LIFE_RECYCLED;
}


#endif