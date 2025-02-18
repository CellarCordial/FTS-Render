#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../common/ddgi.hlsl"


cbuffer pass_constants : register(b0)
{
    float3 camera_position;
    float pad;

    DDGIVolumeData volume_data;
};

Texture2D<float3> ddgi_volume_irradiance_texture : register(t0);
Texture2D<float2> ddgi_volume_depth_texture : register(t1);

Texture2D<float4> world_space_normal_texture : register(t2);
Texture2D<float4> world_position_view_depth_texture : register(t3);

RWTexture2D<float4> ddgi_final_irradiance_texture : register(u0);

SamplerState linear_warp_sampler : register(s0);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint2 pixel_id = thread_id.xy;
    float4 world_position_view_depth = world_position_view_depth_texture[pixel_id];

    float depth = world_position_view_depth.w;
    if (abs(depth - 1.0f) <= 0.0001f)
    {
        ddgi_final_irradiance_texture[pixel_id] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float3 pixel_world_position = world_position_view_depth.xyz;
    float3 pixel_world_normal = world_space_normal_texture[pixel_id].xyz;
    float3 pixel_to_camera = normalize(camera_position - pixel_world_position);

    float3 irradiance = sample_probe_irradiance(
        volume_data,
        pixel_world_position,
        pixel_world_normal,
        pixel_to_camera,
        ddgi_volume_irradiance_texture,
        ddgi_volume_depth_texture,
        linear_warp_sampler
    );

    ddgi_final_irradiance_texture[thread_id.xy] = float4(irradiance, 1.0f);
}














#endif