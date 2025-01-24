
cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;
    float4x4 world_matrix[6];
};

struct VertexInput
{
    float3 local_space_position : POSITION;
    float3 local_space_normal : NORMAL;
    float3 local_space_tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;

    float3 world_space_normal : NORMAL;
    float3 world_space_tangent : TANGENT;
    float2 uv : TEXCOORD;
};


[shader("vertex")]
VertexOutput main(VertexInput input, uint instance_id : SV_InstanceID)
{
    VertexOutput output;
    output.sv_position = mul(float4(input.local_space_position, 1.0f), view_proj);
    output.world_space_normal = mul(float4(input.local_space_normal, 1.0f), world_matrix[instance_id]).xyz;
    output.world_space_tangent = mul(float4(input.local_space_tangent, 1.0f), world_matrix[instance_id]).xyz;
    output.uv = input.uv;
    return output;
}