
cbuffer pass_constants : register(b0)
{
    float4x4 world_matrix;
    float4x4 view_proj;
};


float4 main(float3 Position : POSITION) : SV_POSITION
{
    return mul(mul(float4(Position, 1.0f), world_matrix), view_proj);
}

