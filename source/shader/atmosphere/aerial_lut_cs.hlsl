// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1

#include "../common/atmosphere_properties.hlsl"
#include "../common/intersect.hlsl"



cbuffer pass_constants : register(b0)
{
    float3 sun_dir;          float sun_theta;
    float3 frustum_A;        float max_distance;
    float3 frustum_B;        int per_slice_march_step_count;
    float3 frustum_C;        float atmos_eye_height;
    float3 frustum_D;        uint enable_multi_scattering;
    float3 camera_position;  uint enable_shadow;
    float world_scale;      float3 pad;
    float4x4 shadow_view_proj;
};


cbuffer atomsphere_properties : register(b1)
{
    AtmosphereProperties AP;
};


Texture2D<float3> multi_scattering_texture : register(t0);
Texture2D<float3> transmittance_texture : register(t1);
Texture2D<float> shadow_map_texture : register(t2);

SamplerState linear_clamp_sampler : register(s0);
SamplerState point_clamp_sampler : register(s1);

RWTexture3D<float4> aerial_perspective_lut_texture : register(u0);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)


float relative_luminance(float3 c)
{
    return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
}

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint width, height, depth;
    aerial_perspective_lut_texture.GetDimensions(width, height, depth);

    if (thread_id.x >= width || thread_id.y >= height) return;

    float u = (thread_id.x + 0.5f) / width;
    float v = (thread_id.y + 0.5f) / height;

    float3 ori = float3(0.0f, atmos_eye_height, 0.0f);
    float3 dir = normalize(lerp(
        lerp(frustum_A, frustum_B, u),
        lerp(frustum_C, frustum_D, u),
        v
    ));

    float cos_theta = dot(-sun_dir, dir);

    float distance = 0.0f;
    float3 world_ori = ori + float3(0.0f, AP.planet_radius, 0.0f);
    if (!intersect_ray_sphere(world_ori, dir, AP.planet_radius, distance))
    {
        intersect_ray_sphere(world_ori, dir, AP.atmosphere_radius, distance);
    }

    float slice_depth = max_distance / depth;
    float begin = 0.0f, end = min(0.5f * slice_depth, distance);

    float3 transmittance_sum = float3(0.0f, 0.0f, 0.0f);
    float3 scatter_sum = float3(0.0f, 0.0f, 0.0f);

    float random = frac(sin(dot(float2(u, v), float2(12.9898f, 78.233f) * 2.0f)) * 43758.5453f);

    for (uint z = 0; z < depth; ++z)
    {
        float dt = (end - begin) / per_slice_march_step_count;
        float t = begin;
        for (uint ix = 0; ix < per_slice_march_step_count; ++ix)
        {
            float next_t = t + dt;
            float mid_t = lerp(t, next_t, random);

            float3 pos = world_ori + dir * mid_t;
            float height = length(pos) - AP.planet_radius;

            float3 transmittance;
            float3 in_scatter;
            get_scatter_transmittance(AP, height, in_scatter, transmittance);

            float3 delta_transimittance = dt * transmittance;
            float3 eye_transimittance = exp(-transmittance_sum - 0.5f * delta_transimittance);
            float2 uv = get_transmittance_uv(AP, height, sun_theta);

            if (!intersect_ray_sphere(pos, -sun_dir, AP.planet_radius))
            {
                float3 shadow_pos = camera_position + dir * mid_t / world_scale;
                float4 shadow_clip = mul(float4(shadow_pos, 1.0f), shadow_view_proj);
                float2 shadow_ndc = shadow_clip.xy / shadow_clip.w;
                float2 shadow_uv = 0.5f + float2(0.5f, -0.5f) * shadow_ndc;

                bool in_shadow = bool(enable_shadow);
                if (in_shadow && all(saturate(shadow_uv) == shadow_uv))
                {
                    float ray_z = shadow_clip.z;
                    float shadow_map_z = shadow_map_texture.SampleLevel(point_clamp_sampler, shadow_uv, 0);
                    in_shadow = ray_z >= shadow_map_z;
                }

                if (!in_shadow)
                {
                    float3 phase = estimate_phase_func(AP, height, cos_theta);
                    float3 sun_transmittance = transmittance_texture.SampleLevel(linear_clamp_sampler, uv, 0);

                    scatter_sum += dt * (eye_transimittance * in_scatter * phase) * sun_transmittance;
                }
            }

            if (bool(enable_multi_scattering))
            {
                float3 multi_scattering = multi_scattering_texture.SampleLevel(linear_clamp_sampler, uv, 0);
                scatter_sum += dt * eye_transimittance * in_scatter * multi_scattering;
            }

            transmittance_sum += delta_transimittance;
            t = next_t;
        }

        aerial_perspective_lut_texture[uint3(thread_id.xy, z)] = float4(scatter_sum, relative_luminance(exp(-transmittance_sum)));

        begin = end;
        end = min(end + slice_depth, distance);
    }
}






#endif