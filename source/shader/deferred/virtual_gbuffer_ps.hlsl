#include "../common/gbuffer.hlsl"


cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;

    float4x4 view_matrix;
    float4x4 prev_view_matrix;
    
    float4x4 shadow_view_proj;

    uint view_mode;
    uint vt_page_size;
    uint virtual_shadow_resolution;
    uint virtual_shadow_page_size;
    
    float3 camera_position;
    uint client_width;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;

    float3 color : COLOR;
    float3 world_space_position : WORLD_POSITION;

    float3 view_space_position : VIEW_POSITION;
    float3 prev_view_space_position : PREV_VIEW_POSITION;

    float3 world_space_normal : NORMAL;
    float3 world_space_tangent : TANGENT;
    float2 uv : TEXCOORD;

    uint geometry_id : GEOMETRY_ID;
};

struct PixelOutput
{
    float4 world_position_view_depth : SV_Target0;
    float4 view_space_velocity : SV_TARGET1;
    float4 world_space_normal : SV_Target2;
    float4 world_space_tangent : SV_TARGET3;
    float4 base_color : SV_TARGET4;
    float4 pbr : SV_TARGET5;
    float4 emmisive : SV_TARGET6;
    float4 virtual_mesh_visual : SV_TARGET7;
};

StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t0);

RWTexture2D<uint2> vt_page_uv_texture : register(u0);
RWStructuredBuffer<uint2> vt_page_buffer : register(u1);
RWStructuredBuffer<uint2> virtual_shadow_page_buffer : register(u2);

uint estimate_mip_level(float2 pixel_id)
{
    float2 dx = ddx(pixel_id);
	float2 dy = ddy(pixel_id);
    return max(0.0f, 0.5f * log2(max(dot(dx, dx), dot(dy, dy))));
}


#ifdef VT_FEED_BACK_SCALE_FACTOR

PixelOutput main(VertexOutput input)
{
    uint2 pixel_id = uint2(input.sv_position.xy);
    uint pixel_index = pixel_id.x + pixel_id.y * client_width;

    GeometryConstant geometry = geometry_constant_buffer[input.geometry_id];
    if (all(geometry.texture_resolution != 0))
    {
        uint mip_level = estimate_mip_level(input.uv * geometry.texture_resolution);
        if (geometry.texture_resolution >> mip_level < vt_page_size)
        {
            mip_level = log2(geometry.texture_resolution / vt_page_size);
        }
        uint2 geometry_texture_resolution = max(geometry.texture_resolution >> mip_level, vt_page_size);
        uint2 geometry_texture_pixel_id = uint2(input.uv * geometry_texture_resolution);

        vt_page_uv_texture[pixel_id] = geometry_texture_pixel_id % vt_page_size;

        if (((pixel_id.x | pixel_id.y) & (VT_FEED_BACK_SCALE_FACTOR - 1)) == 0)
        {
            uint2 page_id = geometry_texture_pixel_id / vt_page_size;
            uint page_id_mip_level = uint(
                (page_id.x << 20) |
                (page_id.y << 8) |
                (mip_level & 0xff)
            );

            uint2 feed_back_id = pixel_id / VT_FEED_BACK_SCALE_FACTOR;
            uint feed_back_index = feed_back_id.x + feed_back_id.y * (client_width / VT_FEED_BACK_SCALE_FACTOR);

            vt_page_buffer[feed_back_index] = uint2(input.geometry_id, page_id_mip_level);
        }
    }
    
    // float4 shadow_view_proj_pos = mul(float4(input.world_space_position, 1.0f), shadow_view_proj);
    // shadow_view_proj_pos.xyz = shadow_view_proj_pos.xyz / shadow_view_proj_pos.z;

    // // 是否在太阳投影的裁剪空间内.
    // bool in_clip = shadow_view_proj_pos.w > 0.0f &&
    //                shadow_view_proj_pos.x >= -1.0f && shadow_view_proj_pos.x <= 1.0f &&
    //                shadow_view_proj_pos.y >= -1.0f && shadow_view_proj_pos.y <= 1.0f &&
    //                shadow_view_proj_pos.z >= 0.0f && shadow_view_proj_pos.z <= 1.0f;
    // if (in_clip)
    // {
    //     float2 uv = shadow_view_proj_pos.xy * float2(0.5f, -0.5f) + 0.5f;
    //     uint2 shadow_page_id = (uint2)(uv * virtual_shadow_resolution) / virtual_shadow_page_size;

    //     virtual_shadow_page_buffer[pixel_index] = shadow_page_id;
    // }


    PixelOutput output;
    output.world_position_view_depth = float4(input.world_space_position, input.view_space_position.z);
    output.view_space_velocity = float4(input.prev_view_space_position - input.view_space_position, 0.0f);
    output.world_space_normal = float4(input.world_space_normal, 0.0f);
    output.world_space_tangent = float4(input.world_space_tangent, 0.0f);
    output.base_color = geometry.base_color;
    output.pbr = float4(geometry.metallic, geometry.roughness, geometry.occlusion, 0.0f);
    output.emmisive = geometry.emissive;
    output.virtual_mesh_visual = float4(input.color, 1.0f);
    return output;
}

#endif
