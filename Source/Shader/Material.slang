#ifndef SHADER_MATERIAL_SLANG
#define SHADER_MATERIAL_SLANG

struct FMaterialFactor
{
    float4 fDiffuseFactor;
    float  fRoughnessFactor;
    float  fMetallicFactor;
    float  fOcclusionFactor;
    float  fEmissiveFactor;
};

struct FMaterial
{
    float4 fDiffuse;
    float  fRoughness;
    float  fMetallic;
    float  fOcclusion;
    float4  fEmissive;
};


#endif