#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../common/ddgi.slang"


cbuffer pass_constants : register(b0)
{
    float4x4 inv_view_proj;

    float3 camera_position;
    float pad;

    DDGIVolumeData volume_data;
};

Texture2D<float3> irradiance_texture : register(t0);
Texture2D<float2> depth_texture : register(t1);

Texture2D<float2> world_space_normal_texture : register(t2);
Texture2D<float4> world_position_view_depth_texture : register(t3);

RWTexture2D<float4> output_texture : register(u0);

SamplerState linear_clamp_sampler : register(s0);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_Dispatchthread_id)
{
    uint2 uv = thread_id.xy;
    float depth = depth_texture[uv].r;
    
    if (abs(depth - 1.0f) <= 0.0001f)
    {
        output_texture[uv] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }
    
    float3 pixel_world_position = get_world_pos_from_depth_ndc(uv, depth, inv_view_proj);
    float3 pixel_world_normal = octahedron_to_unit_vector(world_space_normal_texture[uv]);
    float3 pixel_to_camera = normalize(camera_position - pixel_world_position);

    float3 irradiance = sample_probe_irradiance(
        volume_data, 
        pixel_world_position, 
        pixel_world_normal, 
        pixel_to_camera, 
        irradiance_texture, 
        depth_texture, 
        linear_clamp_sampler
    );

    output_texture[thread_id.xy] = float4(irradiance, 1.0f);
}














#endif