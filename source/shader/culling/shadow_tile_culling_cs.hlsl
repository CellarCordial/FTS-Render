// #define THREAD_GROUP_SIZE_X 1

#include "../common/indirect_argument.hlsl"
#include "../common/gbuffer.hlsl"
#include "../common/math.hlsl"


cbuffer pass_constants : register(b0)
{
    uint shadow_tile_num;
    uint group_count;
    float near_plane;
    float far_plane;

    uint cluster_size;
    float3 frustum_top_normal;

    float3 frustum_right_normal;
};

struct ShadowTileInfo
{
    uint2 id;
    float4x4 view_matrix;
};

struct ShadowVisibleInfo
{
    uint cluster_id;
    uint2 tile_id;
};

RWStructuredBuffer<DrawIndirectArguments> virtual_shadow_draw_indirect_buffer : register(u0);
RWStructuredBuffer<ShadowVisibleInfo> virtual_shadow_visible_cluster_id_buffer : register(u1);

StructuredBuffer<MeshClusterGroup> mesh_cluster_group_buffer : register(t0);
StructuredBuffer<MeshCluster> mesh_cluster_buffer : register(t1);
StructuredBuffer<ShadowTileInfo> shadow_tile_info_buffer : register(t2);

#if defined(THREAD_GROUP_SIZE_X)


[numthreads(THREAD_GROUP_SIZE_X, 1, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint group_index = thread_id.x;
    if (group_index >= group_count) return;

    MeshClusterGroup group = mesh_cluster_group_buffer[group_index];

    // 使用第二高等级的 LOD 进行剔除.
    if (group.mip_level != group.max_mip_level - 1) return;

    for (uint ix = 0; ix < group.cluster_count; ++ix)
    {
        uint cluster_id = group.cluster_index_offset + ix;
        MeshCluster cluster = mesh_cluster_buffer[cluster_id];

        for (uint ix = 0; ix < shadow_tile_num; ++ix)
        {
            ShadowTileInfo tile_info = shadow_tile_info_buffer[ix];

            float3 view_space_cluster_position = mul(float4(cluster.bounding_sphere.xyz, 1.0f), tile_info.view_matrix).xyz;
            float distance = view_space_cluster_position.z;
            bool visible = !(distance + cluster.bounding_sphere.w < near_plane || distance - cluster.bounding_sphere.w > far_plane) &&
                            distance - cluster.bounding_sphere.w > near_plane && distance + cluster.bounding_sphere.w < far_plane;

            if (visible)
            {
                visible = dot(frustum_top_normal, view_space_cluster_position) < cluster.bounding_sphere.w;
                visible = visible && dot(-frustum_top_normal, view_space_cluster_position) < cluster.bounding_sphere.w;
                visible = visible && dot(frustum_right_normal, view_space_cluster_position) < cluster.bounding_sphere.w;
                visible = visible && dot(-frustum_right_normal, view_space_cluster_position) < cluster.bounding_sphere.w;
            }

            if (visible)
            {
                ShadowVisibleInfo info;
                info.cluster_id = cluster_id;
                info.tile_id = tile_info.id;

                uint current_pos;
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