#ifndef SHADER_POST_PROCESS_HLSL
#define SHADER_POST_PROCESS_HLSL

#define POSTCOLOR_A 2.51f
#define POSTCOLOR_B 0.03f
#define POSTCOLOR_C 2.43f
#define POSTCOLOR_D 0.59f
#define POSTCOLOR_E 0.14f


float3 tone_map(float3 color)
{
    return (color * (POSTCOLOR_A * color + POSTCOLOR_B)) / (color * (POSTCOLOR_C * color + POSTCOLOR_D) + POSTCOLOR_E);
}

float3 simple_post_process(float2 seed, float3 color)
{
    color = tone_map(color);
    
    // 抖动.
    float rand = frac(sin(dot(seed, float2(12.9898f, 78.233f) * 2.0f)) * 43758.5453f);
    color = 255.0f * saturate(pow(color, 1.0f / 2.2f));
    color = select(any(rand.xxx < (color - floor(color))), ceil(color), floor(color));

    return color / 255.0f;
}

float calculate_luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}









#endif