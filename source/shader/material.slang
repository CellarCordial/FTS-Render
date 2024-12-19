#ifndef SHADER_MATERIAL_SLANG
#define SHADER_MATERIAL_SLANG

struct Material
{
    float4 diffuse;
    float  roughness;
    float  metallic;
    float  occlusion;
    float4  emissive;
};


#endif