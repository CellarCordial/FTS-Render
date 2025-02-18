#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../common/light.hlsl"

cbuffer pass_constant : register(b0)
{
    float4x4 view_proj;

    uint3 divide_count;
    float half_fov_y;
    float near_z;

    uint point_light_count;
    uint spot_light_count;
};

Texture2D<float4> world_position_view_depth_texture : register(t0);
StructuredBuffer<PointLight> point_light_buffer : register(t1);
StructuredBuffer<SpotLight> spot_light_buffer : register(t2);
RWStructuredBuffer<uint> light_index_buffer : register(u0);
RWStructuredBuffer<uint2> light_cluster_buffer : register(u1);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    uint2 pixel_id = thread_id.xy;
    float3 world_position = world_position_view_depth_texture[pixel_id].xyz;
    float4 clip_position = mul(float4(world_position, 1.0f), view_proj);
    clip_position.xyz /= clip_position.w;

    uint3 cluster_id = uint3(
        (uint2)((clip_position.xy + 1.0f) * divide_count.xy / 2.0f),
        (uint)floor(log(clip_position.z / near_z) / log(1 + 2.0f * tan(half_fov_y) / divide_count.y))
    );
    uint cluster_index =
        cluster_id.z * divide_count.x * divide_count.y +
        cluster_id.y * divide_count.x +
        cluster_id.x;
        

    bool intersect_light = false;
    for (uint ix = 0; ix < point_light_count; ++ix)
    {
        PointLight light = point_light_buffer[ix];
        if (length(world_position - light.position) <= light.fall_off_end)
        {
            uint light_index;
            InterlockedAdd(light_index_buffer[0], 1, light_index);
            light_index++;

            light_index_buffer[light_index] = ix;

            if (!intersect_light)
            {
                intersect_light = true;
                light_cluster_buffer[cluster_index] = uint2(light_index, 1);
            }
            else
            {
                light_cluster_buffer[cluster_index].y++;
            }
        }
    }

    uint spot_light_offset = point_light_count;
    for (uint ix = 0; ix < spot_light_count; ++ix)
    {
        SpotLight light = spot_light_buffer[ix];

        float3 light_to_point = world_position - light.position;
        float distance = length(light_to_point);

        if (distance <= light.max_distance)
        {
            light_to_point = normalize(light_to_point);
        
            float cos_theta = dot(light_to_point, normalize(light.direction));
            float theta = acos(cos_theta);

            // 判断点是否在光锥内
            if (theta <= light.outer_angle)
            {
                uint light_index;
                InterlockedAdd(light_index_buffer[0], 1, light_index);
                light_index++;

                light_index_buffer[light_index] = spot_light_offset + ix;

                if (!intersect_light)
                {
                    intersect_light = true;
                    light_cluster_buffer[cluster_index] = uint2(light_index, 1);
                }
                else
                {
                    light_cluster_buffer[cluster_index].y++;
                }
            }
        }
    }
}

#endif
