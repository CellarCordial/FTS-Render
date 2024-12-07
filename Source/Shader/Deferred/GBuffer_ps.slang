#include "../Material.slang"
#include "../Octohedral.slang"

cbuffer FGeometryConstants : register(b1)
{
    float4x4 WorldMatrix;
    float4x4 InvTransWorld;

    uint dwMaterialIndex;
    uint3 Pad;
};


SamplerState gSampler : register(s0);

Texture2D gDiffuse           : register(t0);
Texture2D gNormal            : register(t1);
Texture2D gEmissive          : register(t2);
Texture2D gOcclusion         : register(t3);
Texture2D gMetallicRoughness : register(t4);

StructuredBuffer<FMaterial> gMaterials : register(t5);

struct FVertexOutput
{
    float4 PositionH : SV_Position;

    float3 PositionW : WORLD_POSITION;

    float4 PositionV : VIEW_POSITION;
    float4 PositionP : PROJ_POSITION;
    float4 PrevPositionV : PREV_VIEW_POSITION;
    float4 PrevPositionP : PREV_PROJ_POSITION;

    float3 NormalW : NORMAL;
    float4 TangentW : TANGENT;
    float2 UV : TEXCOORD;
};

struct FPixelOutput
{
    float4 PosDepth : SV_Target0;
    float2 Normal : SV_Target1;
    float4 BaseColor : SV_Target2;
    float4 PBR : SV_Target3;
    float4 Emmisive : SV_Target4;
    float3 VelocityView : SV_Target5;
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

[shader("pixel")]
FPixelOutput PS(FVertexOutput In)
{
    FPixelOutput Out;
    Out.PosDepth = float4(In.PositionW, In.PositionV.z);
    
    float3 Normal = CalcNormal(gNormal.Sample(gSampler, In.UV).xyz, In.NormalW, In.TangentW);
    Out.Normal = UnitVectorToOctahedron(Normal);

    Out.BaseColor = gDiffuse.Sample(gSampler, In.UV) * gMaterials[dwMaterialIndex].fDiffuse;
    if (Out.BaseColor.a < 0.001f) discard;
    
    float4 MetallicRoughness = gMetallicRoughness.Sample(gSampler, In.UV);
    MetallicRoughness.r *= gMaterials[dwMaterialIndex].fMetallic;
    MetallicRoughness.g *= gMaterials[dwMaterialIndex].fRoughness;
    float fOcclusion = gOcclusion.Sample(gSampler, In.UV).r;
    Out.PBR = float4(MetallicRoughness.rg, fOcclusion, 1.0f);

    Out.Emmisive = gEmissive.Sample(gSampler, In.UV);

    float2 PositionS = In.PositionP.xy / In.PositionP.w;
    float2 PrevPositionS = In.PrevPositionP.xy / In.PrevPositionP.w;
    Out.VelocityView = float3(In.PrevPositionV.xyz - In.PositionV.xyz);

    return Out;
}