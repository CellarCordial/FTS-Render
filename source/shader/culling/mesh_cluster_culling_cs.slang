#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../gbuffer.slang"
#include "../math_helper.slang"
#include "../indirect_argument.slang"


cbuffer pass_constants : register(b0)
{
    float4x4 view_matrix;
    float4x4 proj_matrix;

    uint32_t client_width;
    uint32_t client_height;
    uint32_t camera_fov_y;
    uint32_t hzb_resolution;

    uint32_t group_count;
    uint32_t cluster_size;
};

RWStructuredBuffer<DrawIndirectArguments> draw_indirect_arguments_buffer : register(u0);
RWStructuredBuffer<uint32_t> visible_cluster_id_buffer : register(u1);

StructuredBuffer<MeshClusterGroup> mesh_cluster_group_buffer : register(t0);
StructuredBuffer<MeshCluster> mesh_cluster_buffer : register(t1);
Texture2D hierarchical_zbuffer : register(t2);

SamplerState linear_clamp_sampler : register(s0);

bool hierarchical_zbuffer_cull(float3 view_space_position, float radius, float4x4 proj_matrix);
bool frustum_cull(float3 view_space_position, float radius, float4x4 proj_matrix);
bool check_lod(float3 view_space_position, float radius, float error);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    uint32_t group_index = thread_id.x;
    if (group_index >= group_count) return;

    MeshClusterGroup group = mesh_cluster_group_buffer[group_index];
    float3 group_view_space_position = mul(float4(group.lod_bounding_sphere.xyz, 1.0f), view_matrix).xyz;
    if (!check_lod(group_view_space_position, group.lod_bounding_sphere.w, group.max_parent_lod_error))
    {
        for (uint32_t ix = 0; ix < group.cluster_count; ++ix)
        {
            uint32_t cluster_id = group.cluster_index_offset + ix;
            MeshCluster cluster = mesh_cluster_buffer[cluster_id];
            bool visible = check_lod(
                mul(float4(cluster.lod_bounding_sphere.xyz, 1.0f), view_matrix).xyz,
                cluster.lod_bounding_sphere.w,
                cluster.lod_error
            );

            if (visible)
            {
                float3 cluster_view_space_position =
                    mul(float4(cluster.bounding_sphere.xyz, 1.0f), view_matrix).yxz;
                if (cluster_view_space_position.z <= cluster.bounding_sphere.w)
                {
                    visible = frustum_cull(cluster_view_space_position, cluster.bounding_sphere.w, proj_matrix) &&
                              cluster_view_space_position.z + cluster.bounding_sphere.w < -0.1f &&
                              hierarchical_zbuffer_cull(
                                  cluster_view_space_position, 
                                  cluster.bounding_sphere.w, 
                                  proj_matrix
                              );
                    if (visible)
                    {
                        uint32_t current_pos;
                        InterlockedAdd(
                            draw_indirect_arguments_buffer[0].instance_count,
                            1,
                            current_pos
                        );
                        draw_indirect_arguments_buffer[0].vertex_count = cluster_size;
                        visible_cluster_id_buffer[current_pos] = cluster_id;
                    }
                }
            }
        }
    }
}

float2 project_sphere(float x_y, float z, float radius) {
    float tangent = sqrt(x_y * x_y + z * z - radius * radius);
    float max = (z * radius - x_y * tangent) / (x_y * radius + z * tangent);
    float min = (z * radius + x_y * tangent) / (x_y * radius - z * tangent);
    return float2(min, max);
}

uint32_t search_most_significant_bit(uint32_t x)
{
    // 折半查找.
    uint32_t res = 0, t = 16, y = 0;
    y = -((x >> t) != 0 ? 1 : 0), res += y & t, x >>= y & t, t >>= 1;
    y = -((x >> t) != 0 ? 1 : 0), res += y & t, x >>= y & t, t >>= 1;
    y = -((x >> t) != 0 ? 1 : 0), res += y & t, x >>= y & t, t >>= 1;
    y = -((x >> t) != 0 ? 1 : 0), res += y & t, x >>= y & t, t >>= 1;
    y = (x >> t) != 0 ? 1 : 0, res += y;
    return res;
}

bool hierarchical_zbuffer_cull(float3 view_space_position, float radius, float4x4 proj_matrix)
{
    float2 range_x = project_sphere(view_space_position.x, view_space_position.z, radius) * proj_matrix[0][0];
    float2 range_y = project_sphere(view_space_position.y, view_space_position.z, radius) * proj_matrix[1][1];
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

    uint4 hzb_rect;

    uint32_t tx = (screen_rect.x + 1) * hzb_resolution;
    uint32_t ty = (screen_rect.y + 1) * hzb_resolution;
    hzb_rect.x = tx / client_width;
    hzb_rect.y = ty / client_height;
    hzb_rect.z = (screen_rect.z - 1) * hzb_resolution / client_width;
    hzb_rect.w = (screen_rect.w - 1) * hzb_resolution / client_height;
    if (tx % client_width == 0) hzb_rect.x--;
    if (ty % client_height == 0) hzb_rect.y--;
    if (hzb_rect.z < hzb_rect.x) hzb_rect.z = hzb_rect.x;
    if (hzb_rect.w < hzb_rect.y) hzb_rect.w = hzb_rect.y;

    uint32_t lod = search_most_significant_bit(max(hzb_rect.z - hzb_rect.x, hzb_rect.w - hzb_rect.y));

    float2 uv = (hzb_rect.xy + 0.5f) / hzb_resolution;
    float z0 = hierarchical_zbuffer.SampleLevel(linear_clamp_sampler, uv, lod).r;
    float z1 = hierarchical_zbuffer.SampleLevel(linear_clamp_sampler, uv, lod, int2(1, 0)).r;
    float z2 = hierarchical_zbuffer.SampleLevel(linear_clamp_sampler, uv, lod, int2(1, 1)).r;
    float z3 = hierarchical_zbuffer.SampleLevel(linear_clamp_sampler, uv, lod, int2(0, 1)).r;

    float min_z = min4(z0, z1, z2, z3);
    float near_z = view_space_position.z + radius;
    near_z = -1.0f / near_z;

    return near_z > min_z;
}

bool frustum_cull(float3 view_space_position, float radius, float4x4 proj_matrix)
{
    float3 p0 = normalize(float3(proj_matrix[0][0], 0, 1));
    float3 p1 = normalize(float3(-proj_matrix[0][0], 0, 1));
    float3 p2 = normalize(float3(0, proj_matrix[1][1], 1));
    float3 p3 = normalize(float3(0, -proj_matrix[1][1], 1));

    bool visiable = dot(p0, view_space_position) < radius;
    visiable = visiable && dot(p1, view_space_position) < radius;
    visiable = visiable && dot(p2, view_space_position) < radius;
    visiable = visiable && dot(p3, view_space_position) < radius;
    return visiable;
}

bool check_lod(float3 view_space_position, float radius, float error)
{
    float distance = max(length(view_space_position) - radius, 0.0f);
    float theta = radians(camera_fov_y) / client_height;
    return distance * theta >= error;
}

#endif