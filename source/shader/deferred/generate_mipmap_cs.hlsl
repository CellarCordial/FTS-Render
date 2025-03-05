// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1

cbuffer pass_constants : register(b0)
{
    uint output_resolution;
    uint input_mip_level;
};

Texture2D input_texture : register(t0);
RWTexture2D<float4> output_texture : register(u0);
SamplerState linear_clamp_sampler : register(s0);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    if (thread_id.x >= output_resolution || thread_id.y >= output_resolution) return;

    float2 uv = float2(thread_id.x + 0.5f, thread_id.y + 0.5f) / output_resolution;

    output_texture[thread_id.xy] = input_texture.SampleLevel(linear_clamp_sampler, uv, input_mip_level);
}

#endif