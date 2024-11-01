#include "Material.hlsli"



struct FPassConstants
{
    float4x4 ViewProj;
    float2 ProjConstants; float2 Pads;
};

struct FGeometryConstants
{
    float4x4 WorldMatrix;
    float4x4 InvTransWorld;

    uint dwMaterialIndex;
    uint3 Pad;
};


ConstantBuffer<FPassConstants>     gPassCB      : register(b0);
ConstantBuffer<FGeometryConstants> gGeometryCB  : register(b1);

SamplerState gSampler : register(s0);

Texture2D gDiffuse           : register(t0);
Texture2D gNormal            : register(t1);
Texture2D gEmissive          : register(t2);
Texture2D gOcclusion         : register(t3);
Texture2D gMetallicRoughness : register(t4);

StructuredBuffer<FMaterial> gMaterials : register(t5);


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
    float3 PositionW : POSITION;
    float3 NormalW   : NORMAL;
    float4 TangentW  : TANGENT;
    float2 UV        : TEXCOORD;
};


FVertexOutput VS(FVertexInput In)
{
    FVertexOutput Out;
    float4 CurrPosW = mul(float4(In.PositionL, 1.0f), gGeometryCB.WorldMatrix);

    Out.PositionH = mul(CurrPosW, gPassCB.ViewProj);
    Out.PositionW = CurrPosW.xyz;
    Out.NormalW = normalize(mul(float4(In.NormalL, 1.0f), gGeometryCB.InvTransWorld)).xyz;
    Out.TangentW = normalize(mul(In.TangentL, gGeometryCB.InvTransWorld));
    Out.UV = In.UV;
    return Out;
}


struct FPixelOutput
{
    float4 Diffuse                           : SV_Target0;
    float4 MetallicRoughnessOcclusion        : SV_Target1;
    float4 Emmisive                          : SV_Target2;
    float4 Normal                            : SV_Target3;
    float4 PositionW                         : SV_Target4;
};

float3 CalcNormal(float3 TextureNormal, float3 VertexNormal, float4 VertexTangent)
{
    float3 UnpackedNormal = TextureNormal * 2.0f - 1.0f;
    float3 N = VertexNormal;
    float3 T = normalize(VertexTangent.xyz - N * dot(VertexTangent.xyz, N));
    float3 B = cross(N, T) * VertexTangent.w;
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(UnpackedNormal, TBN));
}

FPixelOutput PS(FVertexOutput In)
{
    FPixelOutput Out;

    Out.Diffuse = gDiffuse.Sample(gSampler, In.UV) * gMaterials[gGeometryCB.dwMaterialIndex].fDiffuse;
    Out.Normal = float4(CalcNormal(gNormal.Sample(gSampler, In.UV).xyz, In.NormalW, In.TangentW), 1.0f);
    Out.Emmisive = gEmissive.Sample(gSampler, In.UV) * gMaterials[gGeometryCB.dwMaterialIndex].fEmissive;
    float4 Occlusion = gOcclusion.Sample(gSampler, In.UV) * gMaterials[gGeometryCB.dwMaterialIndex].fOcclusion;
    float4 MetallicRoughness = gMetallicRoughness.Sample(gSampler, In.UV);
    MetallicRoughness.r *= gMaterials[gGeometryCB.dwMaterialIndex].fMetallic;
    MetallicRoughness.g *= gMaterials[gGeometryCB.dwMaterialIndex].fRoughness;

    Out.MetallicRoughnessOcclusion.r = MetallicRoughness.r;
    Out.MetallicRoughnessOcclusion.g = MetallicRoughness.g;
    Out.MetallicRoughnessOcclusion.b = Occlusion.r;
    Out.PositionW = float4(In.PositionW, 1.0f);

    return Out;
}