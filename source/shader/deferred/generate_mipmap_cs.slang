#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

Texture2D input_texture : register(t0);
RWTexture2D output_texture : register(u0);
SamplerState linear_clamp_sampler : register(s0);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void compute_shader(uint3 thread_id: SV_DispatchThreadID)
{
    uint32_t width, height;
    input_texture.GetDimensions(width, height);

    float2 uv = float2(
        (thread_id.x + 0.5f) * width,
        (thread_id.y + 0.5f) * height
    );

    output_texture[thread_id.xy] = input_texture.SampleLevel(linear_clamp_sampler, uv, 0.0f);
}

#endif