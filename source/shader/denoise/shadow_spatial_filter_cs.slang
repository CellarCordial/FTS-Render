#define THREAD_GROUP_NUM_X 1
#define THREAD_GROUP_NUM_Y 1

cbuffer pass_constants : register(b0)
{
    uint2 shadow_map_resolution;
    float2 inv_shadow_map_resolution;

    float4x4 inv_view_matrix;
    float4x4 inv_proj_matrix;

    uint2 shadow_meta_texture_resolution;
    float depth_similarity_sigma;
    uint32_t step_size;
};

Texture2D<float> depth_texture : register(t0);
Texture2D<float> view_space_normal_texture : register(t1);
Texture2D<uint32_t> shadow_meta_texture : register(t2);
Texture2D<float2> shadow_temporal_filter_texture : register(t3);

RWTexture2D<float2> denoised_shadow_texture : register(u0);

float4x4 FFX_DNSR_Shadows_GetViewProjectionInverse() { return inv_view_matrix; }
float2 FFX_DNSR_Shadows_GetInvBufferDimensions() { return inv_shadow_map_resolution; }
float4x4 FFX_DNSR_Shadows_GetProjectionInverse() { return inv_proj_matrix; }
float FFX_DNSR_Shadows_GetDepthSimilaritySigma() { return depth_similarity_sigma; }

bool FFX_DNSR_Shadows_IsShadowReciever(uint2 pixel_id) { return depth_texture[pixel_id] != 0.0f; }
float FFX_DNSR_Shadows_ReadDepth(uint2 pixel_id) { return depth_texture[pixel_id]; }
float3 FFX_DNSR_Shadows_ReadNormals(uint2 pixel_id) { return view_space_normal_texture[pixel_id]; }

float16_t2 FFX_DNSR_Shadows_ReadInput(uint2 pixel_id) { return float16_t2(shadow_temporal_filter_texture[pixel_id]);}

uint32_t FFX_DNSR_Shadows_ReadTileMetaData(uint32_t tile_index)
{
    uint2 tile_id = uint2(
        tile_index % shadow_meta_texture_resolution.x,
        tile_index / shadow_meta_texture_resolution.x
    );
    return shadow_meta_texture[tile_id];
}

#include "../../External/ffx-shadows-dnsr/ffx_denoiser_shadows_filter.slang"

#if defined(THREAD_GROUP_NUM_X) && defined(THREAD_GROUP_NUM_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, THREAD_GROUP_NUM_Y, 1)]
void compute_shader(uint3 thread_id : SV_Dispatchthread_id, uint2 group_id: SV_GroupID, uint2 group_thread_id : SV_Groupthread_id)
{
    bool write_result = true;
    float2 Result = FFX_DNSR_Shadows_FilterSoftShadowsPass(group_id, group_thread_id, thread_id.xy, write_result, 0, step_size);
    if (write_result) denoised_shadow_texture[thread_id.xy] = Result;
}

#endif