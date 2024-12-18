#define THREAD_GROUP_NUM_X 1

#include "../vxgi_helper.slang"
using namespace voxel_irradiance;

StructuredBuffer<uint> ircache_entry_indirection_buffer : register(t0);
RWByteAddressBuffer ircache_meta_buffer : register(t1);
StructuredBuffer<float4> ircache_irradiance_buffer : register(t2);

RWStructuredBuffer<float4> ircache_auxiliary_buffer : register(u0);

#if defined(THREAD_GROUP_NUM_X)

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, 1, 1)]
void compute_shader(uint3 thread_id: SV_DispatchThreadID)
{
    uint entry_index = ircache_entry_indirection_buffer[thread_id.x];
    uint total_allocate_num = ircache_meta_buffer.Load(IRCACHE_META_TRACING_ALLOC_COUNT);

    // 是否应该 reset.
    if (all(ircache_irradiance_buffer[entry_index * IRCACHE_IRRADIANCE_STRIDE]))
    {
        for (uint ix = 0; ix < IRCACHE_AUXILIARY_STRIDE; ++ix)
        {
            ircache_auxiliary_buffer[entry_index * IRCACHE_AUXILIARY_STRIDE + ix] = float4(0, 0, 0, 0);
        }
    }
}

#endif