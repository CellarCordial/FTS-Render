#ifndef SHADER_MATH_HELPER_HLSL
#define SHADER_MATH_HELPER_HLSL

#define PI 3.14159265358979323846
#define PI2 6.28318530717958647692
#define INV_PI 0.318309886183790671538 
#define MAX_FLOAT 3.402823466e+38f
#define INVALID_SIZE_32 0xffffffff
#define RANDOM_SEED 1.32471795724474602596


float3 get_world_pos_from_depth_ndc(float2 uv, float ndc_depth, float4x4 inv_view_proj)
{
    float2 screen_space_position = uv * 2.0f - 1.0f;
    float3 ndc_position = float3(screen_space_position, ndc_depth);
    float4 world_space_position = mul(float4(ndc_position, 1.0f), inv_view_proj);
    world_space_position = world_space_position / world_space_position.w;
    return world_space_position.xyz;
}

float3 spherical_fibonacci(uint ix, uint max)
{
    // Theta 是方向与 z 轴的夹角.
    // phi 是方向在 x-y 平面的投影与 x 轴的夹角.

    // 黄金比例: ((sqrt(5.0f) + 1.0f) * 0.5f) 再减去 1.0f.
    float phi = sqrt(5.0f) * 0.5f - 0.5;
    phi = 2.0f * PI * frac(ix * phi);

    float cos_theta = ((float(ix) + 0.5f) / float(max)) * 2.0f - 1.0f;
    float sin_theta = sqrt(1.0f - cos_theta * cos_theta);
    return float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
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

float radical_inverse_vdc(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// 低差异序列.
float2 hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), radical_inverse_vdc(i));
}

float max4(float x, float y, float z, float w)
{
    return max(max(x, y), max(z, w));
}

float min4(float x, float y, float z, float w)
{
    return min(min(x, y), min(z, w));
}

float max3(float x, float y, float z)
{
    return max(x, max(y, z));
}

float min3(float x, float y, float z)
{
    return min(x, min(y, z));
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
uint hash(uint x)
{
    x += (x << 10u);
    x ^= (x >> 6u);
    x += (x << 3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}

uint murmur_mix(uint hash)
{
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;
    return hash;
}

static uint morton_code(uint x)
{
    x &= 0x0000ffff;
    x = (x ^ (x << 8)) & 0x00ff00ff;
    x = (x ^ (x << 4)) & 0x0f0f0f0f;
    x = (x ^ (x << 2)) & 0x33333333;
    x = (x ^ (x << 1)) & 0x55555555;
    return x;
}

static uint morton_encode(uint x,uint y)
{
    uint Morton = morton_code(x) | (morton_code(y) << 1);
    return Morton;
}

static uint morton_encode(uint2 value)
{
    return morton_encode(value.x, value.y);
}

static uint reverse_morton_code(uint x)
{
    x &= 0x55555555;
    x = (x ^ (x >> 1)) & 0x33333333;
    x = (x ^ (x >> 2)) & 0x0f0f0f0f;
    x = (x ^ (x >> 4)) & 0x00ff00ff;
    x = (x ^ (x >> 8)) & 0x0000ffff;
    return x;
}

static uint2 morton_decode(uint Morton)
{
    uint2 ret;
    ret.x = reverse_morton_code(Morton);
    ret.y = reverse_morton_code(Morton >> 1);
    return ret;
}

#endif