
cbuffer pass_constants : register(b0)
{
    float step_size;
    float color_phi;
    float normal_phi;
    float position_phi;

    uint2 window_resolution;
};

Texture2D<float4> color_texture : register(t0);
Texture2D<float3> view_space_normal_texture : register(t1);
Texture2D<float3> view_space_position_texture : register(t2);

SamplerState sampler : register(s0);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float2 uv : TEXCOORD;
};

#define KERNEL_SIZE 25

static const float kernel[KERNEL_SIZE] =
{
    1.0f / 256.0f, 1.0f / 64.0f, 3.0f / 128.0f, 1.0f / 64.0f, 1.0f / 256.0f,
    1.0f / 64.0f, 1.0f / 16.0f, 3.0f / 32.0f, 1.0f / 16.0f, 1.0f / 64.0f,
    3.0f / 128.0f, 3.0f / 32.0f, 9.0f / 64.0f, 3.0f / 32.0f, 3.0f / 128.0f,
    1.0f / 64.0f, 1.0f / 16.0f, 3.0f / 32.0f, 1.0f / 16.0f, 1.0f / 64.0f,
    1.0f / 256.0f, 1.0f / 64.0f, 3.0f / 128.0f, 1.0f / 64.0f, 1.0f / 256.0f
};

static const float2 offset[KERNEL_SIZE] =
{
    { -2, -2 }, { -1, -2 }, { 0, -2 }, { 1, -2 }, { 2, -2 }, 
    { -2, -1 }, { -1, -1 }, { 0, -1 }, { 1, -1 }, { 2, -1 }, 
    { -2, 0 }, { -1, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 }, 
    { -2, 1 }, { -1, 1 }, { 0, 1 }, { 1, 1 }, { 2, 1 },
    { -2, 2 }, { -1, 2 }, { 0, 2 }, { 1, 2 }, { 2, 2 }
};

float4 pixel_shader(VertexOutput input) : SV_Target0
{
    float2 uv = input.uv;
    uint2 ori_pixel_id = uint2(window_resolution * uv);

    float4 ori_color = color_texture[ori_pixel_id];
    float3 ori_view_space_normal = view_space_normal_texture[ori_pixel_id];
    float3 ori_view_space_position = view_space_position_texture[ori_pixel_id];

    float4 color_sum = float4(0, 0, 0, 0);
    float weight_sum = 0.0f;

    for (uint ix = 0; ix < KERNEL_SIZE; ++ix)
    {
        uint2 pixel_id = uint2(ori_pixel_id + offset[ix] * step_size);

        float4 color = color_texture[pixel_id];
        float3 delta = (ori_color - color).xyz;
        float delta2_sum = dot(delta, delta);
        float color_weight = min(exp(-delta2_sum / color_phi), 1.0f);

        float3 view_space_normal = view_space_normal_texture[pixel_id];
        delta = (ori_view_space_normal - view_space_normal).xyz;
        delta2_sum = max(dot(delta, delta) / step_size * step_size, 0.0f);
        float normal_weight = min(exp(-delta2_sum / normal_phi), 1.0f);

        float3 view_space_position = view_space_position_texture[pixel_id];
        delta = (ori_view_space_position - view_space_position).xyz;
        delta2_sum = dot(delta, delta);
        float position_weight = min(exp(-delta2_sum / position_phi), 1.0f);

        float weight = color_weight * normal_weight * position_weight;

        color_sum += color * weight;
        weight_sum += weight;
    }

    return color_sum / weight_sum;
}

