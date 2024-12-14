#ifndef SHADER_MATH_HELPER_SLANG
#define SHADER_MATH_HELPER_SLANG

#define PI 3.14159265
#define PI2 6.28318530717958647692528676655900577
#define MAX_FLOAT 3.402823466e+38f
#define RANDOM_SEED 1.32471795724474602596

float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// 低差异序列.
float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

__generic<typename T>
T max4(T x, T y, T z, T w) where T : __BuiltinFloatingPointType
{
    return max(max(x, y), max(z, w));
}

float3 decompress_unit_direction(uint dir)
{
    return float3(
        float(dir & ((1u << 11u) - 1u)) * (2.0f / float((1u << 11u) - 1u)) - 1.0f,
        float((dir >> 11u) & ((1u << 10u) - 1u)) * (2.0f / float((1u << 10u) - 1u)) - 1.0f,
        float((dir >> 21u)) * (2.0f / float((1u << 11u) - 1u)) - 1.0f
    );
}

float2 generate_random2(uint ix)
{
    const float r1 = 1.0 / RANDOM_SEED;
    const float r2 = 1.0 / (RANDOM_SEED * RANDOM_SEED);

    return frac(float2(r1, r2) * ix + 0.5);
}

float4 sample_from_blue_noise(uint2 pixel_id, uint random, Texture2D<float4> blue_noise_texture)
{
    uint blue_noise_texture_width;
    uint blue_noise_texture_height;
    uint blue_noise_texture_mip_levels;
    blue_noise_texture.GetDimensions(0, blue_noise_texture_width, blue_noise_texture_height, blue_noise_texture_mip_levels);

    uint2 resolution = uint2(blue_noise_texture_width, blue_noise_texture_height);
    uint2 offset = uint2(resolution * generate_random2(random));

    // [0.5/256, 255.5/256]
    return blue_noise_texture[(pixel_id + offset) % resolution] * 255.0f / 256.0f + 0.5f / 256.0f;
}

float3 sample_direction_from_cone(float2 random, float cos_theta_max)
{
    float cos_theta = (1.0 - random.x) + random.x * cos_theta_max;
    float sin_theta = sqrt(saturate(1.0 - cos_theta * cos_theta));
    float phi = random.y * PI2;
    return float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}

float3x3 create_orthonormal_basis(float3 n) 
{
    float3 b1;
    float3 b2;

    if (n.z < 0.0) 
    {
        const float a = 1.0 / (1.0 - n.z);
        const float b = n.x * n.y * a;
        b1 = float3(1.0 - n.x * n.x * a, -b, n.x);
        b2 = float3(b, n.y * n.y * a - 1.0, -n.y);
    } 
    else 
    {
        const float a = 1.0 / (1.0 + n.z);
        const float b = -n.x * n.y * a;
        b1 = float3(1.0 - n.x * n.x * a, b, -n.x);
        b2 = float3(b, 1.0 - n.y * n.y * a, -n.y);
    }

    return float3x3(
        b1.x, b2.x, n.x,
        b1.y, b2.y, n.y,
        b1.z, b2.z, n.z
    );
}

// Jenkins hash.
uint hash(uint x)
{
    x += (x << 10u);
    x ^= (x >> 6u);
    x += (x << 3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}

#endif