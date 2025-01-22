// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1

#include "../common/bxdf.slang"
#include "../common/math_helper.slang"

RWTexture2D<float3> brdf_lut_texture : register(u0);

float3 integrate_brdf(float n_dot_v, float roughness);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint32_t width, height;
    brdf_lut_texture.GetDimensions(width, height);

    float n_dot_v = ((thread_id.x + 0.5f) / width) * (1.0 - 1e-3) + 1e-3;
    float roughness = max((thread_id.y + 0.5f) / height, 1e-5);

    brdf_lut_texture[thread_id.xy] = integrate_brdf(n_dot_v, roughness);
}

float3 integrate_brdf(float n_dot_v, float roughness)
{
    float3 view_direction = float3(
        sqrt(1.0f - n_dot_v * n_dot_v),
        0.0f,
        n_dot_v
    );

    float A = 0.0f;
    float B = 0.0f;
    float valid_count = 0;

    SpecularBrdf specular_a = SpecularBrdf(roughness, float3(1.0f, 1.0f, 1.0f));
    SpecularBrdf specular_b = SpecularBrdf(roughness, float3(0.0f, 0.0f, 0.0f));

    const uint32_t SAMPLE_COUNT = 1024u;
    for(uint32_t ix = 0; ix < SAMPLE_COUNT; ++ix)
    {
        float2 random = hammersley(ix, SAMPLE_COUNT);
        BrdfSample sample_a = specular_a.sample(view_direction, random);

        if (sample_a.is_valid())
        {
            BrdfValue sample_b = specular_b.evaluate(view_direction, sample_a.wi);
            A += sample_a.value_over_pdf.x - sample_b.value_over_pdf.x;
            B += sample_b.value_over_pdf.x;
            valid_count++;
        }
    }
    return float3(A, B, valid_count) / SAMPLE_COUNT;
}









#endif