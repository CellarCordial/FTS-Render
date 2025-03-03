// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1
// #define VT_TEXTURE_MIP_LEVELS_UINT_4 2

#include "../common/gbuffer.hlsl"

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(VT_TEXTURE_MIP_LEVELS_UINT_4)

cbuffer pass_constants : register(b0)
{
    uint2 client_resolution;
    uint vt_page_size;
    uint vt_physical_texture_size;

    uint4 vt_texture_mip_offset_uint4[VT_TEXTURE_MIP_LEVELS_UINT_4];
    uint4 vt_axis_mip_tile_num_uint4[VT_TEXTURE_MIP_LEVELS_UINT_4];
    uint vt_texture_id_offset;
};

StructuredBuffer<uint> vt_indirect_buffer : register(t0);

Texture2D<uint4> vt_tile_uv_texture : register(t1);
Texture2D<float4> vt_base_color_physical_texture : register(t2);
Texture2D<float3> vt_normal_physical_texture : register(t3);
Texture2D<float3> vt_pbr_physical_texture : register(t4);
Texture2D<float4> vt_emissive_physical_texture : register(t5);
Texture2D<float4> world_position_view_depth_texture : register(t6);
Texture2D<float4> world_space_tangent_texture : register(t6);
Texture2D<float4> geometry_uv_mip_id_texture : register(t7);

RWTexture2D<float4> world_space_normal_texture : register(u0);
RWTexture2D<float4> base_color_texture : register(u1);
RWTexture2D<float4> pbr_texture : register(u2);
RWTexture2D<float4> emissive_texture : register(u3);

SamplerState linear_clamp_sampler : register(s0);


uint get_mip_offset(uint mip)
{
    return vt_texture_mip_offset_uint4[mip / 4][mip % 4];
}

uint get_axis_tile_num(uint mip)
{
    return vt_axis_mip_tile_num_uint4[mip / 4][mip % 4];
}


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    if (thread_id.x >= client_resolution.x || thread_id.y >= client_resolution.y) return;

    uint2 pixel_id = thread_id.xy;
    float4 geometry_uv_mip_id = geometry_uv_mip_id_texture[pixel_id];

    uint geometry_id = asuint(geometry_uv_mip_id.w);
    if (geometry_id == INVALID_SIZE_32) return;

    uint4 tile_uv = vt_tile_uv_texture[pixel_id];
    uint2 interal_uv = tile_uv.xy;

    uint mip = asuint(geometry_uv_mip_id.z);
    uint2 tile_id = tile_uv.zw;

    uint indirect_index = vt_texture_id_offset * geometry_id + 
                          get_mip_offset(mip) + tile_id.x + tile_id.y * get_axis_tile_num(mip);

    uint indirect_info = vt_indirect_buffer[indirect_index];
    uint2 page_id = uint2(indirect_info >> 16, indirect_info & 0xffff);

    float2 physical_uv = float2(interal_uv + page_id.xy * vt_page_size) / vt_physical_texture_size;

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
}

#endif