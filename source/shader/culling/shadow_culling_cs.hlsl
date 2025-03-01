// #define THREAD_GROUP_SIZE_X 1
// #define HI_Z_CULLING

#include "../common/indirect_argument.hlsl"
#include "../common/gbuffer.hlsl"
#include "../common/math.hlsl"


cbuffer pass_constants : register(b0)
{
    float4x4 shadow_view_matrix;
    float4x4 shadow_proj_matrix;
    
    uint packed_shadow_page_id;
    uint group_count;
    float near_plane;
    float far_plane;

    float shadow_orthographic_length;
    uint cluster_tirangle_num;
    uint shadow_map_resolution;
    uint hzb_resolution;
};

RWStructuredBuffer<DrawIndirectArguments> vt_shadow_draw_indirect_buffer : register(u0);
RWStructuredBuffer<uint2> vt_shadow_visible_cluster_buffer : register(u1);

StructuredBuffer<MeshClusterGroup> mesh_cluster_group_buffer : register(t0);
StructuredBuffer<MeshCluster> mesh_cluster_buffer : register(t1);
Texture2D<float> shadow_hi_z_texture : register(t2);

SamplerState linear_clamp_sampler : register(s0);

bool hierarchical_zbuffer_cull(float3 view_space_position, float radius)
{
    // 获取球体在 x 或 y 轴上投影的范围.
    float2 range_x = float2(view_space_position.x - radius, view_space_position.x + radius) * shadow_proj_matrix[0][0] / view_space_position.z;
    float2 range_y = float2(view_space_position.y - radius, view_space_position.y + radius) * shadow_proj_matrix[1][1] / view_space_position.z;;
    float4 rect = clamp(
        float4(
            float2(range_x.x, range_y.y) * float2(0.5f, -0.5f) + 0.5f,
            float2(range_x.y, range_y.x) * float2(0.5f, -0.5f) + 0.5f
        ),
        0.0f,
        1.0f
    );
    uint4 screen_rect = uint4(
        uint2(floor(rect.xy * shadow_map_resolution)),
        uint2(ceil(rect.zw * shadow_map_resolution))
    );

    // 获取包围球在 hzb 纹理上的 uv 范围.
    uint4 hzb_rect;
    hzb_rect.x = float(screen_rect.x) / shadow_map_resolution * hzb_resolution;
    hzb_rect.y = float(screen_rect.y) / shadow_map_resolution * hzb_resolution;
    hzb_rect.z = float(screen_rect.z) / shadow_map_resolution * hzb_resolution;
    hzb_rect.w = float(screen_rect.w) / shadow_map_resolution * hzb_resolution;

    // 计算覆盖区域的最大边长对应的最高有效位, 该有效位可以直接对应 hzb 的 lod,
    // 来判定包围球覆盖了多少像素, 需要指定哪一个层级来使得一个像素就能包含所有包围球覆盖的像素.
    uint max_extent = max(hzb_rect.z - hzb_rect.x, hzb_rect.w - hzb_rect.y);
    uint lod = search_most_significant_bit(max(hzb_rect.z - hzb_rect.x, hzb_rect.w - hzb_rect.y));

    // 采样覆盖区域的四个角点深度值, 取最小值.
    float2 uv = (hzb_rect.xy + 0.5f) / hzb_resolution;
    float z0 = shadow_hi_z_texture.SampleLevel(linear_clamp_sampler, uv, lod);
    float z1 = shadow_hi_z_texture.SampleLevel(linear_clamp_sampler, uv, lod, int2(1, 0));
    float z2 = shadow_hi_z_texture.SampleLevel(linear_clamp_sampler, uv, lod, int2(1, 1));
    float z3 = shadow_hi_z_texture.SampleLevel(linear_clamp_sampler, uv, lod, int2(0, 1));
    float max_z = max4(z0, z1, z2, z3);

    float cluster_near_z = view_space_position.z - radius;
    cluster_near_z = shadow_proj_matrix[2][2] + shadow_proj_matrix[3][2] / cluster_near_z;

    return cluster_near_z < max_z;
}

bool tile_cull(float3 position, float radius)
{
    return !(position.z + radius < near_plane || position.z - radius > far_plane) &&
            any(position.xy - radius - shadow_orthographic_length * 0.5f <= 0.0f) &&
            any(position.xy + radius + shadow_orthographic_length * 0.5f >= 0.0f);
}

#if defined(THREAD_GROUP_SIZE_X)


[numthreads(THREAD_GROUP_SIZE_X, 1, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint group_index = thread_id.x;
    if (group_index >= group_count) return;

    MeshClusterGroup group = mesh_cluster_group_buffer[group_index];

    float pixel_length = shadow_orthographic_length / shadow_map_resolution;
    if (pixel_length < group.max_parent_lod_error)  // check lod.
    {
        for (uint ix = 0; ix < group.cluster_count; ++ix)
        {
            uint cluster_id = group.cluster_index_offset + ix;
            MeshCluster cluster = mesh_cluster_buffer[cluster_id];
            
            // check lod.
            if (pixel_length < cluster.lod_error) return;

            float3 cluster_view_space_position = mul(
                float4(cluster.bounding_sphere.xyz, 1.0f), 
                shadow_view_matrix
            ).xyz;

            bool visible = tile_cull(cluster_view_space_position, cluster.bounding_sphere.w);
#ifdef HI_Z_CULLING
            visible = hierarchical_zbuffer_cull(cluster_view_space_position, cluster.bounding_sphere.w);
#endif
            if (visible)
            {
                uint current_pos;
                InterlockedAdd(
                    vt_shadow_draw_indirect_buffer[0].instance_count,
                    1,
                    current_pos
                );

                vt_shadow_draw_indirect_buffer[0].vertex_count = cluster_tirangle_num * 3;
                vt_shadow_visible_cluster_buffer[current_pos] = uint2(
                    cluster_id, packed_shadow_page_id
                );
            }
        }
    }
}

#endif