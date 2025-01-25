#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../common/octahedral.slang"

cbuffer pass_constants : register(b0)
{
    uint32_t ray_count_per_probe;
    float history_alpha;
    float history_gamma;

    uint32_t first_frame;
    float depth_sharpness;
};

Texture2D<float3> ddgi_radiance_texture : register(t0);
Texture2D<float4> ddgi_direction_distance_texture : register(t1);

RWTexture2D<float3> ddgi_volume_irradiance_texture : register(u0);



#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID, uint3 group_id : SV_GroupID, uint32_t group_index : SV_GroupIndex)
{
    uint2 ddgi_volume_irradiance_texture_resolution;
    ddgi_volume_irradiance_texture.GetDimensions(
        ddgi_volume_irradiance_texture_resolution.x,
        ddgi_volume_irradiance_texture_resolution.y
    );

    if (any(thread_id.xy >= ddgi_volume_irradiance_texture_resolution)) return;
    uint2 single_irradiance_uv = thread_id.xy + 1;
    uint2 irradiance_uv = group_id.xy * (ddgi_volume_irradiance_texture_resolution + 2) + single_irradiance_uv; 

    uint32_t probe_index = group_index;
    float weight_sum = 0.0f;
    float3 irradiance_sum = float3(0.0f, 0.0f, 0.0f);
    for (uint32_t ix = 0; ix < ray_count_per_probe; ++ix)
    {
        uint2 uv = uint2(ix, probe_index);
        float3 radiance = ddgi_radiance_texture[uv];
        float3 ray_direction = ddgi_direction_distance_texture[uv].xyz;

        float2 normalized_irradiance_uv = (((float2)single_irradiance_uv + 0.5f) / (float2)ddgi_volume_irradiance_texture_resolution) * 2.0f - 1.0f;
        float3 pixel_direction = octahedron_to_unit_vector(normalized_irradiance_uv);

        float weight = max(0.0f, dot(pixel_direction, ray_direction));
        irradiance_sum += radiance * weight;
        weight_sum += weight;
    }

    if (weight_sum > 0.00001f)
    {
        irradiance_sum *= 1.0f / (2.0f * weight_sum);
    }
    if (first_frame == 0)
    {
        float inv_history_gamma = 1.0f / history_gamma;
        float3 old_irradiance = pow(ddgi_volume_irradiance_texture[irradiance_uv], inv_history_gamma);
        irradiance_sum = pow(irradiance_sum, inv_history_gamma);
        irradiance_sum = pow(lerp(irradiance_sum, old_irradiance, history_alpha), history_gamma);
    }

    ddgi_volume_irradiance_texture[irradiance_uv] = irradiance_sum;
}




#endif