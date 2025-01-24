#ifndef SCENE_LGIHT_H
#define SCENE_LGIHT_H
#include "../core/math/matrix.h"
#include "../core/tools/delegate.h"

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

    namespace event 
    {
        DELCARE_DELEGATE_EVENT(AddPointLight);
        DELCARE_DELEGATE_EVENT(AddSpotLight);
    }

    struct DirectionalLight
    {
        float4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float intensity = 5.0f;
        float2 angle = { 0.0f, 11.6f };
        float sun_angular_radius = 0.9999f;

        float3 direction;
        float4x4 view_proj;

        void update_direction_view_proj()
        {
            float x = radians(angle.x);
            float y = radians(-angle.y);

            direction = normalize(float3(
                std::cos(x) * std::cos(y),
                std::sin(y),
                std::sin(x) * std::cos(y)
            ));

            view_proj = mul(
                look_at_left_hand(-direction * 20.0f, float3{}, float3(0.0f, 1.0f, 0.0f)),
                orthographic_left_hand(20.0f, 20.0f, 0.1f, 80.0f)
            );
        }
    };
}




















#endif