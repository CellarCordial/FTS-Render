#include "../common/gbuffer.hlsl"

cbuffer pass_constants : register(b0)
{
    float4x4 reverse_z_view_proj;
    float4x4 view_matrix;
    
    uint view_mode;
    uint vt_page_size;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;

    float3 color : COLOR;
    float3 world_space_position : WORLD_POSITION;
    float3 view_space_position : VIEW_POSITION;

    float3 world_space_normal : NORMAL;
    float3 world_space_tangent : TANGENT;
    float2 uv : TEXCOORD;

    uint geometry_id : GEOMETRY_ID;
};

struct PixelOutput
{
    float4 world_position_view_depth : SV_Target0;
    float4 geometry_uv_miplevel_id : SV_TARGET1;
    float4 world_space_normal : SV_Target2;
    float4 world_space_tangent : SV_TARGET3;
    float4 base_color : SV_TARGET4;
    float4 pbr : SV_TARGET5;
    float4 emmisive : SV_TARGET6;
    float4 virtual_mesh_visual : SV_TARGET7;
};

StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t0);

uint estimate_mip_level(float2 pixel_id)
{
    float2 dx = ddx(pixel_id);
	float2 dy = ddy(pixel_id);
    return max(0.0f, 0.5f * log2(max(dot(dx, dx), dot(dy, dy))));
}


PixelOutput main(VertexOutput input)
{
    uint mip_level = INVALID_SIZE_32;

    GeometryConstant geometry = geometry_constant_buffer[input.geometry_id];
    if (geometry.texture_resolution != 0)
    {
        mip_level = estimate_mip_level(input.uv * geometry.texture_resolution);
        if (geometry.texture_resolution >> mip_level < vt_page_size)
        {
            mip_level = log2(geometry.texture_resolution / vt_page_size);
        }
    }

    PixelOutput output;
    output.world_position_view_depth = float4(input.world_space_position, input.view_space_position.z);
    output.geometry_uv_miplevel_id = float4(input.uv, asfloat(mip_level), asfloat(input.geometry_id));
    output.world_space_normal = float4(input.world_space_normal, 0.0f);
    output.world_space_tangent = float4(input.world_space_tangent, 0.0f);
    output.base_color = geometry.base_color;
    output.pbr = float4(geometry.metallic, geometry.roughness, geometry.occlusion, 0.0f);
    output.emmisive = geometry.emissive;
    output.virtual_mesh_visual = float4(input.color, 1.0f);
    return output;
}
