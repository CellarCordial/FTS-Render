#ifndef SHADER_MATERIAL_SLANG
#define SHADER_MATERIAL_SLANG

struct FMaterial
{
    float4 fDiffuse;
    float  fRoughness;
    float  fMetallic;
    float  fOcclusion;
    float4  fEmissive;
};


#endif