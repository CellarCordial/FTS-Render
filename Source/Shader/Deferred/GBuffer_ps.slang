#include "../Material.slang"
#include "../octahedral.slang"

cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;

    float4x4 view_matrix;
    float4x4 PrevViewMatrix;
};

cbuffer FGeometryConstants : register(b1)
{
    float4x4 world_matrix;
    float4x4 inv_trans_world;

    uint material_index;
    uint3 pad;
};


SamplerState gSampler : register(s0);

Texture2D gDiffuse           : register(t0);
Texture2D gNormal            : register(t1);
Texture2D gEmissive          : register(t2);
Texture2D gOcclusion         : register(t3);
Texture2D gMetallicRoughness : register(t4);

StructuredBuffer<Material> gMaterials : register(t5);

struct VertexOutput
{
    float4 sv_position : SV_Position;

    float3 world_space_position : WORLD_POSITION;

    float3 view_space_position : VIEW_POSITION;
    float3 prev_view_space_position : PREV_VIEW_POSITION;

    float3 world_space_normal : NORMAL;
    float4 world_space_tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct FPixelOutput
{
    float3 view_space_position : SV_Target0;
    float3 view_space_normal : SV_Target1;
    float4 base_color : SV_Target2;
    float4 pbr : SV_Target3;
    float4 emmisive : SV_Target4;
    float3 view_space_velocity : SV_Target5;
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
FPixelOutput pixel_shader(VertexOutput In)
{
    FPixelOutput Out;
    Out.view_space_position = In.view_space_position;

    float3 normal = CalcNormal(gNormal.Sample(gSampler, In.uv).xyz, In.world_space_normal, In.world_space_tangent);
    Out.view_space_normal = normalize(mul(float4(normal, 1.0f), view_matrix).xyz);

    Out.base_color = gDiffuse.Sample(gSampler, In.uv) * gMaterials[material_index].fDiffuse;
    if (Out.base_color.a < 0.001f) discard;
    
    float4 MetallicRoughness = gMetallicRoughness.Sample(gSampler, In.uv);
    MetallicRoughness.r *= gMaterials[material_index].fMetallic;
    MetallicRoughness.g *= gMaterials[material_index].fRoughness;
    float fOcclusion = gOcclusion.Sample(gSampler, In.uv).r;
    Out.pbr = float4(MetallicRoughness.rg, fOcclusion, 1.0f);

    Out.emmisive = gEmissive.Sample(gSampler, In.uv);

    Out.view_space_velocity = In.prev_view_space_position - In.view_space_position;

    return Out;
}