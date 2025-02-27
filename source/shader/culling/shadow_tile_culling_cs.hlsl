// #define THREAD_GROUP_SIZE_X 1

#include "../common/indirect_argument.hlsl"
#include "../common/gbuffer.hlsl"
#include "../common/math.hlsl"


cbuffer pass_constants : register(b0)
{
    float4x4 shadow_view_matrix;
    
    uint group_count;
    float far_plane;
    float near_plane;
    uint packed_shadow_page_id;

    uint cluster_tirangle_num;
    float shadow_orthographic_length;
};

RWStructuredBuffer<DrawIndirectArguments> vt_shadow_draw_indirect_buffer : register(u0);
RWStructuredBuffer<uint2> vt_shadow_visible_cluster_buffer : register(u1);

StructuredBuffer<MeshClusterGroup> mesh_cluster_group_buffer : register(t0);
StructuredBuffer<MeshCluster> mesh_cluster_buffer : register(t1);

#if defined(THREAD_GROUP_SIZE_X)

bool tile_cull(float3 position, float radius)
{
    return !(position.z + radius < near_plane || position.z - radius > far_plane) &&
            any(position.xy - radius - shadow_orthographic_length * 0.5f <= 0.0f) &&
            any(position.xy + radius + shadow_orthographic_length * 0.5f >= 0.0f);
}


[numthreads(THREAD_GROUP_SIZE_X, 1, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint group_index = thread_id.x;
    if (group_index >= group_count) return;

    MeshClusterGroup group = mesh_cluster_group_buffer[group_index];

    if (group.mip_level != max(0, group.max_mip_level - 1)) return;

    for (uint ix = 0; ix < group.cluster_count; ++ix)
    {
        uint cluster_id = group.cluster_index_offset + ix;
        MeshCluster cluster = mesh_cluster_buffer[cluster_id];

        float3 cluster_view_space_position = mul(
            float4(cluster.bounding_sphere.xyz, 1.0f), 
            shadow_view_matrix
        ).xyz;

        if (tile_cull(cluster_view_space_position, cluster.bounding_sphere.w))
        {
            uint current_pos;
            InterlockedAdd(
                vt_shadow_draw_indirect_buffer[0].instance_count,
                1,
                current_pos
            );
            current_pos++;

            vt_shadow_draw_indirect_buffer[0].vertex_count = cluster_tirangle_num * 3;
            vt_shadow_visible_cluster_buffer[current_pos] = uint2(
                cluster_id, packed_shadow_page_id
            );
        }
    }
}

#endif