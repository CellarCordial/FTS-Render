#include "../common/gbuffer.hlsl"


cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;
    float4x4 view_matrix;
    uint2 page_size;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float3 view_space_position : VIEW_POSITION;
    uint2 page_id : PAGE_ID;
};

RWTexture2D<uint> physical_shadow_map_texture : register(u0);


void main(VertexOutput input)
{
    uint2 pixel_id = uint2(input.sv_position.xy);
    uint2 uv = pixel_id + input.page_id * page_size;

    InterlockedMin(physical_shadow_map_texture[uv], asuint(input.view_space_position.z));
}
