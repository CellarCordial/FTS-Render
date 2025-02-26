#include "../common/gbuffer.hlsl"


cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;
    float4x4 view_matrix;
    uint2 tile_size_in_pixel;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float3 view_space_position : VIEW_POSITION;
    uint2 tile_id : TILE_ID;
};

RWTexture2D<uint> physical_shadow_map_texture : register(u0);


float4 main(VertexOutput input) : SV_Target
{
    uint2 pixel_id = uint2(input.sv_position.xy);
    uint2 uv = pixel_id - input.tile_id * tile_size_in_pixel;

    InterlockedMin(physical_shadow_map_texture[uv], asuint(input.view_space_position.z));
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
