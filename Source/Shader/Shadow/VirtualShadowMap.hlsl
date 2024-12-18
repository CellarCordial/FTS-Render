
cbuffer gPassConstant : register(b0)
{
    float4x4 world_matrix;
    float4x4 view_proj;
};

float4 vertex_shader(float3 position : POSITION) : SV_POSITION
{
    return mul(mul(float4(position, 1.0f), world_matrix), view_proj);
}

