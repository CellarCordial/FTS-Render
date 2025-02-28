#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../common/post_process.hlsl"
#include "../common/atmosphere_properties.hlsl"


cbuffer pass_constant : register(b0)
{
    float3 sun_dir;
    float sun_theta;

    float3 sun_intensity;
    float max_aerial_distance;

    float3 camera_position;
    float world_scale;

    float4x4 shadow_view_proj;

    float2 jitter_factor;
    float2 blue_noise_uv_factor;

    float3 ground_albedo;
    float pad;

    float4x4 view_proj;
};

cbuffer atomsphere_properties : register(b1)
{
    AtmosphereProperties AP;
};

Texture2D<float3> transmittance_texture : register(t0);
Texture3D<float4> aerial_lut_texture : register(t1);
Texture2D<float> vt_physical_shadow_texture : register(t2);
Texture2D<float2> blue_noise_texture : register(t3);
Texture2D<float4> world_position_view_depth_texture : register(t4);
Texture2D<float4> world_space_normal_texture : register(t5);
Texture2D<uint3> shadow_uv_depth_texture : register(t6);

RWTexture2D<float4> final_texture : register(u0);

SamplerState linear_clamp_sampler : register(s0);
SamplerState point_clamp_sampler : register(s1);
SamplerState point_wrap_sampler : register(s2);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint2 pixel_id = thread_id.xy;
    uint3 shadow_uv_depth = shadow_uv_depth_texture[pixel_id];

    uint2 shadow_uv = shadow_uv_depth.xy;
    if (any(shadow_uv == INVALID_SIZE_32)) return;

    float3 world_space_position = world_position_view_depth_texture[pixel_id].xyz;

    float2 blue_noise_uv = pixel_id * blue_noise_uv_factor;
    float2 blue_noise = blue_noise_texture.Sample(point_wrap_sampler, blue_noise_uv);

    // bn.x 控制偏移强度，bn.y 决定随机方向的角度，通过余弦和正弦函数将角度转换为2D方向矢量.
    float2 jitter = jitter_factor * blue_noise.x * float2(cos(2.0f * PI * blue_noise.y), sin(2.0f * PI * blue_noise.y));

    float aerial_perspective_z = world_scale * distance(world_space_position, camera_position) / max_aerial_distance;
    float4 aerial_perspective = aerial_lut_texture.Sample(linear_clamp_sampler, float3(pixel_id + jitter, saturate(aerial_perspective_z)));

    float3 in_scatter = aerial_perspective.xyz;
    float eye_transmittance = aerial_perspective.w;

    float2 transmittance_uv = get_transmittance_uv(AP, world_scale * world_space_position.y, sun_theta);
	float3 sun_transmittance = transmittance_texture.Sample(linear_clamp_sampler, transmittance_uv);

    float3 world_space_normal = world_space_normal_texture[pixel_id].xyz;
    float3 sun_radiance = ground_albedo * max(0.0f, dot(world_space_normal, -sun_dir));

    float shadow_factor = 1.0f;
    float depth = vt_physical_shadow_texture.Sample(point_clamp_sampler, shadow_uv);
    shadow_factor = float(asfloat(shadow_uv_depth.z) <= depth);

    float3 out_color = sun_intensity * (shadow_factor * sun_radiance * sun_transmittance * eye_transmittance + in_scatter);
    final_texture[pixel_id] = float4(simple_post_process(pixel_id, out_color), 1.0f); 
}

#endif
