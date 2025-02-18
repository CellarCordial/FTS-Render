#ifndef SHADER_COMMON_LIGHT_H
#define SHADER_COMMON_LIGHT_H

struct PointLight
{
    float4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float3 position = { 0.0f, 2.0f, 0.0f };
    float intensity = 0.5f;
    float fall_off_start = 1.0f;
    float fall_off_end = 10.0f;
};

struct SpotLight
{
    float4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float3 position = { 0.0f, 2.0f, 0.0f };
    float3 direction;
    float intensity = 0.5f;
    float inner_angle = 0.0f;
    float outer_angle = 0.0f;
    float attenuation = 0.0f;
    float max_distance = 0.0f;
};

#endif