#include "../post_process_helper.slang"

cbuffer gPassConstants : register(b0)
{
    uint2 window_resolution;
};

Texture2D<float4> color_texture : register(t0);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float2 uv : TEXCOORD;
};

#define KERNEL_SIZE 3

static const float kernel[3] = { 1.0f, 2.0f / 3.0f, 1.0f / 6.0f };

float4 PS(VertexOutput input) : SV_Target0
{
    float2 uv = input.uv;
    uint2 ori_pixel_id = uint2(window_resolution * uv);
}
