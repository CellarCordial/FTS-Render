#include "../../math_helper.slang"
#include "../../ray_tracing_helper/shadow_helper.slang"

cbuffer gPassConstants : register(b0)
{
    float4x4 inv_view_matrix;

    float3 sun_direction;
    uint frame_index;

    float sun_angular_radius;
};

Texture2D<float> depth_texture : register(t0);
Texture2D<float4> blue_noise_texture : register(t2);
Texture2D<float> view_space_normal_texture : register(t3);
Texture2D<float> view_space_position_texture : register(t4);
RaytracingAccelerationStructure accel_struct : register(t5);

RWTexture2D<float4> shadow_mask_texture : register(u0);


[shader("raygeneration")]
void RayGeneration()
{
    uint2 ray_id = DispatchRaysIndex().xy;
    float2 uv = (ray_id + 0.5f) / DispatchRaysDimensions().xy;

    if (depth_texture[ray_id] == 0.0f)
    {
        shadow_mask_texture[ray_id] = 1;
        return;
    }

    float3 view_space_position = view_space_position_texture[ray_id];
    float3 world_space_position = mul(float4(view_space_position, 1.0f), inv_view_matrix).xyz;
    float3 world_space_normal = view_space_normal_texture[ray_id];
    float3 view_space_normal = mul(float4(world_space_normal, 1.0f), inv_view_matrix).xyz;

    float3 bias_step = normalize(view_space_normal) * (length(world_space_position) - view_space_position.z) * 1e-5;
    float3 ray_origin = world_space_position + bias_step;

    float2 random = sample_from_blue_noise(ray_id, frame_index, blue_noise_texture).xy;
    
    RayDesc ray;
    ray.Origin = ray_origin;
    ray.Direction = mul(create_orthonormal_basis(sun_direction), sample_direction_from_cone(random, cos(sun_angular_radius)));
    ray.TMin = 0.0f;
    ray.TMax = MAX_FLOAT;

    shadow_mask_texture[ray_id] = ray_tracing_if_shadowed(accel_struct, ray) ? float4(0.0f, 0.0f, 0.0f, 0.0f) : float4(1.0f, 1.0f, 1.0f, 1.0f);
}

