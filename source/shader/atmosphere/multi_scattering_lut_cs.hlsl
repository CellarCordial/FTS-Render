// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1
// #define DIRECTION_SAMPLE_COUNT 1

#include "../common/atmosphere_properties.hlsl"
#include "../common/intersect.hlsl"

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(DIRECTION_SAMPLE_COUNT)

cbuffer pass_constants : register(b0)
{
    float3 SunIntensity;
    int ray_march_step_count;

    float3 GroundAlbedo;
    float pad;
};


cbuffer atmosphere_properties : register(b1)
{
    AtmosphereProperties AP;
};

Texture2D<float4> transmittance_texture : register(t0);
StructuredBuffer<float2> dir_samples_buffer : register(t1);
RWTexture2D<float4> multi_scattering_texture : register(u0);
SamplerState sampler_ : register(s0);

// u1 控制 Dir 的 z 值, u2 控制 Dir 投影在 xy 平面的与 x 轴的角度.
float3 unit_sphere_dir(float u1, float u2)
{
    float z = 1.0f - 2.0f * u1;     // [0, 1] -> [-1, 1].
    float r = sqrt(max(0.0f, 1.0f - z * z));    // r^2 + z^2 = 1. r 为 Dir 投影 xy 平面的半径长度.
    float phi = 2.0f * PI * u2;     // [0, 2PI].
    return float3(r * cos(phi), r * sin(phi), z);
}

void light_intergate(float3 world_ori, float3 world_dir, float sun_theta, float3 sun_dir, out float3 l2, out float3 f)
{
    float distance = 0.0f;
    bool ground_intersect = intersect_ray_sphere(world_ori, world_dir, AP.planet_radius, distance);
    if (!ground_intersect)
    {
        intersect_ray_sphere(world_ori, world_dir, AP.atmosphere_radius, distance);
    }

    float dt = distance / ray_march_step_count;

    float3 transmittance_sum = float3(0.0f, 0.0f, 0.0f);
    float3 l2_sum = transmittance_sum;
    float3 f_sum = transmittance_sum;

    float theta = dot(world_dir, sun_dir);
    float t = 0.0f;
    for (uint ix = 0; ix < ray_march_step_count; ++ix)
    {
        float fMidT = t + 0.5f * dt;
        t += dt;

        float3 p = world_ori + world_dir * fMidT;
        float h = length(p) - AP.planet_radius;

        float3 scatter, transmittance;
        get_scatter_transmittance(AP, h, scatter, transmittance);

        float3 delta_transmittance = dt * transmittance;
        float3 total_transmittance = exp(-transmittance_sum - 0.5f * delta_transmittance);

        if (!intersect_ray_sphere(p, sun_dir, AP.planet_radius))
        {
            float3 phase_func = estimate_phase_func(AP, h, theta);
            float2 uv = get_transmittance_uv(AP, h, sun_theta);
            float4 sun_transmittance = transmittance_texture.SampleLevel(sampler_, uv, 0);

            l2_sum += dt * sun_transmittance.xyz * (total_transmittance * scatter * phase_func) * SunIntensity;
        }
        f_sum += dt * total_transmittance * scatter;
        transmittance_sum += delta_transmittance;
    }

    // 地面反射.
    if (ground_intersect)
    {
        float3 transmittance = exp(-transmittance_sum);
        float2 uv = get_transmittance_uv(AP, 0.0f, sun_theta);
        float4 sun_transmittance = transmittance_texture.SampleLevel(sampler_, uv, 0);

        l2_sum += transmittance * sun_transmittance.xyz * max(0.0f, sun_dir.y) * SunIntensity * (GroundAlbedo / PI);
    }

    l2 = l2_sum;
    f = f_sum;
}

float3 estimate_multi_scatter(float theta, float world_height)
{
    float3 world_ori = float3(0.0f, AP.planet_radius + world_height, 0.0f);
    float3 sun_dir = float3(cos(theta), sin(theta), 0.0f);

    float3 l2_sum = float3(0.0f, 0.0f, 0.0f);
    float3 f_sum = float3(0.0f, 0.0f, 0.0f);
    for (uint ix = 0; ix < DIRECTION_SAMPLE_COUNT; ++ix)
    {
        float2 dir_sample = dir_samples_buffer[ix];
        float3 world_dir = unit_sphere_dir(dir_sample.x, dir_sample.y);
        
        float3 l2, f;
        light_intergate(world_ori, world_dir, theta, sun_dir, l2, f);

        l2_sum += l2;
        f_sum += f;
    }

    // 平均.
    float3 l2 = l2_sum / DIRECTION_SAMPLE_COUNT;
    float3 f = f_sum / DIRECTION_SAMPLE_COUNT;

    return l2 / ( 1.0f - f);
}


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint width, height;
    multi_scattering_texture.GetDimensions(width, height);

    if (thread_id.x >= width || thread_id.y >= height) return;

    float theta = asin(lerp(-1.0f, 1.0f, (thread_id.y + 0.5f) / height));
    float world_height = lerp(0.0f, AP.atmosphere_radius - AP.planet_radius, (thread_id.x + 0.5f) / width);

    multi_scattering_texture[thread_id.xy] = float4(estimate_multi_scatter(theta, world_height), 1.0f);
}







#endif







