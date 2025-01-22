#include "../common/post_process.slang"
#include "../common/atmosphere_properties.slang"


cbuffer atomsphere_properties : register(b1)
{
    AtmosphereProperties AP;
};

cbuffer pass_constant : register(b2)
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
SamplerState TA_sampler : register(s0);

Texture2D<float> shadow_map_texture : register(t2);
SamplerState shadow_map_sampler : register(s1);

Texture2D<float2> blue_noise_texture : register(t3);
SamplerState blue_noise_sampler : register(s2);

[shader("pixel")]
float4 main(VertexOutput In) : SV_Target0
{
    float2 screen_space_position = In.screen_space_position.xy / In.screen_space_position.w;
    screen_space_position = 0.5f + float2(0.5f, -0.5f) * screen_space_position;

    float2 blue_noise_uv = screen_space_position * blue_noise_uv_factor;
    float2 blue_noise = blue_noise_texture.Sample(blue_noise_sampler, blue_noise_uv);

    // bn.x 控制偏移强度，bn.y 决定随机方向的角度，通过余弦和正弦函数将角度转换为2D方向矢量.
    float2 jitter = jitter_factor * blue_noise.x * float2(cos(2.0f * PI * blue_noise.y), sin(2.0f * PI * blue_noise.y));

    float aerial_perspective_z = world_scale * distance(In.world_space_position, camera_position) / max_aerial_distance;
    float4 aerial_perspective = aerial_lut_texture.Sample(TA_sampler, float3(screen_space_position + jitter, saturate(aerial_perspective_z)));

    float3 in_scatter = aerial_perspective.xyz;
    float eye_transmittance = aerial_perspective.w;

    float2 transmittance_uv = get_transmittance_uv(AP, world_scale * In.world_space_position.y, sun_theta);
	float3 sun_transmittance = transmittance_texture.Sample(TA_sampler, transmittance_uv);

    float3 sun_radiance = ground_albedo * max(0.0f, dot(In.world_space_normal, -sun_dir));

    // 将物体位置沿着法线稍微偏移 0.03 个单位。这种偏移可以避免阴影失真 z-fighting.
    float4 shadow_clip = mul(float4(In.world_space_position + 0.03 * In.world_space_normal, 1.0f), shadow_view_proj);
    float2 shadow_ndc = shadow_clip.xy / shadow_clip.w;
    float2 shadow_uv = 0.5f + float2(0.5f, -0.5f) * shadow_ndc;

    float shadow_factor = 1.0f;
    if (all(saturate(shadow_uv) == shadow_uv))
    {
        float depth = shadow_map_texture.Sample(shadow_map_sampler, shadow_uv);
        shadow_factor = float(shadow_clip.z <= depth);
    }

    float3 out_color = sun_intensity * (shadow_factor * sun_radiance * sun_transmittance * eye_transmittance + in_scatter);
    out_color = simple_post_process(screen_space_position, out_color);
    return float4(out_color, 1.0f); 
}

