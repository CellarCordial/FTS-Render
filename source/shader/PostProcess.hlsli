#ifndef SHADER_POST_PROCESS_HLSLI
#define SHADER_POST_PROCESS_HLSLI

#define POSTCOLOR_A 2.51f
#define POSTCOLOR_B 0.03f
#define POSTCOLOR_C 2.43f
#define POSTCOLOR_D 0.59f
#define POSTCOLOR_E 0.14f


float3 TomeMap(float3 Color)
{
    return (Color * (POSTCOLOR_A * Color + POSTCOLOR_B)) / (Color * (POSTCOLOR_C * Color + POSTCOLOR_D) + POSTCOLOR_E);
}

float3 PostProcess(float2 Seed, float3 Color)
{
    Color = TomeMap(Color);
    
    // 抖动.
    float fRand = frac(sin(dot(Seed, float2(12.9898f, 78.233f) * 2.0f)) * 43758.5453f);
    Color = 255.0f * saturate(pow(Color, 1.0f / 2.2f));
    Color = select(fRand.xxx < (Color - floor(Color)), ceil(Color), floor(Color));

    return Color / 255.0f;
}











#endif