#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../common/octahedral.hlsl"

cbuffer pass_constants : register(b0)
{
    uint ray_count_per_probe;
    float history_alpha;
    float history_gamma;

    uint first_frame;
    float depth_sharpness;
};

Texture2D<float4> ddgi_direction_distance_texture : register(t0);

RWTexture2D<float2> ddgi_volume_depth_texture : register(u0);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint group_index : SV_GroupIndex)
{
    uint2 ddgi_volume_depth_texture_resolution;
    ddgi_volume_depth_texture.GetDimensions(
        ddgi_volume_depth_texture_resolution.x,
        ddgi_volume_depth_texture_resolution.y
    );

    if (any(thread_id.xy >= ddgi_volume_depth_texture_resolution)) return;
    uint2 single_depth_uv = thread_id.xy + 1;
    uint2 depth_uv = group_id.xy * (ddgi_volume_depth_texture_resolution + 2) + single_depth_uv; 

    uint probe_index = group_index;
    float weight_sum = 0.0f;
    float2 depth_sum = float2(0.0f, 0.0f);
    for (uint ix = 0; ix < ray_count_per_probe; ++ix)
    {
        uint2 uv = uint2(ix, probe_index);
        float4 ray_direction_distance = ddgi_direction_distance_texture[uv];

        float2 normalized_depth_uv = (((float2)single_depth_uv + 0.5f) / (float2)ddgi_volume_depth_texture_resolution) * 2.0f - 1.0f;
        float3 pixel_direction = octahedron_to_unit_vector(normalized_depth_uv);

        float weight = pow(max(0.0f, dot(pixel_direction, ray_direction_distance.xyz)), depth_sharpness);
        depth_sum += float2(ray_direction_distance.w * weight, ray_direction_distance.w * ray_direction_distance.w * weight);
        weight_sum += weight;
    }

    if (weight_sum > 0.00001f)
    {
        depth_sum *= 1.0f / (2.0f * weight_sum);
    }
    if (first_frame == 0)
    {
        float inv_history_gamma = 1.0f / history_gamma;
        float2 old_irradiance = pow(ddgi_volume_depth_texture[depth_uv], inv_history_gamma);
        depth_sum = pow(depth_sum, inv_history_gamma);
        depth_sum = pow(lerp(depth_sum, old_irradiance, history_alpha), history_gamma);
    }

    ddgi_volume_depth_texture[depth_uv] = depth_sum;
}




#endif