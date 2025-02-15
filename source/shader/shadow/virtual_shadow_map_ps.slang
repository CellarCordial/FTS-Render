#include "../common/gbuffer.slang"

[vk_binding(0, 0)]
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

[vk_binding(1, 0)] RWTexture2D<float> physical_shadow_map_texture : register(u0);

[shader("pixel")]
void main(VertexOutput input)
{
    uint2 pixel_id = (uint2)round(input.sv_position.xy);
    uint2 uv = pixel_id - input.tile_id * tile_size_in_pixel;
    InterlockedMin(physical_shadow_map_texture[uv], input.view_space_position.z);
}
