#define THREAD_GROUP_NUM_X 1
#define THREAD_GROUP_NUM_Y 1

#include "../bxdf.slang"
#include "../math_helper.slang"

RWTexture2D<float2> brdf_lut_texture : register(u0);

float2 IntegrateBrdf(float n_dot_v, float roughness);

#if defined(THREAD_GROUP_NUM_X) && defined(THREAD_GROUP_NUM_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, THREAD_GROUP_NUM_Y, 1)]
void compute_shader(uint3 thread_id : SV_Dispatchthread_id)
{
    uint3 width_height_levels;
    brdf_lut_texture.GetDimensions(0, width_height_levels.x, width_height_levels.y, width_height_levels.z);

    float n_dot_v = ((thread_id.x + 0.5f) / width_height_levels.x) * (1.0 - 1e-3) + 1e-3;
    float roughness = max((thread_id.y + 0.5f) / width_height_levels.y, 1e-5);

    brdf_lut_texture[thread_id.xy] = IntegrateBrdf(n_dot_v, roughness);
}

float2 IntegrateBrdf(float n_dot_v, float roughness)
{
    float3 view_direction = float3(
        sqrt(1.0f - n_dot_v * n_dot_v),
        0.0f,
        n_dot_v
    );

    float red_channel = 0.0f;
    float green_channel = 0.0f;

    brdf::SpecularReflection specular_a = { float3(1.0f, 1.0f, 1.0f), roughness };
    brdf::SpecularReflection specular_b = { float3(0.0f, 0.0f, 0.0f), roughness };

    const uint SAMPLE_COUNT = 1024u;
    for(uint ix = 0; ix < SAMPLE_COUNT; ++ix)
    {
        float pdf;
        float3 weight_a;
        float3 light_direction;
        float2 random = hammersley(ix, SAMPLE_COUNT);

        if (specular_a.sample(view_direction, random, light_direction, pdf, weight_a))
        {
            float3 WeightB = specular_b.eval(view_direction, light_direction);
            red_channel += weight_a.x - WeightB.x;
            green_channel += WeightB.x;
        }
    }
    return float2(red_channel, green_channel) / SAMPLE_COUNT;
}









#endif