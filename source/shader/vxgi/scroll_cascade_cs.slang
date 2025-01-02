#define THREAD_GROUP_NUM_X 1

#include "../vxgi_helper.slang"
using namespace voxel_irradiance;

ByteAddressBuffer ircache_grid_meta_buffer_ping : register(t0);
RWStructuredBuffer<IrcacheCascadeDesc> ircache_cascade_desc_buffer : register(t1);

RWByteAddressBuffer ircache_grid_meta_buffer_pong : register(u0);
RWStructuredBuffer<uint32_t> ircache_life_buffer : register(u1);
RWStructuredBuffer<float4> ircache_irradiance_buffer : register(u2);
RWByteAddressBuffer ircache_meta_buffer : register(u3);
RWStructuredBuffer<uint32_t> ircache_entry_pool_buffer : register(u4);
RWStructuredBuffer<uint32_t> ircache_entry_voxel_buffer : register(u5);

void release_grid_voxel(uint32_t voxel_index)
{
    // meta.x: 索引 (即 entry).
    // meta.y: 状态 (如是否被占用).
    uint2 meta = ircache_grid_meta_buffer_ping.Load2(voxel_index * sizeof(uint2));

    if ((meta.y & IRCACHE_ENTRY_META_OCCUPIED) != 0)
    {
        ircache_life_buffer[meta.x] = IRCACHE_ENTRY_LIFE_RECYCLED; // 弃用.

        // 清空该 entry 对应的 Irradiance.
        [unroll]
        for (uint32_t ix = 0; ix < IRCACHE_IRRADIANCE_STRIDE; ++ix)
        {
            ircache_irradiance_buffer[meta.x * IRCACHE_IRRADIANCE_STRIDE + ix] = float4(0, 0, 0, 0);
        }

        // 在 IRCACHE_META_ALLOC_COUNT 该位置存储一共分配了多少个 entry.
        // //当前分配 entry 数量 -1.
        uint32_t allocate_count = 0;
        ircache_meta_buffer.InterlockedAdd(IRCACHE_META_ALLOC_COUNT, -1, allocate_count);
        ircache_entry_pool_buffer[allocate_count - 1] = meta.x;
    }
}

#if defined(THREAD_GROUP_NUM_X)

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, 1, 1)]
void compute_shader(uint3 thread_id: SV_DispatchThreadID)
{
    uint3 voxel_id = uint3(thread_id.xy, thread_id.z % IRCACHE_CASCADE_SIZE);
    uint32_t cascade_index = thread_id.z / IRCACHE_CASCADE_SIZE;

    uint32_t voxel_index = IrcacheGridCoord(voxel_id, cascade_index).get_voxel_index();

    uint3 scroll_size = ircache_cascade_desc_buffer[cascade_index].voxels_scrolled.xyz;

    if (any(abs(voxel_id - scroll_size) >= IRCACHE_CASCADE_SIZE))
    {
        release_grid_voxel(voxel_index);
    }

    uint3 ori_voxel_id = voxel_id + scroll_size;
    if (all(scroll_size < IRCACHE_CASCADE_SIZE))
    {
        uint32_t ori_voxel_index = IrcacheGridCoord(ori_voxel_id, cascade_index).get_voxel_index();
        uint2 ori_meta = ircache_grid_meta_buffer_ping.Load2(ori_voxel_index * sizeof(uint2));
        ircache_grid_meta_buffer_pong.Store2(voxel_index * sizeof(uint2), ori_meta);

        if ((ori_meta.y & IRCACHE_ENTRY_META_OCCUPIED) != 0)
        {
            // 将上一帧的 entry 与这一帧的 entry 连接起来.
            ircache_entry_voxel_buffer[ori_meta.x] = voxel_index;
        }
    }
    else
    {
        ircache_grid_meta_buffer_pong.Store2(voxel_index * sizeof(uint2), uint2(0, 0));
    }

}

#endif