cbuffer gPassConstants : register(b0)
{
    float4x4 ViewProj;
    float4x4 WorldMatrices[6];
};

SamplerState gSampler : register(s0);

Texture2D gDiffuse           : register(t0);
Texture2D gNormal            : register(t1);
Texture2D gEmissive          : register(t2);
Texture2D gOcclusion         : register(t3);
Texture2D gMetallicRoughness : register(t4);


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


FVertexOutput VS(FVertexInput In, uint dwInstanceID : SV_InstanceID)
{
    float4 PosW = mul(float4(In.PositionL, 1.0f),  WorldMatrices[dwInstanceID]);

    FVertexOutput Out;
    Out.PositionH = mul(PosW, ViewProj);
    Out.PositionW = PosW.xyz;
    Out.NormalW = In.NormalL;
    Out.TangentW = In.TangentL;
    Out.UV = In.UV;
    
    return Out;
}


struct FPixelOutput
{
    float4 Color    : SV_Target0;
    float4 Normal   : SV_Target1;
    float4 PBR      : SV_Target2;
    float4 Emmisive : SV_Target3;
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

    Out.Color = gDiffuse.Sample(gSampler, In.UV);
    Out.Normal = float4(CalcNormal(gNormal.Sample(gSampler, In.UV).xyz, In.NormalW, In.TangentW), 1.0f);
    
    float fOcclusion = gOcclusion.Sample(gSampler, In.UV).r;
    float2 MetallicRoughness = gMetallicRoughness.Sample(gSampler, In.UV).rg;
    Out.PBR = float4(MetallicRoughness, fOcclusion, 1.0f);

    Out.Emmisive = gEmissive.Sample(gSampler, In.UV);
    return Out;
}