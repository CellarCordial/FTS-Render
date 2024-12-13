
cbuffer pass_constant
{
    uint show_type;
};

Texture2D<float4> world_space_position_depth_texture : register(t0);
Texture2D<float3> normal_texture : register(t1);
Texture2D<float4> base_color_texture : register(t2);
Texture2D<float4> pbr_texture : register(t3);
Texture2D<float4> emissive_texture : register(t4);

SamplerState sampler : register(s0);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float2 uv : TEXCOORD;
};

[shader("pixel")]
float4 pixel_shader(VertexOutput input) : SV_Target0
{
    float4 color;
    switch (show_type)
    {
    case 0: color = float4(world_space_position_depth_texture.Sample(sampler, input.uv).xyz, 1.0f); break;
    case 1: color = float4(world_space_position_depth_texture.Sample(sampler, input.uv).x, 0.0f, 0.0f, 1.0f); break;
    case 2: color = float4(normal_texture.Sample(sampler, input.uv).xyz, 1.0f); break;
    case 3: color = base_color_texture.Sample(sampler, input.uv); break;
    case 4: color = float4(pbr_texture.Sample(sampler, input.uv).x, 0.0f, 0.0f, 1.0f); break;
    case 5: color = float4(pbr_texture.Sample(sampler, input.uv).y, 0.0f, 0.0f, 1.0f); break;
    case 6: color = float4(pbr_texture.Sample(sampler, input.uv).z, 0.0f, 0.0f, 1.0f); break;
    case 7: color = emissive_texture.Sample(sampler, input.uv); break;
    };

    return color;
}