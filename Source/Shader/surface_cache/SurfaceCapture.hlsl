cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;
    float4x4 WorldMatrices[6];
};

SamplerState gSampler : register(s0);

Texture2D gDiffuse           : register(t0);
Texture2D gNormal            : register(t1);
Texture2D gEmissive          : register(t2);
Texture2D gOcclusion         : register(t3);
Texture2D gMetallicRoughness : register(t4);


struct VertexInput
{
    float3 local_space_position : POSITION;
    float3 local_space_normal   : NORMAL;
    float4 local_space_tangent  : TANGENT;
    float2 uv        : TEXCOORD;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float3 world_space_position : POSITION;
    float3 world_space_normal   : NORMAL;
    float4 world_space_tangent  : TANGENT;
    float2 uv        : TEXCOORD;
};


VertexOutput vertex_shader(VertexInput In, uint instance_id : SV_InstanceID)
{
    float4 PosW = mul(float4(In.local_space_position, 1.0f),  WorldMatrices[instance_id]);

    VertexOutput Out;
    Out.sv_position = mul(PosW, view_proj);
    Out.world_space_position = PosW.xyz;
    Out.world_space_normal = In.local_space_normal;
    Out.world_space_tangent = In.local_space_tangent;
    Out.uv = In.uv;
    
    return Out;
}


struct FPixelOutput
{
    float4 Color    : SV_Target0;
    float4 normal   : SV_Target1;
    float4 pbr      : SV_Target2;
    float4 emmisive : SV_Target3;
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

FPixelOutput pixel_shader(VertexOutput In)
{
    FPixelOutput Out;

    Out.Color = gDiffuse.Sample(gSampler, In.uv);
    Out.normal = float4(CalcNormal(gNormal.Sample(gSampler, In.uv).xyz, In.world_space_normal, In.world_space_tangent), 1.0f);
    
    float fOcclusion = gOcclusion.Sample(gSampler, In.uv).r;
    float2 MetallicRoughness = gMetallicRoughness.Sample(gSampler, In.uv).rg;
    Out.pbr = float4(MetallicRoughness, fOcclusion, 1.0f);

    Out.emmisive = gEmissive.Sample(gSampler, In.uv);
    return Out;
}