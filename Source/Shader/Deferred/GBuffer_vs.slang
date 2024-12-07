#include "../Material.slang"
#include "../Octohedral.slang"

cbuffer gPassConstants : register(b0)
{
    float4x4 ViewProj;

    float4x4 View;
    float4x4 PrevView;
};

cbuffer FGeometryConstants : register(b1)
{
    float4x4 WorldMatrix;
    float4x4 InvTransWorld;

    uint dwMaterialIndex;
    uint3 Pad;
};

struct FVertexInput
{
    float3 PositionL : POSITION;
    float3 NormalL   : NORMAL;
    float4 TangentL  : TANGENT;
    float2 UV        : TEXCOORD;
};

struct FVertexOutput
{
    float4 PositionH : SV_Position;

    float3 PositionW : WORLD_POSITION;

    float4 PositionV : VIEW_POSITION;
    float4 PrevPositionV : PREV_VIEW_POSITION;
    
    float3 NormalW   : NORMAL;
    float4 TangentW  : TANGENT;
    float2 UV        : TEXCOORD;
};

[shader("vertex")]
FVertexOutput VS(FVertexInput In)
{
    FVertexOutput Out;
    float4 WorldPos = mul(float4(In.PositionL, 1.0f), WorldMatrix);

    Out.PositionH = mul(WorldPos, ViewProj);

    Out.PositionW = WorldPos.xyz;

    Out.PositionV = mul(WorldPos, View);
    Out.PrevPositionV = mul(WorldPos, PrevView);

    Out.NormalW = normalize(mul(float4(In.NormalL, 1.0f), InvTransWorld)).xyz;
    Out.TangentW = normalize(mul(In.TangentL, InvTransWorld));
    Out.UV = In.UV;

    return Out;
}

