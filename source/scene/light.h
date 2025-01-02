#ifndef SCENE_LGIHT_H
#define SCENE_LGIHT_H
#include "../core/math/matrix.h"

namespace fantasy 
{
    struct PointLight
    {
        float4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float3 position = { 0.0f, 2.0f, 0.0f };
        float intensity = 0.5f;
        float fall_off_start = 1.0f;
        float fall_off_end = 10.0f;
    };
        
    struct DirectionalLight
    {
        float4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float intensity = 5.0f;
        float3 direction;
        float2 angle = { 0.0f, 11.6f };
        float sun_angular_radius = 0.9999f;

        float4x4 view_proj;
    };
}




















#endif