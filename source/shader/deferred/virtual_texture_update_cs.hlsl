#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1
#define VT_FEED_BACK_SCALE_FACTOR 1

#include "../common/gbuffer.hlsl"


cbuffer pass_constants : register(b0)
{
    float4x4 shadow_view_proj;

    uint2 client_resolution;
    uint vt_page_size;
    uint vt_physical_texture_size;
    
    uint vt_virtual_shadow_resolution;
    uint vt_shadow_page_size;
};

Texture2D<uint2> vt_page_uv_texture : register(t0);
Texture2D<uint4> vt_indirect_texture : register(t1);
Texture2D<float4> vt_base_color_physical_texture : register(t2);
Texture2D<float3> vt_normal_physical_texture : register(t3);
Texture2D<float3> vt_pbr_physical_texture : register(t4);
Texture2D<float4> vt_emissive_physical_texture : register(t5);
Texture2D<float4> world_position_view_depth_texture : register(t6);
Texture2D<float4> world_space_tangent_texture : register(t6);

RWTexture2D<float4> world_space_normal_texture : register(u0);
RWTexture2D<float4> base_color_texture : register(u1);
RWTexture2D<float4> pbr_texture : register(u2);
RWTexture2D<float4> emissive_texture : register(u3);
RWTexture2D<uint3> shadow_uv_depth_texture : register(u4);

SamplerState linear_clamp_sampler : register(s0);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(VT_FEED_BACK_SCALE_FACTOR)


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    if (thread_id.x >= client_resolution.x || thread_id.y >= client_resolution.y) return;

    uint2 pixel_id = thread_id.xy;
    uint2 page_uv = vt_page_uv_texture[pixel_id];
    if (page_uv.x == INVALID_SIZE_32 || page_uv.y == INVALID_SIZE_32) return;

    uint2 indirect_uv = pixel_id / VT_FEED_BACK_SCALE_FACTOR;
    uint4 indirect_info = vt_indirect_texture[indirect_uv];

    if (any(indirect_info == INVALID_SIZE_32))
    {
        // 如果没有命中 indirect address, 则按 x 型向四周搜索.
        int factor = VT_FEED_BACK_SCALE_FACTOR;
        for (int offset = -factor; offset <= factor; ++offset)
        {
            indirect_info = vt_indirect_texture[indirect_uv + offset];
            if (all(indirect_info != INVALID_SIZE_32)) break;
            indirect_info = vt_indirect_texture[indirect_uv + uint2(-offset, offset)];
            if (all(indirect_info != INVALID_SIZE_32)) break;
        }
    }

    uint2 page_coordinate = indirect_info.xy;
    float2 physical_uv = float2(page_uv + page_coordinate.xy * vt_page_size) / vt_physical_texture_size;

    float3 world_space_normal = calculate_normal(
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

    world_space_normal_texture[pixel_id] = float4(world_space_normal, 0.0f);
    base_color_texture[pixel_id] = base_color;
    pbr_texture[pixel_id] = float4(pbr, 1.0f);
    emissive_texture[pixel_id] = emissive;


    uint2 shadow_page_coordinate = indirect_info.zw;

    float3 world_space_position = world_position_view_depth_texture[pixel_id].xyz;
    float4 shadow_clip = mul(float4(world_space_position + 0.03 * world_space_normal, 1.0f), shadow_view_proj);
    float2 shadow_ndc = shadow_clip.xy / shadow_clip.w;
    float2 shadow_uv = 0.5f + float2(0.5f, -0.5f) * shadow_ndc;
    uint2 internal_uv = (uint2)(shadow_uv * vt_virtual_shadow_resolution) % vt_shadow_page_size;

    shadow_uv_depth_texture[pixel_id] = uint3(
        internal_uv + shadow_page_coordinate * vt_shadow_page_size,
        asuint(shadow_clip.z)
    );
}

#endif