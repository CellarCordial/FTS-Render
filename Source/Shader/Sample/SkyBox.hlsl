
struct FPassConstants
{
    float4x4 ViewProj;
    float4x4 WorldMatrix;
    float3 CameraPosition; float Pad;
};

ConstantBuffer<FPassConstants> gPassCB : register(b0);
SamplerState gSampler : register(s0);
TextureCube gSkyBox : register(t0);

struct FVertexInput
{
    float3 PositionL    : POSITION;
};

struct FVertexOutput
{
    float4 PositionH    : SV_Position;
    float3 PositionL    : POSITION;
};


FVertexOutput VS(FVertexInput In)
{
    float3 PositionW = mul(float4(In.PositionL, 1.0f), gPassCB.WorldMatrix).xyz;
    PositionW += gPassCB.CameraPosition;

    FVertexOutput Out;
    Out.PositionL = In.PositionL;
    Out.PositionH = mul(float4(PositionW, 1.0f), gPassCB.ViewProj);
    return Out;
}

float4 PS(FVertexOutput In) : SV_Target0
{
    // 天空盒始终跟随摄像机, 所以可以直接使用物体空间位置.
    return gSkyBox.Sample(gSampler, In.PositionL);
}