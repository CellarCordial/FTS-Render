#ifndef SHADER_POST_PROCESS_HELPER_SLANG
#define SHADER_POST_PROCESS_HELPER_SLANG

namespace post_process
{
    float calculate_luminance(float3 rgb)
    {
        return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
    }
}
#endif