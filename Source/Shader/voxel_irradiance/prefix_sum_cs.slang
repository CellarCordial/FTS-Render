#define THREAD_GROUP_NUM_X 1
#define PREFIX_SCAN_0
#define PREFIX_SCAN_1

#include "../voxel_ircache_helper.slang"

ByteAddressBuffer input_buffer : register(t0);
RWByteAddressBuffer output_buffer : register(u0);

#if defined(THREAD_GROUP_NUM_X) && (defined(PREFIX_SCAN_0) || defined(PREFIX_SCAN_1))

#define SHARED_SIZE THREAD_GROUP_NUM_X * 2

groupshared uint shared_buffer[SHARED_SIZE];

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, 1, 1)]
void CS(uint3 thread_id: SV_DispatchThreadID, uint3 group_id : SV_GroupID)
{
    uint index = thread_id.x;
    uint offset = group_id.x * SHARED_SIZE;

#if defined(PREFIX_SCAN_0)
    uint2 input = input_buffer.Load2((index * 2 + offset) * sizeof(uint));
    shared_buffer[index * 2] = input.x;
    shared_buffer[index * 2 + 1] = input.y;
#elif defined(PREFIX_SCAN_1)
    shared_buffer[index * 2] = input_buffer.Load((index * 2 * SHARED_SIZE + SHARED_SIZE - 1) * sizeof(uint));
    shared_buffer[index * 2 + 1] = input_buffer.Load(((index * 2 + 1) * SHARED_SIZE + SHARED_SIZE - 1) * sizeof(uint));
#endif


    GroupMemoryBarrierWithGroupSync();

    // Reduction, 并行归约算法.
    uint step_size = uint(log2(THREAD_GROUP_NUM_X)) + 1;
    [unroll]
    for (uint ix = 0; ix < step_size; ++ix)
    {
        uint mask = (1 << ix) - 1;
        uint read_index = ((index >> ix) << (ix + 1)) + mask;
        uint write_index = read_index + 1 + (index & mask);

        shared_buffer[write_index] += shared_buffer[read_index];

        GroupMemoryBarrierWithGroupSync();
    }

#if defined(PREFIX_SCAN_0)
    output_buffer.Store2((index * 2 + offset) * sizeof(uint), uint2(shared_buffer[index * 2], shared_buffer[index * 2 + 1]));
#elif defined(PREFIX_SCAN_1)
    output_buffer.Store2((index * 2 * sizeof(uint)) * sizeof(uint), uint2(shared_buffer[index * 2], shared_buffer[index * 2 + 1]));
#endif
}


#endif