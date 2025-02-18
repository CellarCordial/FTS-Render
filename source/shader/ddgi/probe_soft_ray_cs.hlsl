#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../common/ddgi.hlsl"
#include "../common/sdf_trace.hlsl"
#include "../common/sky.hlsl"
#include "../common/surface_cache.hlsl"

cbuffer pass_constants : register(b0)
{
    DDGIVolumeData volume_data;
    GlobalSDFInfo sdf_data;    

    float4x4 random_orientation;
    
    float sdf_voxel_size;
    float sdf_chunk_size;
    float max_gi_distance;
    uint surface_texture_resolution;
    uint surface_atlas_resolution;
};

Texture2D<float3> sky_lut_texture : register(t0);
Texture3D<float> global_sdf_texture : register(t1);
Texture2D<float3> surface_normal_atlas_texture : register(t2);
Texture2D<float> surface_depth_atlas_texture : register(t3);
Texture2D<float4> surface_light_cache_atlas_texture : register(t4);

StructuredBuffer<SDFChunkData> sdf_chunk_data_buffer : register(t5);
StructuredBuffer<MeshSurfaceData> mesh_surface_data_buffer : register(t6);

RWTexture2D<float4> ddgi_radiance_texture : register(u0);
RWTexture2D<float4> ddgi_direction_distance_texture : register(u1);

SamplerState linear_clamp_sampler : register(s1);
SamplerState linear_wrap_sampler : register(s2);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint ray_index = thread_id.x;
    uint probe_index = thread_id.y;

    uint total_probe_num = volume_data.probe_count.x * volume_data.probe_count.y * volume_data.probe_count.z;
    if (ray_index >= volume_data.ray_count || probe_index >= total_probe_num) return;

    uint3 probe_id = uint3(
        probe_index % volume_data.probe_count.x,
        (probe_index / volume_data.probe_count.x) % volume_data.probe_count.y,
        probe_index / (volume_data.probe_count.x * volume_data.probe_count.y)
    );
    float3 ray_ori = volume_data.origin_position + probe_id * volume_data.probe_interval_size;
    float3 ray_dir = mul(float4(spherical_fibonacci(ray_index, volume_data.ray_count), 1.0f), random_orientation).xyz;

    SDFHitData hit_data = trace_global_sdf(ray_ori, ray_dir, sdf_data, global_sdf_texture, linear_clamp_sampler);

    float3 radiance = float3(0.0f, 0.0f, 0.0f);
    float distance = 0.0f;

    if (!hit_data.is_hit())
    {
        radiance = sky_lut_texture.Sample(linear_clamp_sampler, get_sky_uv(ray_dir));
        distance = max_gi_distance;
    }
    else
    {
        if (!hit_data.is_inside())
        {
            float3 hit_pos = ray_ori + ray_dir * hit_data.step_size;

            uint chunk_num_per_axis = (uint)(sdf_data.sdf_grid_size / sdf_chunk_size);
            uint3 chunk_id = (uint3)((hit_pos - sdf_data.sdf_grid_origin) / sdf_chunk_size);
            uint chunk_index = chunk_id.x + chunk_id.y * chunk_num_per_axis + chunk_id.z * chunk_num_per_axis * chunk_num_per_axis;
            SDFChunkData sdf_chunk_data = sdf_chunk_data_buffer[chunk_index];
            
            // Chunk 不为空.
            if (sdf_chunk_data.mesh_index_begin != -1)
            {
                float weight_sum = 0.0f;
                for (int ix = sdf_chunk_data.mesh_index_begin; ix < sdf_chunk_data.mesh_index_end; ++ix)
                {
                    MeshSurfaceData surface_data = mesh_surface_data_buffer[ix];

                    if (length(surface_data.bounding_sphere.xyz - hit_pos) > surface_data.bounding_sphere.w) continue;

                    [unroll]
                    for (uint jx = 0; jx < 6; ++jx)
                    {
                        CardData card = surface_data.cards[jx];
                        float3 local_hit_pos = mul(float4(hit_pos, 1.0f), card.local_matrix).xyz;
                        
                        float2 uv = saturate(local_hit_pos.xy / card.extent.xy + 0.5f);
                        uv.y = 1.0f - uv.y;

                        float4 bilinear_weight = float4(
                            (1.0f - uv.x) * (1.0f - uv.y),
                            (uv.x)        * (1.0f - uv.y),
                            (1.0f - uv.x) * (uv.y),
                            (uv.x)        * (uv.y)
                        );

                        uv = (uv * surface_texture_resolution + card.atlas_offset) / surface_atlas_resolution;

                        // 击中点的深度与实际 surface 上的深度比值权重.
                        float4 surface_depth = surface_depth_atlas_texture.Gather(linear_wrap_sampler, uv) * card.extent.z;
                        float4 depth_weight;
                        [unroll]
                        for (uint kx = 0; kx < 4; ++kx)
                        {
                            if (surface_depth[kx] >= 1.0f) 
                            {
                                depth_weight[kx] = 0.0f;
                            }
                            else 
                            {
                                // 如果 hit position 和其所对应的 surface depth 相隔超过 3 个体素距离, 则 depthVisibility = 0.0f, 
                                // 在 2 ~ 3 个体素之间 depthVisibility = 0 ~ 1,
                                // 在 2 个体素以下 depthVisibility = 1.
                                depth_weight[kx] = 1.0f - clamp(abs(local_hit_pos.z - surface_depth[kx]) - 2.0f * sdf_voxel_size, 0.0f, 1.0f);
                            }
                        }

                        // 光线与击中点的夹角权重.
                        float3 hit_normal = surface_normal_atlas_texture.SampleLevel(linear_wrap_sampler, uv, 0.0f);
                        float normal_weight = saturate(dot(hit_normal, ray_dir));
                        
                        // 总权重.
                        float sample_weight = dot(depth_weight, bilinear_weight) * normal_weight;
                        if (sample_weight <= 0.0f) continue;
                        else
                        {
                            radiance += float3(
                                dot(surface_light_cache_atlas_texture.GatherRed(linear_wrap_sampler, uv), sample_weight), 
                                dot(surface_light_cache_atlas_texture.GatherGreen(linear_wrap_sampler, uv), sample_weight), 
                                dot(surface_light_cache_atlas_texture.GatherBlue(linear_wrap_sampler, uv), sample_weight)
                            );
                            weight_sum += sample_weight;
                        }
                    }
                }
                radiance /= max(weight_sum, 0.0001f);
            }

            distance = max(hit_data.step_size + 0.5f * sdf_voxel_size, 0.0f);
        }
    }

    ddgi_radiance_texture[thread_id.xy] = float4(radiance, 1.0f);
    ddgi_direction_distance_texture[thread_id.xy] = float4(ray_dir, distance);
}




#endif