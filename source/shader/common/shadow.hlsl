#ifndef SHADER_SHADOW_HLSL
#define SHADER_SHADOW_HLSL


bool estimate_shadow(
    float3 world_space_position, 
    float4x4 shadow_view_proj,
    uint vt_shadow_page_size,
    uint vt_virtual_shadow_resolution,
    uint vt_physical_shadow_resolution,
    StructuredBuffer<uint2> vt_shadow_indirect_buffer,
    Texture2D<float> vt_physical_shadow_texture,
    SamplerState sampler_
)
{
    uint vt_virtual_shadow_axis_tile_num = vt_virtual_shadow_resolution / vt_shadow_page_size;

    float4 shadow_clip = mul(float4(world_space_position, 1.0f), shadow_view_proj);
    float2 shadow_ndc = shadow_clip.xy / shadow_clip.w;
    float2 shadow_uv = 0.5f + float2(0.5f, -0.5f) * shadow_ndc;

    uint2 shadow_pixel_id = uint2(shadow_uv * vt_virtual_shadow_resolution);
    uint2 interal_uv = shadow_pixel_id % vt_shadow_page_size;
    uint2 tile_id = shadow_pixel_id / vt_shadow_page_size;
    uint2 page_id = vt_shadow_indirect_buffer[tile_id.x + tile_id.y * vt_virtual_shadow_axis_tile_num];
    
    float2 uv = float2(interal_uv + page_id * vt_shadow_page_size) / vt_physical_shadow_resolution;

    float depth = vt_physical_shadow_texture.Sample(sampler_, uv);
    return shadow_clip.z <= depth;
}


#endif