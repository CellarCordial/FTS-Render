// #define THREAD_GROUP_SIZE_X 1

#include "../common/indirect_argument.slang"
#include "../common/gbuffer.slang"
#include "../common/math.slang"

[vk_binding(0, 0)]
cbuffer pass_constants : register(b0)
{
    uint32_t shadow_tile_num;
    uint32_t group_count;
    float near_plane;
    float far_plane;

    uint32_t cluster_size;
};

struct ShadowTileInfo
{
    uint2 id;
    float4x4 view_matrix;
    float3 frustum_top_normal;
    float3 frustum_right_normal;
};

struct ShadowVisibleInfo
{
    uint32_t cluster_id;
    uint2 tile_id;
};

[vk_binding(1, 0)] RWStructuredBuffer<DrawIndirectArguments> virtual_shadow_draw_indirect_buffer : register(u0);
[vk_binding(2, 0)] RWStructuredBuffer<ShadowVisibleInfo> virtual_shadow_visible_cluster_id_buffer : register(u1);

[vk_binding(3, 0)] StructuredBuffer<MeshClusterGroup> mesh_cluster_group_buffer : register(t0);
[vk_binding(4, 0)] StructuredBuffer<MeshCluster> mesh_cluster_buffer : register(t1);
[vk_binding(5, 0)] StructuredBuffer<ShadowTileInfo> shadow_tile_info_buffer : register(t2);

#if defined(THREAD_GROUP_SIZE_X)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, 1, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint32_t group_index = thread_id.x;
    if (group_index >= group_count) return;

    MeshClusterGroup group = mesh_cluster_group_buffer[group_index];

    // 使用第二高等级的 LOD 进行剔除.
    if (group.mip_level != group.max_mip_level - 1) return;

    for (uint32_t ix = 0; ix < group.cluster_count; ++ix)
    {
        uint32_t cluster_id = group.cluster_index_offset + ix;
        MeshCluster cluster = mesh_cluster_buffer[cluster_id];

        for (uint32_t ix = 0; ix < shadow_tile_num; ++ix)
        {
            ShadowTileInfo tile_info = shadow_tile_info_buffer[ix];

            float3 view_space_cluster_position = mul(float4(cluster.bounding_sphere.xyz, 1.0f), tile_info.view_matrix).xyz;
            float distance = view_space_cluster_position.z;
            bool visible = !(distance + cluster.bounding_sphere.w < near_plane || distance - cluster.bounding_sphere.w > far_plane) &&
                            distance - cluster.bounding_sphere.w > near_plane && distance + cluster.bounding_sphere.w < far_plane;

            if (visible)
            {
                visible = dot(tile_info.frustum_top_normal, view_space_cluster_position) < cluster.bounding_sphere.w;
                visible = visible && dot(-tile_info.frustum_top_normal, view_space_cluster_position) < cluster.bounding_sphere.w;
                visible = visible && dot(tile_info.frustum_right_normal, view_space_cluster_position) < cluster.bounding_sphere.w;
                visible = visible && dot(-tile_info.frustum_right_normal, view_space_cluster_position) < cluster.bounding_sphere.w;
            }

            if (visible)
            {
                ShadowVisibleInfo info;
                info.cluster_id = cluster_id;
                info.tile_id = tile_info.id;

                uint32_t current_pos;
                InterlockedAdd(
                    virtual_shadow_draw_indirect_buffer[0].instance_count,
                    1,
                    current_pos
                );
                current_pos++;

                virtual_shadow_draw_indirect_buffer[0].vertex_count = cluster_size;
                virtual_shadow_visible_cluster_id_buffer[current_pos] = info;
            }
        }
    }
}

#endif