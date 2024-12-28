
cbuffer pass_constant
{
    int show_type;
};

Texture2D<float4> world_position_view_depth_texture : register(t0);
Texture2D<float3> world_space_normal_texture : register(t1);
Texture2D<float4> base_color_texture : register(t2);
Texture2D<float4> pbr_texture : register(t3);
Texture2D<float4> emissive_texture : register(t4);

// Texture2D<float> shadow_map_texture : register(t5);

SamplerState sampler0 : register(s0);

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
    case 0: color = base_color_texture.Sample(sampler0, input.uv); break;
    case 1: color = float4(world_position_view_depth_texture.Sample(sampler0, input.uv).xyz, 1.0f); break;
    case 2: color = float4(world_position_view_depth_texture.Sample(sampler0, input.uv).w, 0.0f, 0.0f, 1.0f); break;
    case 3: color = float4(world_space_normal_texture.Sample(sampler0, input.uv).xyz, 1.0f); break;
    case 4: color = base_color_texture.Sample(sampler0, input.uv); break;
    case 5: color = float4(pbr_texture.Sample(sampler0, input.uv).x, 0.0f, 0.0f, 1.0f); break;
    case 6: color = float4(pbr_texture.Sample(sampler0, input.uv).y, 0.0f, 0.0f, 1.0f); break;
    case 7: color = float4(pbr_texture.Sample(sampler0, input.uv).z, 0.0f, 0.0f, 1.0f); break;
    case 8: color = emissive_texture.Sample(sampler0, input.uv); break;
    // case 9: color = float4(shadow_map_texture.Sample(sampler0, input.uv), 0.0f, 0.0f, 1.0f); break;
    };

    return color;
}