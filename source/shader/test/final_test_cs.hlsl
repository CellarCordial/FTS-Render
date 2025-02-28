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

struct VertexOutput
{
    float4 sv_position : SV_POSITION;
    float4 screen_space_position : SCREEN_POSITION;
    float3 world_space_position : POSITION;
    float3 world_space_normal : NORMAL;
};

Texture2D<float3> transmittance_texture : register(t0);
Texture3D<float4> aerial_lut_texture : register(t1);
Texture3D<float4> vt_indirect_texture : register(t1);
Texture2D<float> shadow_map_texture : register(t2);
Texture2D<float2> blue_noise_texture : register(t3);


SamplerState linear_clamp_sampler : register(s0);
SamplerState point_clamp_sampler : register(s1);
SamplerState point_wrap_sampler : register(s2);

RWTexture2D<float4> final_texture : register(u0);
Texture2D<float4> world_position_view_depth_texture : register(t0);
Texture2D<float4> world_space_normal_texture : register(t1);

// TODO: Everything.

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint2 pixel_id = thread_id.xy;

    float3 world_space_position = world_position_view_depth_texture[pixel_id].xyz;
    float4 sv_position = mul(float4(world_space_position, 1.0f), view_proj);
    float2 screen_space_position = sv_position.xy / sv_position.w;
    screen_space_position = 0.5f + float2(0.5f, -0.5f) * screen_space_position;

    float2 blue_noise_uv = screen_space_position * blue_noise_uv_factor;
    float2 blue_noise = blue_noise_texture.Sample(point_wrap_sampler, blue_noise_uv);

    // bn.x 控制偏移强度，bn.y 决定随机方向的角度，通过余弦和正弦函数将角度转换为2D方向矢量.
    float2 jitter = jitter_factor * blue_noise.x * float2(cos(2.0f * PI * blue_noise.y), sin(2.0f * PI * blue_noise.y));

    float aerial_perspective_z = world_scale * distance(world_space_position, camera_position) / max_aerial_distance;
    float4 aerial_perspective = aerial_lut_texture.Sample(linear_clamp_sampler, float3(screen_space_position + jitter, saturate(aerial_perspective_z)));

    float3 in_scatter = aerial_perspective.xyz;
    float eye_transmittance = aerial_perspective.w;

    float2 transmittance_uv = get_transmittance_uv(AP, world_scale * world_space_position.y, sun_theta);
	float3 sun_transmittance = transmittance_texture.Sample(linear_clamp_sampler, transmittance_uv);

    float3 world_space_normal = world_space_normal_texture[pixel_id].xyz;

    float3 sun_radiance = ground_albedo * max(0.0f, dot(world_space_normal, -sun_dir));

    // 将物体位置沿着法线稍微偏移 0.03 个单位。这种偏移可以避免阴影失真 z-fighting.
    float4 shadow_clip = mul(float4(world_space_position + 0.03 * world_space_normal, 1.0f), shadow_view_proj);
    float2 shadow_ndc = shadow_clip.xy / shadow_clip.w;
    float2 shadow_uv = 0.5f + float2(0.5f, -0.5f) * shadow_ndc;

    float shadow_factor = 1.0f;
    if (all(saturate(shadow_uv) == shadow_uv))
    {
        float depth = shadow_map_texture.Sample(point_clamp_sampler, shadow_uv);
        shadow_factor = float(shadow_clip.z <= depth);
    }

    float3 out_color = sun_intensity * (shadow_factor * sun_radiance * sun_transmittance * eye_transmittance + in_scatter);
    out_color = simple_post_process(screen_space_position, out_color);
    final_texture[pixel_id] = float4(out_color, 1.0f); 
}

#endif
