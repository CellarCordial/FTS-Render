

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float2 uv : TEXCOORD;
};

VertexOutput VS(uint vertex_id: SV_VertexID)
{
    // Full screen quad.
    VertexOutput out;
    out.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    out.sv_position = float4(out.uv * float2(2, -2) + float2(-1, 1), 0.5f, 1.0f);
    return out;
}
