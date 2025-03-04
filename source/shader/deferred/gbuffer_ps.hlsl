#include "../common/gbuffer.hlsl"


cbuffer pass_constants : register(b0)
{
    float4x4 reverse_z_view_proj;
    float4x4 view_matrix;

    uint geometry_id;
    uint vt_page_size;
};

StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t6);

struct VertexOutput
{
    float4 sv_position : SV_Position;

    float3 world_space_position : WORLD_POSITION;
    float3 view_space_position : VIEW_POSITION;
    
    float3 world_space_normal : NORMAL;
    float3 world_space_tangent  : TANGENT;
    float2 uv        : TEXCOORD;
};


struct PixelOutput
{
    float4 world_position_view_depth : SV_Target0;
    float4 geometry_uv_mip_id : SV_TARGET1;
    float4 world_space_normal : SV_Target2;
    float4 world_space_tangent : SV_TARGET3;
    float4 base_color : SV_TARGET4;
    float4 pbr : SV_TARGET5;
    float4 emmisive : SV_TARGET6;
};


uint estimate_mip_level(float2 pixel_id)
{
    float2 dx = ddx(pixel_id);
	float2 dy = ddy(pixel_id);
    return max(0.0f, 0.5f * log2(max(dot(dx, dx), dot(dy, dy))));
}


PixelOutput main(VertexOutput input)
{
    uint mip_level = INVALID_SIZE_32;

    GeometryConstant geometry = geometry_constant_buffer[geometry_id];
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
    output.geometry_uv_mip_id = float4(input.uv, asfloat(mip_level), asfloat(geometry_id));
    output.world_space_normal = float4(input.world_space_normal, 0.0f);
    output.world_space_tangent = float4(input.world_space_tangent, 0.0f);
    output.base_color = geometry.base_color;
    output.pbr = float4(geometry.metallic, geometry.roughness, geometry.occlusion, 0.0f);
    output.emmisive = geometry.emissive;
    return output;
}
