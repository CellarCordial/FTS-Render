// #define THREAD_GROUP_SIZE_X 1

#include "../common/gbuffer.hlsl"
#include "../common/math.hlsl"
#include "../common/indirect_argument.hlsl"

cbuffer pass_constants : register(b0)
{
    float4x4 view_matrix;
    float4x4 reverse_z_proj_matrix;

    uint client_width;
    uint client_height;
    uint camera_fov_y;
    uint hzb_resolution;

    uint group_count;
    uint cluster_size;
    float near_plane;
    float far_plane;
};

RWStructuredBuffer<DrawIndirectArguments> virtual_gbuffer_draw_indirect_buffer : register(u0);
RWStructuredBuffer<uint> visible_cluster_id_buffer : register(u1);

StructuredBuffer<MeshClusterGroup> mesh_cluster_group_buffer : register(t0);
StructuredBuffer<MeshCluster> mesh_cluster_buffer : register(t1);
Texture2D hierarchical_zbuffer_texture : register(t2);

SamplerState linear_clamp_sampler : register(s0);

float2 project_sphere(float a, float z, float radius) 
{
    // 获取某点在 x 或 y 轴上投影的范围, a 是 x 或 y.
    float tangent = sqrt(a * a + z * z - radius * radius);  // 摄像机原点到包围球表面的切线长.
    float max = (z * radius - a * tangent) / (a * radius + z * tangent);
    float min = (z * radius + a * tangent) / (a * radius - z * tangent);
    return float2(min, max);
}

uint search_most_significant_bit(uint x)
{
    // 折半查找最高有效位.
    uint res = 0, t = 16, y = 0;
    y = -((x >> t) != 0 ? 1 : 0); res += y & t; x >>= y & t; t >>= 1;
    y = -((x >> t) != 0 ? 1 : 0); res += y & t; x >>= y & t; t >>= 1;
    y = -((x >> t) != 0 ? 1 : 0); res += y & t; x >>= y & t; t >>= 1;
    y = -((x >> t) != 0 ? 1 : 0); res += y & t; x >>= y & t; t >>= 1;
    y = (x >> t) != 0 ? 1 : 0; res += y;
    return res;
}

bool hierarchical_zbuffer_cull(float3 view_space_position, float radius, float4x4 reverse_z_proj_matrix)
{
    // 获取球体在 x 或 y 轴上投影的范围.
    float2 range_x = project_sphere(view_space_position.x, view_space_position.z, radius) * reverse_z_proj_matrix[0][0];
    float2 range_y = project_sphere(view_space_position.y, view_space_position.z, radius) * reverse_z_proj_matrix[1][1];
    float4 rect = clamp(
        float4(
            float2(range_x.x, range_y.x) * float2(0.5f, -0.5f) + 0.5f,
            float2(range_x.y, range_y.y) * float2(0.5f, -0.5f) + 0.5f
        ),
        0.0f,
        1.0f
    );
    uint4 screen_rect = uint4(
        uint(floor(rect.x * client_width)),
        uint(floor(rect.y * client_height)),
        uint(ceil(rect.z * client_width)),
        uint(ceil(rect.w * client_height))
    );

    // 获取包围球在 hzb 纹理上的 uv 范围.
    uint4 hzb_rect;
    uint tx = (screen_rect.x + 1) * hzb_resolution;
    uint ty = (screen_rect.y + 1) * hzb_resolution;
    hzb_rect.x = tx / client_width;
    hzb_rect.y = ty / client_height;
    hzb_rect.z = (screen_rect.z - 1) * hzb_resolution / client_width;
    hzb_rect.w = (screen_rect.w - 1) * hzb_resolution / client_height;
    if (tx % client_width == 0) hzb_rect.x--;
    if (ty % client_height == 0) hzb_rect.y--;
    if (hzb_rect.z < hzb_rect.x) hzb_rect.z = hzb_rect.x;
    if (hzb_rect.w < hzb_rect.y) hzb_rect.w = hzb_rect.y;

    // 计算覆盖区域的最大边长对应的最高有效位,
    // 判定包围球覆盖了多少像素, 需要指定哪一个层级来使得一个像素就能包含所有包围球覆盖的像素.
    uint lod = search_most_significant_bit(max(hzb_rect.z - hzb_rect.x, hzb_rect.w - hzb_rect.y));

    // 采样覆盖区域的四个角点深度值, 取最小值.
    float2 uv = (hzb_rect.xy + 0.5f) / hzb_resolution;
    float z0 = hierarchical_zbuffer_texture.SampleLevel(linear_clamp_sampler, uv, lod).r;
    float z1 = hierarchical_zbuffer_texture.SampleLevel(linear_clamp_sampler, uv, lod, int2(1, 0)).r;
    float z2 = hierarchical_zbuffer_texture.SampleLevel(linear_clamp_sampler, uv, lod, int2(1, 1)).r;
    float z3 = hierarchical_zbuffer_texture.SampleLevel(linear_clamp_sampler, uv, lod, int2(0, 1)).r;
    float min_z = min4(z0, z1, z2, z3);

    float cluster_near_z = view_space_position.z - radius;
    cluster_near_z = reverse_z_proj_matrix[2][2] + reverse_z_proj_matrix[3][2] / cluster_near_z;

    return cluster_near_z > min_z;
}

bool frustum_cull(float3 view_space_position, float radius, float4x4 reverse_z_proj_matrix)
{
    // 视锥体左右上下四个面的单位法线.
    float3 p0 = normalize(float3(-reverse_z_proj_matrix[0][0], 0, -1));
    float3 p1 = normalize(float3(reverse_z_proj_matrix[0][0], 0, -1));
    float3 p2 = normalize(float3(0, -reverse_z_proj_matrix[1][1], -1));
    float3 p3 = normalize(float3(0, reverse_z_proj_matrix[1][1], -1));

    bool visible = dot(p0, view_space_position) < radius;
    visible = visible && dot(p1, view_space_position) < radius;
    visible = visible && dot(p2, view_space_position) < radius;
    visible = visible && dot(p3, view_space_position) < radius;
    return visible;
}

bool check_lod(float3 view_space_position, float radius, float error)
{
    // 根据物体远近确定 LOD.
    float distance = max(length(view_space_position) - radius, 0.0f);   // 摄像机到物体包围球的最近距离.
    float theta = radians(camera_fov_y) / client_height;    // 每个像素点所占有的弧度值, 即像素的角分辨率.
    return distance * theta >= error;   // distance * theta 为半径乘以圆心角, 结果是弧长, 若弧长大于阈值, 则表明足够精细.
}

#if defined(THREAD_GROUP_SIZE_X)

[numthreads(THREAD_GROUP_SIZE_X, 1, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    uint group_index = thread_id.x;
    if (group_index >= group_count) return;

    MeshClusterGroup group = mesh_cluster_group_buffer[group_index];
    float3 group_view_space_position = mul(float4(group.bounding_sphere.xyz, 1.0f), view_matrix).xyz;
    if (!check_lod(group_view_space_position, group.bounding_sphere.w, group.max_parent_lod_error))
    {
        for (uint ix = 0; ix < group.cluster_count; ++ix)
        {
            uint cluster_id = group.cluster_index_offset + ix;
            MeshCluster cluster = mesh_cluster_buffer[cluster_id];
            bool visible = check_lod(
                mul(float4(cluster.lod_bounding_sphere.xyz, 1.0f), view_matrix).xyz,
                cluster.lod_bounding_sphere.w,
                cluster.lod_error
            );

            if (visible)
            {
                float3 cluster_view_space_position = mul(float4(cluster.bounding_sphere.xyz, 1.0f), view_matrix).yxz;

                // 包围球在 z 轴上的最远端不超过近平面或最近端超过远平面, 则将该 cluster 剔除.
                if (
                    cluster_view_space_position.z + cluster.bounding_sphere.w < near_plane ||
                    cluster_view_space_position.z - cluster.bounding_sphere.w > far_plane
                )
                    return;

                // 判定是否在视锥体内, 以及从 hzb 中采样深度值进行比较.
                visible = frustum_cull(cluster_view_space_position, cluster.bounding_sphere.w, reverse_z_proj_matrix) && 
                          cluster_view_space_position.z - cluster.bounding_sphere.w > near_plane &&
                          cluster_view_space_position.z + cluster.bounding_sphere.w < far_plane &&
                          hierarchical_zbuffer_cull(
                            cluster_view_space_position,
                            cluster.bounding_sphere.w,
                            reverse_z_proj_matrix
                          );
                
                if (visible)
                {
                    uint current_pos;
                    InterlockedAdd(
                        virtual_gbuffer_draw_indirect_buffer[0].instance_count,
                        1,
                        current_pos
                    );

                    virtual_gbuffer_draw_indirect_buffer[0].vertex_count = cluster_size * 3;
                    visible_cluster_id_buffer[current_pos] = cluster_id;
                }
            }
        }
    }
}


#endif