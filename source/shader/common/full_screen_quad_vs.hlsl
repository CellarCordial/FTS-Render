

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float2 uv : TEXCOORD;
};


VertexOutput main(uint vertex_id : SV_VertexID)
{
    VertexOutput output;
    output.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.sv_position = float4(output.uv * float2(2, -2) + float2(-1, 1), 0.001f, 1.0f);
    return output;
}