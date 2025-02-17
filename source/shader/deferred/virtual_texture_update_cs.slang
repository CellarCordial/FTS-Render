// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1

#include "../common/gbuffer.slang"

[vk_binding(0, 0)]
cbuffer pass_constants : register(b0)
{
    uint32_t vt_page_size;
    uint32_t vt_physical_texture_size;
};

[vk_binding(1, 0)] Texture2D<float2> tile_uv_texture : register(t0);
[vk_binding(2, 0)] Texture2D<uint2> vt_indirect_texture : register(t1);
[vk_binding(3, 0)] Texture2D<float4> vt_base_color_physical_texture : register(t2);
[vk_binding(4, 0)] Texture2D<float3> vt_normal_physical_texture : register(t3);
[vk_binding(5, 0)] Texture2D<float3> vt_pbr_physical_texture : register(t4);
[vk_binding(6, 0)] Texture2D<float4> vt_emissive_physical_texture : register(t5);
[vk_binding(7, 0)] Texture2D<float4> world_space_tangent_texture : register(t6);

[vk_binding(8, 0)] RWTexture2D<float4> world_space_normal_texture : register(u0);
[vk_binding(9, 0)] RWTexture2D<float4> base_color_texture : register(u1);
[vk_binding(10, 0)] RWTexture2D<float4> pbr_texture : register(u2);
[vk_binding(11, 0)] RWTexture2D<float4> emissive_texture : register(u3);

[vk_binding(12, 0)] SamplerState linear_clamp_sampler : register(s0);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint2 pixel_id = thread_id.xy;
    float2 tile_uv = tile_uv_texture[pixel_id];
    uint2 indirection_info = vt_indirect_texture[pixel_id];
    if (indirection_info.x == INVALID_SIZE_32 || indirection_info.y == INVALID_SIZE_32) return;

    float2 physical_uv =
        (tile_uv * vt_page_size + indirection_info.xy * vt_page_size) /
        vt_physical_texture_size;

    float3 normal = calculate_normal(
        vt_normal_physical_texture.Sample(linear_clamp_sampler, physical_uv).xyz,
        normalize(world_space_normal_texture[pixel_id].xyz),
        normalize(world_space_tangent_texture[pixel_id].xyz)
    );
    float4 base_color =
        vt_base_color_physical_texture.Sample(linear_clamp_sampler, physical_uv) *
        base_color_texture[pixel_id];
    float3 pbr =
        vt_pbr_physical_texture.Sample(linear_clamp_sampler, physical_uv).rgb *
        pbr_texture[pixel_id].rgb;
    float4 emissive =
        vt_emissive_physical_texture.Sample(linear_clamp_sampler, physical_uv) * 
        emissive_texture[pixel_id];

    world_space_normal_texture[pixel_id] = float4(normal, 0.0f);
    base_color_texture[pixel_id] = base_color;
    pbr_texture[pixel_id] = float4(pbr, 1.0f);
    emissive_texture[pixel_id] = emissive;
}

#endif