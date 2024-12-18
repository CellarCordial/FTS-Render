#define THREAD_GROUP_NUM_X 1

#include "../vxgi_helper.slang"

ByteAddressBuffer ircache_meta_buffer : register(t0);
StructuredBuffer<uint> ircache_life_buffer : register(t1);

RWStructuredBuffer<uint> ircache_entry_occupy_buffer : register(u0);
RWStructuredBuffer<uint> ircache_entry_indirection_buffer : register(u1);

#if defined(THREAD_GROUP_NUM_X)

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, 1, 1)]
void compute_shader(uint3 thread_id: SV_DispatchThreadID)
{
    uint entry_index = thread_id.x;
    uint total_entry_num = ircache_meta_buffer.Load(IRCACHE_META_ENTRY_COUNT);

    uint age = ircache_life_buffer[entry_index];
    if (entry_index < total_entry_num && voxel_irradiance::is_ircache_entry_life_valid(age))
    {
        ircache_entry_indirection_buffer[ircache_entry_occupy_buffer[entry_index]] = entry_index;
    }
}


#endif