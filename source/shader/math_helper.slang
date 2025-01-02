#ifndef SHADER_MATH_HELPER_SLANG
#define SHADER_MATH_HELPER_SLANG

#define PI 3.14159265
#define PI2 6.28318530717958647692528676655900577
#define MAX_FLOAT 3.402823466e+38f
#define RANDOM_SEED 1.32471795724474602596

float3 get_world_pos_from_depth_ndc(float2 uv, float fDepthNDC, float4x4 InvViewProj)
{
    float2 screen_space_position = uv * 2.0f - 1.0f;
    float3 PositionNDC = float3(screen_space_position, fDepthNDC);
    float4 WorldPos = mul(float4(PositionNDC, 1.0f), InvViewProj);
    WorldPos = WorldPos / WorldPos.w;
    return WorldPos.xyz;
}

float3 spherical_fibonacci(uint32_t ix, uint32_t dwTotalNum)
{
    // Theta 是方向与 z 轴的夹角.
    // Phi 是方向在 x-y 平面的投影与 x 轴的夹角.

    // 黄金比例: ((sqrt(5.0f) + 1.0f) * 0.5f) 再减去 1.0f.
    float Phi = sqrt(5.0f) * 0.5f - 0.5;
    Phi = 2.0f * PI * frac(ix * Phi);

    float fThetaCos = ((float(ix) + 0.5f) / float(dwTotalNum)) * 2.0f - 1.0f;
    float fThetaSin = sqrt(1.0f - fThetaCos * fThetaCos);
    return float3(fThetaSin * cos(Phi), fThetaSin * sin(Phi), fThetaCos);
}

float3 calcute_normal(float3 TextureNormal, float3 VertexNormal, float4 VertexTangent)
{
    float3 UnpackedNormal = TextureNormal * 2.0f - 1.0f;
    float3 N = VertexNormal;
    float3 T = normalize(VertexTangent.xyz - N * dot(VertexTangent.xyz, N));
    float3 B = cross(N, T) * VertexTangent.w;
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(UnpackedNormal, TBN));
}

float radical_inverse_vdc(uint32_t bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// 低差异序列.
float2 hammersley(uint32_t i, uint32_t N)
{
    return float2(float(i) / float(N), radical_inverse_vdc(i));
}

__generic<typename T>
T max4(T x, T y, T z, T w) where T : __BuiltinFloatingPointType
{
    return max(max(x, y), max(z, w));
}

float3 decompress_unit_direction(uint32_t dir)
{
    return float3(
        float(dir & ((1u << 11u) - 1u)) * (2.0f / float((1u << 11u) - 1u)) - 1.0f,
        float((dir >> 11u) & ((1u << 10u) - 1u)) * (2.0f / float((1u << 10u) - 1u)) - 1.0f,
        float((dir >> 21u)) * (2.0f / float((1u << 11u) - 1u)) - 1.0f
    );
}

float2 generate_random2(uint32_t ix)
{
    const float r1 = 1.0 / RANDOM_SEED;
    const float r2 = 1.0 / (RANDOM_SEED * RANDOM_SEED);

    return frac(float2(r1, r2) * ix + 0.5);
}

float4 sample_from_blue_noise(uint2 pixel_id, uint32_t random, Texture2D<float4> blue_noise_texture)
{
    uint32_t blue_noise_texture_width;
    uint32_t blue_noise_texture_height;
    blue_noise_texture.GetDimensions(blue_noise_texture_width, blue_noise_texture_height);

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
uint32_t hash(uint32_t x)
{
    x += (x << 10u);
    x ^= (x >> 6u);
    x += (x << 3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}

#endif