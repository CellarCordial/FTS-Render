
cbuffer gPassConstant : register(b0)
{
    float4x4 world_matrix;
    float4x4 view_proj;
};

[shader("vertex")]
float4 vertex_shader(float3 Position : POSITION) : SV_POSITION
{
    return mul(mul(float4(Position, 1.0f), world_matrix), view_proj);
}

