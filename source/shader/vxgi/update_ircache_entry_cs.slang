#define THREAD_GROUP_NUM_X 1

#include "../vxgi_helper.slang"
using namespace voxel_irradiance;

StructuredBuffer<uint32_t> ircache_entry_voxel_buffer : register(t0);
RWStructuredBuffer<CompressedVertex> ircache_reprojection_proposal_buffer : register(t1);

RWStructuredBuffer<uint32_t> ircache_life_buffer : register(u0);
RWByteAddressBuffer ircache_meta_buffer : register(u1);
RWStructuredBuffer<float4> ircache_irradiance_buffer : register(u2);
RWStructuredBuffer<uint32_t> ircache_entry_pool_buffer : register(u3);
RWStructuredBuffer<CompressedVertex> ircache_spatial_buffer : register(u4);
RWStructuredBuffer<uint32_t> ircache_reprojection_proposal_count_buffer : register(u5);
RWStructuredBuffer<uint32_t> ircache_entry_occupy_buffer : register(u6);

void update_ircache_entry(uint32_t entry_index)
{
    uint32_t entry_age = ircache_life_buffer[entry_index];
    uint32_t new_age = entry_age + 1;

    // 判断当前 entry age 是否合法.
    if (is_ircache_entry_life_valid(new_age))
    {
        ircache_life_buffer[entry_index] = new_age;

        // 更改该 voxel 的 entry flag，如果为 IRCACHE_ENTRY_META_JUST_ALLOCATED 状态改为 0 状态，如果是 IRCACHE_ENTRY_META_OCCUPIED 则不变.
        uint32_t voxel_index = ircache_entry_voxel_buffer[entry_index];
        ircache_meta_buffer.InterlockedAdd(voxel_index * sizeof(uint2) + sizeof(uint32_t), ~IRCACHE_ENTRY_META_JUST_ALLOCATED);
    }
    else
    {
        ircache_life_buffer[entry_index] = IRCACHE_ENTRY_LIFE_RECYCLED; // 弃用.

        // 清空该 entry 对应的 Irradiance.
        [unroll]
        for (uint32_t ix = 0; ix < IRCACHE_IRRADIANCE_STRIDE; ++ix)
        {
            ircache_irradiance_buffer[entry_index * IRCACHE_IRRADIANCE_STRIDE + ix] = float4(0, 0, 0, 0);
        }

        // //当前分配 entry 数量 -1.
        uint32_t allocate_count = 0;
        ircache_meta_buffer.InterlockedAdd(IRCACHE_META_ALLOC_COUNT, -1, allocate_count);
        ircache_entry_pool_buffer[allocate_count - 1] = entry_index;
        // 更改该cell index的entry flag为0状态

        // 更改该 voxel 的 entry flag 为 0 状态.
        uint32_t voxel_index = ircache_entry_voxel_buffer[entry_index];
        ircache_meta_buffer.InterlockedAdd(
            voxel_index * sizeof(uint2) + sizeof(uint32_t), 
            ~(IRCACHE_ENTRY_META_OCCUPIED | IRCACHE_ENTRY_META_JUST_ALLOCATED)
        );
    }
}



#if defined(THREAD_GROUP_NUM_X)

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, 1, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    uint32_t entry_index = thread_id.x;
    uint32_t total_entry_num = ircache_meta_buffer.Load(IRCACHE_META_ALLOC_COUNT);

    if (entry_index > (total_entry_num + THREAD_GROUP_NUM_X - 1) / THREAD_GROUP_NUM_X * THREAD_GROUP_NUM_X) return;

    uint32_t age = ircache_life_buffer[entry_index];

    if (entry_index < total_entry_num)
    {
        if (age != IRCACHE_ENTRY_LIFE_RECYCLED) // 是否需要更新.
        {
            update_ircache_entry(entry_index);
        }

        ircache_spatial_buffer[entry_index] = ircache_reprojection_proposal_buffer[entry_index];
        ircache_reprojection_proposal_count_buffer[entry_index] = 0;
    }
    else
    {
        ircache_spatial_buffer[entry_index] = CompressedVertex::invalid();
    }

    uint32_t valid = (entry_index < total_entry_num && is_ircache_entry_life_valid(age)) ? 1 : 0;
    ircache_entry_occupy_buffer[entry_index] = valid;
}

#endif