
cbuffer gPassConstant : register(b0)
{
    float4x4 WorldMatrix;
    float4x4 ViewProj;
};

float4 VS(float3 Position : POSITION) : SV_POSITION
{
    return mul(mul(float4(Position, 1.0f), WorldMatrix), ViewProj);
}

