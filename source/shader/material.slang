#ifndef SHADER_MATERIAL_SLANG
#define SHADER_MATERIAL_SLANG

struct Material
{
    float4 fDiffuse;
    float  fRoughness;
    float  fMetallic;
    float  fOcclusion;
    float4  fEmissive;
};


#endif