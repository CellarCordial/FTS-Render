#ifndef SHADER_COMMON_SURFACD_CACHE_H
#define SHADER_COMMON_SURFACD_CACHE_H

struct CardData
{
    float4x4 local_matrix;
    uint2 atlas_offset;
    float3 extent;
};

struct MeshSurfaceData
{
    float4 bounding_sphere;
    CardData cards[6];
};

#endif