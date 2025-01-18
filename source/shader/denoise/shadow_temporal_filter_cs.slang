#define THREAD_GROUP_NUM_X 1
#define THREAD_GROUP_NUM_Y 1

#include "../bicubic_texture_filter.slang"

cbuffer pass_constants : register(b0)
{
    uint2 shadow_map_resolution;
    float2 inv_shadow_map_resolution;

    float4x4 inv_view_matrix;
    float4x4 inv_proj_matrix;
    float4x4 reproj_matrix;

    float3 camera_position;
    float max_shadow_moments_variance;

    uint2 shadow_meta_texture_resolution;
    uint32_t is_first_frame;

    uint2 shadow_moments_texture_resolution;
};

Texture2D<float4> shadow_map_texture : register(t0);
Texture2D<float4> prev_shadow_map_texture : register(t1);
Texture2D<uint32_t> bit_packed_shadow_texture : register(t2);
Texture2D<float4> prev_shadow_moments_texture : register(t3);

RWTexture2D<uint32_t> shadow_meta_texture : register(u0);
RWTexture2D<float4> shadow_moments_texture : register(u1);
RWTexture2D<float2> shadow_temporal_filter_texture : register(u2);

float4x4 FFX_DNSR_Shadows_GetViewProjectionInverse() { return inv_view_matrix; }
float2 FFX_DNSR_Shadows_GetInvBufferDimensions() { return inv_shadow_map_resolution; }
float4x4 FFX_DNSR_Shadows_GetProjectionInverse() { return inv_proj_matrix; }
float4x4 FFX_DNSR_Shadows_GetReprojectionMatrix() { return reproj_matrix; }
uint2 FFX_DNSR_Shadows_GetBufferDimensions() { return shadow_map_resolution; }
uint32_t FFX_DNSR_Shadows_IsFirstFrame() { return is_first_frame; }
float3 FFX_DNSR_Shadows_GetEye() { return camera_position; }

float FFX_DNSR_Shadows_ReadDepth() { return 0.5f; }
bool FFX_DNSR_Shadows_IsShadowReciever(uint2 ID) { return true; }
float FFX_DNSR_Shadows_ReadPreviousDepth(uint2 ID) { return 0.5f; }
float2 FFX_DNSR_Shadows_ReadVelocity(uint2 ID) { return float2(0.0f, 0.0f); }
float3 FFX_DNSR_Shadows_ReadNormals(uint2 ID) { return float3(0.0f, 0.0f, 1.0f); }

void FFX_DNSR_Shadows_WriteMetadata(uint32_t tile_index, uint32_t mask)
{
    uint2 tile_id = uint2(
        tile_index % shadow_meta_texture_resolution.x,
        tile_index / shadow_meta_texture_resolution.x
    );
    shadow_meta_texture[tile_id] = mask;
}

uint32_t FFX_DNSR_Shadows_ReadRaytracedShadowMask(uint32_t tile_index)
{
    uint2 tile_id = uint2(
        tile_index % shadow_meta_texture_resolution.x,
        tile_index / shadow_meta_texture_resolution.x
    );
    return bit_packed_shadow_texture[tile_id];
}

void FFX_DNSR_Shadows_Writemoments(uint2 pixel_id, float4 shadow_moments)
{
    shadow_moments.z = min(shadow_moments.z, max_shadow_moments_variance);
    shadow_moments_texture[pixel_id] = shadow_moments;
}

void FFX_DNSR_Shadows_WriteReprojectionResults(uint2 pixel_id, float2 ShadowVariance)
{
    shadow_temporal_filter_texture[pixel_id] = ShadowVariance;
}

float FFX_DNSR_Shadows_HitsLight(uint2 pixel_id)
{
    return shadow_map_texture[pixel_id].r;
}

float4 FFX_DNSR_Shadows_ReadPreviousmomentsBuffer(float2 uv)
{
    float4 moments = bicubic_hermite_texture_sample(uv, prev_shadow_moments_texture, shadow_moments_texture_resolution);
    moments.yz = max(float2(0.0f, 0.0f), moments.yz);
    return moments;
}

float FFX_DNSR_Shadows_ReadHistory(float2 uv)
{
    return bicubic_hermite_texture_sample(uv, prev_shadow_map_texture, shadow_map_resolution).r;
}


#include "../../External/ffx-shadows-dnsr/ffx_denoiser_shadows_tileclassification.slang"

#if defined(THREAD_GROUP_NUM_X) && defined(THREAD_GROUP_NUM_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, THREAD_GROUP_NUM_Y, 1)]
void main(uint32_t group_index: SV_GroupIndex, uint2 group_id : SV_GroupID)
{
    FFX_DNSR_Shadows_TileClassification(group_index, group_id);
}


#endif