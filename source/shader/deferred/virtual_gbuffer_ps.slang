#include "../gbuffer.slang"

cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;

    float4x4 view_matrix;
    float4x4 prev_view_matrix;

    uint32_t view_mode;
    uint32_t mip_level;
    uint32_t vt_page_size;
    uint32_t client_width;
};

struct VertexOutput
{
    float3 color : COLOR;
    float4 sv_position : SV_Position;

    float3 world_space_position : WORLD_POSITION;

    float3 view_space_position : VIEW_POSITION;
    float3 prev_view_space_position : PREV_VIEW_POSITION;
    
    float3 world_space_normal : NORMAL;
    float3 world_space_tangent  : TANGENT;
    float2 uv : TEXCOORD;

    uint32_t geometry_id : GEOMETRY_ID;
};

struct PixelOutput
{
    float4 world_position_view_depth : SV_Target0;
    float3 view_space_velocity : SV_TARGET1;
    float2 tile_uv : SV_Target2;

    float3 world_space_normal : SV_TARGET3;
    float3 world_space_tangent : SV_TARGET4;
    float4 base_color : SV_TARGET5;
    float3 pbr : SV_TARGET6;
    float4 emmisive : SV_TARGET7;
};

StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t0);

struct VTPageInfo
{
    uint32_t geometry_id;
    uint32_t page_id_mip_level;
};

RWStructuredBuffer<VTPageInfo> vt_page_info_buffer : register(u0);

uint32_t estimate_mip_level(float2 texture_coordinate);

[shader("pixel")]
PixelOutput pixel_shader(VertexOutput input)
{
    GeometryConstant geometry = geometry_constant_buffer[input.geometry_id];
    uint2 page_id = (uint2)floor(input.uv * (geometry.texture_resolution / vt_page_size));
    uint2 page_id_high_bit = (page_id >> 8) & 0xf;
    uint2 page_id_low_bit = page_id & 0xff;

    uint2 geometry_texture_pixel_id = uint2(input.uv * geometry.texture_resolution);
    uint32_t mip_level = estimate_mip_level(geometry_texture_pixel_id);

    VTPageInfo info;
    info.geometry_id = input.geometry_id;
    info.page_id_mip_level = uint32_t(
        (page_id_low_bit.x << 24) |
        (page_id_low_bit.y << 16) |
        (page_id_high_bit.x << 12) |
        (page_id_high_bit.y << 8) |
        (mip_level & 0xff)
    );

    uint2 pixel_id = (uint2)input.sv_position.xy;
    uint32_t pixel_index = pixel_id.x + pixel_id.y * client_width;
    vt_page_info_buffer[pixel_index] = info;

    PixelOutput output;
    output.world_position_view_depth = float4(input.world_space_position, input.view_space_position.z);
    output.view_space_velocity = input.prev_view_space_position - input.view_space_position;

    uint32_t vt_mip_page_size = vt_page_size << mip_level;
    uint2 mip_page_id = (uint2)floor(input.uv * (geometry.texture_resolution / vt_mip_page_size));
    output.tile_uv = (geometry_texture_pixel_id - mip_page_id * vt_mip_page_size) / vt_mip_page_size;
    
    output.world_space_normal = input.world_space_normal;
    output.world_space_tangent = input.world_space_tangent;
    output.base_color = geometry.base_color;
    output.pbr = float3(geometry.metallic, geometry.roughness, geometry.occlusion);
    output.emmisive = geometry.emissive;
    return output;
}

uint32_t estimate_mip_level(float2 texture_coordinate)
{
    float2 dx_vtc = ddx(texture_coordinate);
    float2 dy_vtc = ddy(texture_coordinate);
    float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));
    float mml = 0.5 * log2(delta_max_sqr);
    return uint32_t(max(0.0f, round(mml)));
}