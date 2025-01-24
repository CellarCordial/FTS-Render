#ifndef SHADER_SKY_COMMON_SLANG
#define SHADER_SKY_COMMON_SLANG

#include "math.slang"

float2 get_sky_uv(float3 ray_dir)
{
    float phi = atan2(ray_dir.z, ray_dir.x);   // [-π, π]
    float theta = asin(ray_dir.y);         // [-π/2, π/2]
    float u = phi / (2.0f * PI);
    float v = 0.5f + 0.5f * sign(theta) * sqrt(abs(theta) / (PI / 2.0f));
    // 对归一化结果取平方根，能够调整输出值的曲线，使得小角度（接近 0）的变化更加平滑，而大角度（接近 π/2）的变化较快.

    return float2(u, v);
}









#endif