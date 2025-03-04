
cbuffer pass_constants : register(b0)
{
    float4x4 world_matrix;
    float4x4 view_proj;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;
    uint2 page_id : PAGE_ID;
};


VertexOutput main(float3 Position : POSITION)
{
    return mul(mul(float4(Position, 1.0f), world_matrix), view_proj);
}

