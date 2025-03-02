#include "../common/gbuffer.hlsl"


cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;
    uint page_size;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;
    uint2 page_id : PAGE_ID;
};

RWTexture2D<uint> vt_physical_shadow_texture : register(u0);


float4 main(VertexOutput input) : SV_Target0
{
    uint2 pixel_id = uint2(input.sv_position.xy);
    uint2 uv = pixel_id + input.page_id * page_size;

    float depth = input.sv_position.z / input.sv_position.w;
    InterlockedMin(vt_physical_shadow_texture[uv], asuint(depth));

    return float4(depth, float2(input.page_id), 0.0f);
}
