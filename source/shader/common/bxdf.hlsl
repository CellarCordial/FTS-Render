#ifndef SHADER_COMMON_BXDF_H
#define SHADER_COMMON_BXDF_H

#include "math.hlsl"

// Enable support for delta reflection/transmission.
#define EnableDeltaBSDF         1

// Enable GGX sampling using the distribution of visible normals (VNDF) instead of classic NDF sampling.
// This should be the default as it has lower variance, disable for testing only.
// TODO: Make default when transmission with VNDF sampling is properly validated
#define EnableVNDFSampling      1

// Enable explicitly computing sampling weights using eval(wo, wi) / evalPdf(wo, wi).
// This is for testing only, as many terms of the equation cancel out allowing to save on computation.
#define ExplicitSampleWeights   0

#define SpecularMaskingFunctionSmithGGXSeparable 0
#define SpecularMaskingFunctionSmithGGXCorrelated 1
#define SpecularMaskingFunction SpecularMaskingFunctionSmithGGXCorrelated

#define DiffuseBrdfLambert 0
#define DiffuseBrdfDisney 1
#define DiffuseBrdfFrostbite 2
#define DiffuseBrdf DiffuseBrdfLambert

// We clamp the GGX width parameter to avoid numerical instability.
// In some computations, we can avoid clamps etc. if 1.0 - alpha^2 != 1.0, so the epsilon should be 1.72666361e-4 or larger in fp32.
// The the value below is sufficient to avoid visible artifacts.
// Falcor used to clamp roughness to 0.08 before the clamp was removed for allowing delta events. We continue to use the same threshold.
static const float kMinGGXAlpha = 0.0064f;

// Minimum cos(theta) for the view and light vectors.
// A few functions are not robust for cos(theta) == 0.0.
// TODO: Derive appropriate bounds
static const float kMinCosTheta = 1e-6f;

/** Returns a relative luminance of an input linear RGB color in the ITU-R BT.709 color space
    \param RGBColor linear HDR RGB color in the ITU-R BT.709 color space
*/
inline float luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

/** 32-bit bit interleave (Morton code).
    \param[in] v 16-bit values in the LSBs of each component (higher bits don't matter).
    \return 32-bit value.
*/
uint interleave_32bit(uint2 v)
{
    uint x = v.x & 0x0000ffff; // x = ---- ---- ---- ---- fedc ba98 7654 3210
    uint y = v.y & 0x0000ffff;

    x = (x | (x << 8)) & 0x00FF00FF; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    x = (x | (x << 4)) & 0x0F0F0F0F; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    x = (x | (x << 2)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
    x = (x | (x << 1)) & 0x55555555; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0

    y = (y | (y << 8)) & 0x00FF00FF;
    y = (y | (y << 4)) & 0x0F0F0F0F;
    y = (y | (y << 2)) & 0x33333333;
    y = (y | (y << 1)) & 0x55555555;

    return x | (y << 1);
}

/** Implementation of the xoshiro128** 32-bit all-purpose, rock-solid generator
    written in 2018 by David Blackman and Sebastiano Vigna (vigna@acm.org).
    The state is 128 bits and the period (2^128)-1. It has a jump function that
    allows you to skip ahead 2^64 in the seqeuence.

    Note: The state must be seeded so that it is not everywhere zero.
    The recommendation is to initialize the state using SplitMix64.

    See the original public domain code: http://xoshiro.di.unimi.it/xoshiro128starstar.c
*/

struct Xoshiro128StarStar
{
    uint state[4];
};

uint rotl(const uint x, int k)
{
    return (x << k) | (x >> (32 - k));
}

/** Generates the next pseudorandom number in the sequence (32 bits).
 */
uint nextRandom(inout Xoshiro128StarStar rng)
{
    const uint result_starstar = rotl(rng.state[0] * 5, 7) * 9;
    const uint t = rng.state[1] << 9;

    rng.state[2] ^= rng.state[0];
    rng.state[3] ^= rng.state[1];
    rng.state[1] ^= rng.state[2];
    rng.state[0] ^= rng.state[3];

    rng.state[2] ^= t;
    rng.state[3] = rotl(rng.state[3], 11);

    return result_starstar;
}

/** SplitMix64 pseudorandom number generator.

    This is a fixed-increment version of Java 8's SplittableRandom generator.
    The period is 2^64 and its state size is 64 bits.
    It is a very fast generator passing BigCrush. It is recommended for use with
    other generators like xoroshiro and xorshift to initialize their state arrays.

    Steele Jr, Guy L., Doug Lea, and Christine H. Flood., "Fast Splittable Pseudorandom Number Generators",
    ACM SIGPLAN Notices 49.10 (2014): 453-472. http://dx.doi.org/10.1145/2714064.2660195.

    This code requires shader model 6.0 or above for 64-bit integer support.
*/

struct SplitMix64
{
    uint64_t state;
};

uint64_t asuint64(uint lowbits, uint highbits)
{
    return (uint64_t(highbits) << 32) | uint64_t(lowbits);
}

/** Generates the next pseudorandom number in the sequence (64 bits).
 */
uint64_t nextRandom64(inout SplitMix64 rng)
{
    uint64_t z = (rng.state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

/** Generates the next pseudorandom number in the sequence (low 32 bits).
 */
uint nextRandom(inout SplitMix64 rng)
{
    return (uint)nextRandom64(rng);
}

/** Initialize SplitMix64 pseudorandom number generator.
    \param[in] s0 Low bits of initial state (seed).
    \param[in] s1 High bits of initial state (seed).
*/
SplitMix64 createSplitMix64(uint s0, uint s1)
{
    SplitMix64 rng;
    rng.state = asuint64(s0, s1);
    return rng;
}

/** Initialize Xoshiro128StarStar pseudorandom number generator.
    The initial state should be pseudorandom and must not be zero everywhere.
    It is recommended to use SplitMix64 for creating the initial state.
    \param[in] s Array of 4x 32-bit values of initial state (seed).
*/
Xoshiro128StarStar createXoshiro128StarStar(uint s[4])
{
    Xoshiro128StarStar rng;
    rng.state[0] = s[0];
    rng.state[1] = s[1];
    rng.state[2] = s[2];
    rng.state[3] = s[3];
    return rng;
}

/** Default uniform pseudorandom number generator.

    This generator has 128 bit state and should have acceptable statistical
    properties for most rendering applications.

    This sample generator requires shader model 6.0 or above.
*/
struct SampleGenerator
{
    struct Padded
    {
        SampleGenerator internal;
    }

    /** Create sample generator.
     */
    static SampleGenerator create(uint2 pixel, uint sampleNumber)
    {
        SampleGenerator sampleGenerator;

        // Use SplitMix64 generator to generate a good pseudorandom initial state.
        // The pixel coord is expected to be max 28 bits (16K^2 is the resource limit in D3D12).
        // The sample number is expected to be practically max ~28 bits, e.g. 16spp x 16M samples.
        // As long as both stay <= 32 bits, we will always have a unique initial seed.
        // This is however no guarantee that the generated sequences will never overlap,
        // but it is very unlikely. For example, with these most extreme parameters of
        // 2^56 sequences of length L, the probability of overlap is P(overlap) = L*2^-16.
        SplitMix64 rng = createSplitMix64(interleave_32bit(pixel), sampleNumber);
        uint64_t s0 = nextRandom64(rng);
        uint64_t s1 = nextRandom64(rng);
        uint seed[4] = { uint(s0), uint(s0 >> 32), uint(s1), uint(s1 >> 32) };

        // Create xoshiro128** pseudorandom generator.
        sampleGenerator.rng = createXoshiro128StarStar(seed);
        return sampleGenerator;
    }

    /** Returns the next sample value. This function updates the state.
     */
    [mutating] uint next()
    {
        return nextRandom(rng);
    }

    Xoshiro128StarStar rng;
};

/** Convenience functions for generating 1D/2D/3D values in the range [0,1).

    Note: These are global instead of member functions in the sample generator
    interface, as there seems to be no way in Slang currently to specify default
    implementations without duplicating the code into all classes that implement
    the interace.
*/
float sampleNext1D(inout SampleGenerator sg)
{
    // Use upper 24 bits and divide by 2^24 to get a number u in [0,1).
    // In floating-point precision this also ensures that 1.0-u != 0.0.
    uint bits = sg.next();
    return (bits >> 8) * 0x1p-24;
}

float2 sampleNext2D(inout SampleGenerator sg)
{
    float2 sample;
    // Don't use the float2 initializer to ensure consistent order of evaluation.
    sample.x = sampleNext1D(sg);
    sample.y = sampleNext1D(sg);
    return sample;
}

float3 sampleNext3D(inout SampleGenerator sg)
{
    float3 sample;
    // Don't use the float3 initializer to ensure consistent order of evaluation.
    sample.x = sampleNext1D(sg);
    sample.y = sampleNext1D(sg);
    sample.z = sampleNext1D(sg);
    return sample;
}

/** Flags representing the various lobes of a BxDF.
 */
enum class LobeType
{
    DiffuseReflection = 0x01,
    SpecularReflection = 0x02,
    DeltaReflection = 0x04,

    DiffuseTransmission = 0x10,
    SpecularTransmission = 0x20,
    DeltaTransmission = 0x40,

    Diffuse = 0x11,
    Specular = 0x22,
    Delta = 0x44,
    NonDelta = 0x33,

    Reflection = 0x0f,
    Transmission = 0xf0,

    NonDeltaReflection = 0x03,

    All = 0xff,
};

/** This struct holds all the data for shading a specific hit point. This consists of:
    - Geometric data
    - Preprocessed material properties (fetched from constants/textures)
    - BSDF lobes to be evaluated/sampled
*/
struct ShadingData
{
    float3 posW; ///< Shading hit position in world space
    float3 V;    ///< Direction to the eye at shading hit
    float3 N;    ///< Shading normal at shading hit
    float3 T;    ///< Shading tangent at shading hit
    float3 B;    ///< Shading bitangent at shading hit
    float2 uv;   ///< Texture mapping coordinates
    float NdotV; // Unclamped, can be negative.

    // Primitive data
    float3 faceN;     ///< Face normal in world space, always on the front-facing side.
    bool frontFacing; ///< True if primitive seen from the front-facing side.
    bool doubleSided; ///< Material double-sided flag, if false only shade front face.

    // Pre-loaded texture data
    uint materialID; ///< Material ID at shading location.
    float3 diffuse;  ///< Diffuse albedo.
    float opacity;
    float3 specular;       ///< Specular albedo.
    float linearRoughness; ///< This is the original roughness, before re-mapping.
    float ggxAlpha;        ///< DEPRECATED: This is the re-mapped roughness value, which should be used for GGX computations. It equals `roughness^2`
    float3 emissive;
    float4 occlusion;
    float IoR;                  ///< Index of refraction of the medium "behind" the surface.
    float metallic;             ///< Metallic parameter, blends between dielectric and conducting BSDFs.
    float specularTransmission; ///< Specular transmission, blends between opaque dielectric BRDF and specular transmissive BSDF.
    float eta;                  ///< Relative index of refraction (incident IoR / transmissive IoR).

    // Sampling/evaluation data
    uint activeLobes; ///< BSDF lobes to include for sampling and evaluation. See LobeType in BxDFTypes.slang.

    [mutating] void setActiveLobes(uint lobes) { activeLobes = lobes; }
};

/** Uniform sampling of the unit disk using Shirley's concentric mapping.
    \param[in] u Uniform random numbers in [0,1)^2.
    \return Sampled point on the unit disk.
*/
float2 sample_disk_concentric(float2 u)
{
    u = 2.f * u - 1.f;
    if (u.x == 0.f && u.y == 0.f) return u;
    float phi, r;
    if (abs(u.x) > abs(u.y))
    {
        r = u.x;
        phi = (u.y / u.x) * PI / 4;
    }
    else
    {
        r = u.y;
        phi = PI / 2 - (u.x / u.y) * PI / 4;
    }
    return r * float2(cos(phi), sin(phi));
}

/** Cosine-weighted sampling of the hemisphere using Shirley's concentric mapping.
    \param[in] u Uniform random numbers in [0,1)^2.
    \param[out] pdf Probability density of the sampled direction (= cos(theta)/pi).
    \return Sampled direction in the local frame (+z axis up).
*/
float3 sample_cosine_hemisphere_concentric(float2 u, out float pdf)
{
    float2 d = sample_disk_concentric(u);
    float z = sqrt(max(0.f, 1.f - dot(d, d)));
    pdf = z * INV_PI;
    return float3(d, z);
}


/** Evaluates the GGX (Trowbridge-Reitz) normal distribution function (D).

    Introduced by Trowbridge and Reitz, "Average irregularity representation of a rough surface for ray reflection", Journal of the Optical Society of America, vol. 65(5), 1975.
    See the correct normalization factor in Walter et al. https://dl.acm.org/citation.cfm?id=2383874
    We use the simpler, but equivalent expression in Eqn 19 from http://blog.selfshadow.com/publications/s2012-shading-course/hoffman/s2012_pbs_physics_math_notes.pdf

    For microfacet models, D is evaluated for the direction h to find the density of potentially active microfacets (those for which microfacet normal m = h).
    The 'alpha' parameter is the standard GGX width, e.g., it is the square of the linear roughness parameter in Disney's BRDF.
    Note there is a singularity (0/0 = NaN) at NdotH = 1 and alpha = 0, so alpha should be clamped to some epsilon.

    \param[in] alpha GGX width parameter (should be clamped to small epsilon beforehand).
    \param[in] cosTheta Dot product between shading normal and half vector, in positive hemisphere.
    \return D(h)
*/
float evalNdfGGX(float alpha, float cosTheta)
{
    float a2 = alpha * alpha;
    float d = ((cosTheta * a2 - cosTheta) * cosTheta + 1);
    return a2 / (d * d * PI);
}

/** Evaluates the PDF for sampling the GGX normal distribution function using Walter et al. 2007's method.
    See https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf

    \param[in] alpha GGX width parameter (should be clamped to small epsilon beforehand).
    \param[in] cosTheta Dot product between shading normal and half vector, in positive hemisphere.
    \return D(h) * cosTheta
*/
float evalPdfGGX_NDF(float alpha, float cosTheta)
{
    return evalNdfGGX(alpha, cosTheta) * cosTheta;
}

/** Samples the GGX (Trowbridge-Reitz) normal distribution function (D) using Walter et al. 2007's method.
    Note that the sampled half vector may lie in the negative hemisphere. Such samples should be discarded.
    See Eqn 35 & 36 in https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
    See Listing A.1 in https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf

    \param[in] alpha GGX width parameter (should be clamped to small epsilon beforehand).
    \param[in] u Uniform random number (2D).
    \param[out] pdf Sampling probability.
    \return Sampled half vector in local space.
*/
float3 sampleGGX_NDF(float alpha, float2 u, out float pdf)
{
    float alphaSqr = alpha * alpha;
    float phi = u.y * (2 * PI);
    float tanThetaSqr = alphaSqr * u.x / (1 - u.x);
    float cosTheta = 1 / sqrt(1 + tanThetaSqr);
    float r = sqrt(max(1 - cosTheta * cosTheta, 0));

    pdf = evalPdfGGX_NDF(alpha, cosTheta);
    return float3(cos(phi) * r, sin(phi) * r, cosTheta);
}

/** Evaluates the PDF for sampling the GGX distribution of visible normals (VNDF).
    See http://jcgt.org/published/0007/04/01/paper.pdf

    \param[in] alpha GGX width parameter (should be clamped to small epsilon beforehand).
    \param[in] wo View direction in local space, in the positive hemisphere.
    \param[in] h Half vector in local space, in the positive hemisphere.
    \return D_V(h) = G1(wo) * D(h) * max(0,dot(wo,h)) / wo.z
*/
float evalPdfGGX_VNDF(float alpha, float3 wo, float3 h)
{
    float G1 = evalG1GGX(alpha * alpha, wo.z);
    float D = evalNdfGGX(alpha, h.z);
    return G1 * D * max(0.f, dot(wo, h)) / wo.z;
}

/** Samples the GGX (Trowbridge-Reitz) using the distribution of visible normals (VNDF).
    The GGX VDNF yields significant variance reduction compared to sampling of the GGX NDF.
    See http://jcgt.org/published/0007/04/01/paper.pdf

    \param[in] alpha Isotropic GGX width parameter (should be clamped to small epsilon beforehand).
    \param[in] wo View direction in local space, in the positive hemisphere.
    \param[in] u Uniform random number (2D).
    \param[out] pdf Sampling probability.
    \return Sampled half vector in local space, in the positive hemisphere.
*/
float3 sampleGGX_VNDF(float alpha, float3 wo, float2 u, out float pdf)
{
    float alpha_x = alpha, alpha_y = alpha;

    // Transform the view vector to the hemisphere configuration.
    float3 Vh = normalize(float3(alpha_x * wo.x, alpha_y * wo.y, wo.z));

    // Construct orthonormal basis (Vh,T1,T2).
    float3 T1 = (Vh.z < 0.9999f) ? normalize(cross(float3(0, 0, 1), Vh)) : float3(1, 0, 0); // TODO: fp32 precision
    float3 T2 = cross(Vh, T1);

    // Parameterization of the projected area of the hemisphere.
    float r = sqrt(u.x);
    float phi = (2.f * PI) * u.y;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5f * (1.f + Vh.z);
    t2 = (1.f - s) * sqrt(1.f - t1 * t1) + s * t2;

    // Reproject onto hemisphere.
    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.f, 1.f - t1 * t1 - t2 * t2)) * Vh;

    // Transform the normal back to the ellipsoid configuration. This is our half vector.
    float3 h = normalize(float3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.f, Nh.z)));

    pdf = evalPdfGGX_VNDF(alpha, wo, h);
    return h;
}

/** Evaluates the Smith masking function (G1) for the GGX normal distribution.
    See Eq 34 in https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf

    The evaluated direction is assumed to be in the positive hemisphere relative the half vector.
    This is the case when both incident and outgoing direction are in the same hemisphere, but care should be taken with transmission.

    \param[in] alphaSqr Squared GGX width parameter.
    \param[in] cosTheta Dot product between shading normal and evaluated direction, in the positive hemisphere.
*/
float evalG1GGX(float alphaSqr, float cosTheta)
{
    if (cosTheta <= 0) return 0;
    float cosThetaSqr = cosTheta * cosTheta;
    float tanThetaSqr = max(1 - cosThetaSqr, 0) / cosThetaSqr;
    return 2 / (1 + sqrt(1 + alphaSqr * tanThetaSqr));
}

/** Evaluates the Smith lambda function for the GGX normal distribution.
    See Eq 72 in http://jcgt.org/published/0003/02/03/paper.pdf

    \param[in] alphaSqr Squared GGX width parameter.
    \param[in] cosTheta Dot product between shading normal and the evaluated direction, in the positive hemisphere.
*/
float evalLambdaGGX(float alphaSqr, float cosTheta)
{
    if (cosTheta <= 0) return 0;
    float cosThetaSqr = cosTheta * cosTheta;
    float tanThetaSqr = max(1 - cosThetaSqr, 0) / cosThetaSqr;
    return 0.5 * (-1 + sqrt(1 + alphaSqr * tanThetaSqr));
}

/** Evaluates the separable form of the masking-shadowing function for the GGX normal distribution, using Smith's approximation.
    See Eq 98 in http://jcgt.org/published/0003/02/03/paper.pdf

    \param[in] alpha GGX width parameter (should be clamped to small epsilon beforehand).
    \param[in] cosThetaI Dot product between shading normal and incident direction, in positive hemisphere.
    \param[in] cosThetaO Dot product between shading normal and outgoing direction, in positive hemisphere.
    \return G(cosThetaI, cosThetaO)
*/
float evalMaskingSmithGGXSeparable(float alpha, float cosThetaI, float cosThetaO)
{
    float alphaSqr = alpha * alpha;
    float lambdaI = evalLambdaGGX(alphaSqr, cosThetaI);
    float lambdaO = evalLambdaGGX(alphaSqr, cosThetaO);
    return 1 / ((1 + lambdaI) * (1 + lambdaO));
}

/** Evaluates the height-correlated form of the masking-shadowing function for the GGX normal distribution, using Smith's approximation.
    See Eq 99 in http://jcgt.org/published/0003/02/03/paper.pdf

    Eric Heitz recommends using it in favor of the separable form as it is more accurate and of similar complexity.
    The function is only valid for cosThetaI > 0 and cosThetaO > 0  and should be clamped to 0 otherwise.

    \param[in] alpha GGX width parameter (should be clamped to small epsilon beforehand).
    \param[in] cosThetaI Dot product between shading normal and incident direction, in positive hemisphere.
    \param[in] cosThetaO Dot product between shading normal and outgoing direction, in positive hemisphere.
    \return G(cosThetaI, cosThetaO)
*/
float evalMaskingSmithGGXCorrelated(float alpha, float cosThetaI, float cosThetaO)
{
    float alphaSqr = alpha * alpha;
    float lambdaI = evalLambdaGGX(alphaSqr, cosThetaI);
    float lambdaO = evalLambdaGGX(alphaSqr, cosThetaO);
    return 1 / (1 + lambdaI + lambdaO);
}

/** Evaluates the Fresnel term using Schlick's approximation.
    Introduced in http://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf

    The Fresnel term equals f0 at normal incidence, and approaches f90=1.0 at 90 degrees.
    The formulation below is generalized to allow both f0 and f90 to be specified.

    \param[in] f0 Specular reflectance at normal incidence (0 degrees).
    \param[in] f90 Reflectance at orthogonal incidence (90 degrees), which should be 1.0 for specular surface reflection.
    \param[in] cosTheta Cosine of angle between microfacet normal and incident direction (LdotH).
    \return Fresnel term.
*/
float3 evalFresnelSchlick(float3 f0, float3 f90, float cosTheta)
{
    return f0 + (f90 - f0) * pow(max(1 - cosTheta, 0), 5); // Clamp to avoid NaN if cosTheta = 1+epsilon
}

/** Evaluates the Fresnel term using dieletric fresnel equations.
    Based on http://www.pbr-book.org/3ed-2018/Reflection_Models/Specular_Reflection_and_Transmission.html

    \param[in] eta Relative index of refraction (etaI / etaT).
    \param[in] cosThetaI Cosine of angle between normal and incident direction.
    \param[out] cosThetaT Cosine of angle between negative normal and transmitted direction (0 for total internal reflection).
    \return Returns Fr(eta, cosThetaI).
*/
float evalFresnelDielectric(float eta, float cosThetaI, out float cosThetaT)
{
    if (cosThetaI < 0)
    {
        eta = 1 / eta;
        cosThetaI = -cosThetaI;
    }

    float sinThetaTSq = eta * eta * (1 - cosThetaI * cosThetaI);
    // Check for total internal reflection
    if (sinThetaTSq > 1)
    {
        cosThetaT = 0;
        return 1;
    }

    cosThetaT = sqrt(1 - sinThetaTSq); // No clamp needed

    // Note that at eta=1 and cosThetaI=0, we get cosThetaT=0 and NaN below.
    // It's important the framework clamps |cosThetaI| or eta to small epsilon.
    float Rs = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
    float Rp = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);

    return 0.5 * (Rs * Rs + Rp * Rp);
}

/** Evaluates the Fresnel term using dieletric fresnel equations.
    Based on http://www.pbr-book.org/3ed-2018/Reflection_Models/Specular_Reflection_and_Transmission.html

    \param[in] eta Relative index of refraction (etaI / etaT).
    \param[in] cosThetaI Cosine of angle between normal and incident direction.
    \return Returns Fr(eta, cosThetaI).
*/
float evalFresnelDielectric(float eta, float cosThetaI)
{
    float cosThetaT;
    return evalFresnelDielectric(eta, cosThetaI, cosThetaT);
}

/** Interface for BxDFs.
    Conventions:
    - wo is the outgoing or scattering direction and points away from the shading location.
    - wi is the incident or light direction and points away from the shading location.
    - the local shading frame has normal N=(0,0,1), tangent T=(1,0,0) and bitangent B=(0,1,0).
    - the outgoing direction is always in the positive hemisphere.
    - evaluating the BxDF always includes the foreshortening term (dot(wi, n) = wi.z).
*/
interface IBxDF
{
    /** Evaluates the BxDF.
        \param[in] wo Outgoing direction.
        \param[in] wi Incident direction.
        \return Returns f(wo, wi) * dot(wi, n).
    */
    float3 eval(float3 wo, float3 wi);

    /** Samples the BxDF.
        \param[in] wo Outgoing direction.
        \param[out] wi Incident direction.
        \param[out] pdf pdf with respect to solid angle for sampling incident direction wi (0 if a delta event is sampled).
        \param[out] weight Sample weight f(wo, wi) * dot(wi, n) / pdf(wi).
        \param[out] lobe Sampled lobe.
        \param[inout] sg Sample generator.
        \return Returns true if successful.
    */
    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, out uint lobe, inout SampleGenerator sg);

    /** Evaluates the BxDF directional pdf for sampling incident direction wi.
        \param[in] wo Outgoing direction.
        \param[in] wi Incident direction.
        \return Returns the pdf with respect to solid angle for sampling incident direction wi (0 for delta events).
    */
    float evalPdf(float3 wo, float3 wi);
}

/** Lambertian diffuse reflection.
    f_r(wo, wi) = albedo / pi
*/
struct DiffuseReflectionLambert : IBxDF
{
    float3 albedo;  ///< Diffuse albedo.

    float3 eval(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < kMinCosTheta) return float3(0);

        return INV_PI * albedo * wi.z;
    }

    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, out uint lobe, inout SampleGenerator sg)
    {
        wi = sample_cosine_hemisphere_concentric(sampleNext2D(sg), pdf);

        if (min(wo.z, wi.z) < kMinCosTheta) return false;

        weight = albedo;
        lobe = (uint)LobeType::DiffuseReflection;
        return true;
    }

    float evalPdf(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < kMinCosTheta) return 0;

        return INV_PI * wi.z;
    }
};

/** Disney's diffuse reflection.
    Based on https://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
*/
struct DiffuseReflectionDisney : IBxDF
{
    float3 albedo;          ///< Diffuse albedo.
    float linearRoughness;  ///< Roughness before remapping.

    float3 eval(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < kMinCosTheta) return float3(0);

        return evalWeight(wo, wi) * INV_PI * wi.z;
    }

    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, out uint lobe, inout SampleGenerator sg)
    {
        wi = sample_cosine_hemisphere_concentric(sampleNext2D(sg), pdf);

        if (min(wo.z, wi.z) < kMinCosTheta) return false;

        weight = evalWeight(wo, wi);
        lobe = (uint)LobeType::DiffuseReflection;
        return true;
    }

    float evalPdf(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < kMinCosTheta) return 0;

        return INV_PI * wi.z;
    }

    // private

    // Returns f(wo, wi) * pi.
    float3 evalWeight(float3 wo, float3 wi)
    {
        float3 h = normalize(wo + wi);
        float wiDotH = dot(wi, h);
        float fd90 = 0.5 + 2 * wiDotH * wiDotH * linearRoughness;
        float fd0 = 1;
        float wiScatter = evalFresnelSchlick(fd0, fd90, wi.z).r;
        float woScatter = evalFresnelSchlick(fd0, fd90, wo.z).r;
        return albedo * wiScatter * woScatter;
    }
};

/** Frostbites's diffuse reflection.
    This is Disney's diffuse BRDF with an ad-hoc normalization factor to ensure energy conservation.
    Based on https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
*/
struct DiffuseReflectionFrostbite : IBxDF
{
    float3 albedo;          ///< Diffuse albedo.
    float linearRoughness;  ///< Roughness before remapping.

    float3 eval(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < kMinCosTheta) return float3(0);

        return evalWeight(wo, wi) * INV_PI * wi.z;
    }

    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, out uint lobe, inout SampleGenerator sg)
    {
        wi = sample_cosine_hemisphere_concentric(sampleNext2D(sg), pdf);

        if (min(wo.z, wi.z) < kMinCosTheta) return false;

        weight = evalWeight(wo, wi);
        lobe = (uint)LobeType::DiffuseReflection;
        return true;
    }

    float evalPdf(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < kMinCosTheta) return 0;

        return INV_PI * wi.z;
    }

    // private

    // Returns f(wo, wi) * pi.
    float3 evalWeight(float3 wo, float3 wi)
    {
        float3 h = normalize(wo + wi);
        float wiDotH = dot(wi, h);
        float energyBias = lerp(0, 0.5, linearRoughness);
        float energyFactor = lerp(1, 1.0 / 1.51, linearRoughness);
        float fd90 = energyBias + 2 * wiDotH * wiDotH * linearRoughness;
        float fd0 = 1;
        float wiScatter = evalFresnelSchlick(fd0, fd90, wi.z).r;
        float woScatter = evalFresnelSchlick(fd0, fd90, wo.z).r;
        return albedo * wiScatter * woScatter * energyFactor;
    }
};

/** Specular reflection using microfacets.
*/
struct SpecularReflectionMicrofacet : IBxDF
{
    float3 albedo;  ///< Specular albedo.
    float alpha;    ///< GGX width parameter.

    float3 eval(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < kMinCosTheta) return float3(0);

#if EnableDeltaBSDF
        // Handle delta reflection.
        if (alpha == 0) return float3(0);
#endif

        float3 h = normalize(wo + wi);
        float woDotH = dot(wo, h);

        float D = evalNdfGGX(alpha, h.z);
#if SpecularMaskingFunction == SpecularMaskingFunctionSmithGGXSeparable
        float G = evalMaskingSmithGGXSeparable(alpha, wo.z, wi.z);
#elif SpecularMaskingFunction == SpecularMaskingFunctionSmithGGXCorrelated
        float G = evalMaskingSmithGGXCorrelated(alpha, wo.z, wi.z);
#endif
        float3 F = evalFresnelSchlick(albedo, 1, woDotH);
        return F * D * G * 0.25 / wo.z;
    }

    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, out uint lobe, inout SampleGenerator sg)
    {
        if (wo.z < kMinCosTheta) return false;

#if EnableDeltaBSDF
        // Handle delta reflection.
        if (alpha == 0)
        {
            wi = float3(-wo.x, -wo.y, wo.z);
            pdf = 0;
            weight = evalFresnelSchlick(albedo, 1, wo.z);
            lobe = (uint)LobeType::DeltaReflection;
            return true;
        }
#endif

        // Sample the GGX distribution to find a microfacet normal (half vector).
#if EnableVNDFSampling
        float3 h = sampleGGX_VNDF(alpha, wo, sampleNext2D(sg), pdf);    // pdf = G1(wo) * D(h) * max(0,dot(wo,h)) / wo.z
#else
        float3 h = sampleGGX_NDF(alpha, sampleNext2D(sg), pdf);         // pdf = D(h) * h.z
#endif

        // Reflect the outgoing direction to find the incident direction.
        float woDotH = dot(wo, h);
        wi = 2 * woDotH * h - wo;
        if (wi.z < kMinCosTheta) return false;

#if ExplicitSampleWeights
        // For testing.
        pdf = evalPdf(wo, wi);
        weight = eval(wo, wi) / pdf;
        lobe = (uint)LobeType::SpecularReflection;
        return true;
#endif

#if SpecularMaskingFunction == SpecularMaskingFunctionSmithGGXSeparable
        float G = evalMaskingSmithGGXSeparable(alpha, wo.z, wi.z);
        float GOverG1wo = evalG1GGX(alpha * alpha, wi.z);
#elif SpecularMaskingFunction == SpecularMaskingFunctionSmithGGXCorrelated
        float G = evalMaskingSmithGGXCorrelated(alpha, wo.z, wi.z);
        float GOverG1wo = G * (1.f + evalLambdaGGX(alpha * alpha, wo.z));
#endif
        float3 F = evalFresnelSchlick(albedo, 1, woDotH);

        pdf /= (4 * woDotH); // Jacobian of the reflection operator.
#if EnableVNDFSampling
        weight = F * GOverG1wo;
#else
        weight = F * G * woDotH / (wo.z * h.z);
#endif
        lobe = (uint)LobeType::SpecularReflection;
        return true;
    }

    float evalPdf(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < kMinCosTheta) return 0;

#if EnableDeltaBSDF
        // Handle delta reflection.
        if (alpha == 0) return 0;
#endif

        float3 h = normalize(wo + wi);
        float woDotH = dot(wo, h);
#if EnableVNDFSampling
        float pdf = evalPdfGGX_VNDF(alpha, wo, h);
#else
        float pdf = evalPdfGGX_NDF(alpha, h.z);
#endif
        return pdf / (4 * woDotH);
    }
};

/** Specular reflection and transmission using microfacets.
*/
struct SpecularReflectionTransmissionMicrofacet : IBxDF
{
    float alpha;    ///< GGX width parameter.
    float eta;      ///< Relative index of refraction (e.g. etaI / etaT).

    float3 eval(float3 wo, float3 wi)
    {
        if (min(wo.z, abs(wi.z)) < kMinCosTheta) return float3(0);

#if EnableDeltaBSDF
        // Handle delta reflection/transmission.
        if (alpha == 0) return float3(0);
#endif

        bool isReflection = wi.z > 0;

        float3 h =
            isReflection ?
            normalize(wo + wi) :
            normalize(-(wo * eta + wi));

        float woDotH = dot(wo, h);
        float wiDotH = dot(wi, h);

        float D = evalNdfGGX(alpha, h.z);
#if SpecularMaskingFunction == SpecularMaskingFunctionSmithGGXSeparable
        float G = evalMaskingSmithGGXSeparable(alpha, wo.z, abs(wi.z));
#elif SpecularMaskingFunction == SpecularMaskingFunctionSmithGGXCorrelated
        float G = evalMaskingSmithGGXCorrelated(alpha, wo.z, abs(wi.z));
#endif
        float F = evalFresnelDielectric(eta, woDotH);

        if (isReflection)
        {
            return F * D * G * 0.25 / wo.z;
        }
        else
        {
            float sqrtDenom = woDotH + eta * wiDotH;
            float t = eta * eta * abs(woDotH * wiDotH) / (abs(wo.z * wi.z) * sqrtDenom * sqrtDenom);
            return (1 - F) * D * G * t * abs(wi.z);
        }
    }

    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, out uint lobe, inout SampleGenerator sg)
    {
        if (wo.z < kMinCosTheta) return false;

#if EnableDeltaBSDF
        // Handle delta reflection/transmission.
        if (alpha == 0)
        {
            float cosThetaT;
            float F = evalFresnelDielectric(eta, wo.z, cosThetaT);
            bool isReflection = sampleNext1D(sg) < F;

            pdf = 0;
            weight = float3(1);
            wi = isReflection ? float3(-wo.x, -wo.y, wo.z) : float3(-wo.x * eta, -wo.y * eta, -cosThetaT);
            lobe = isReflection ? (uint)LobeType::DeltaReflection : (uint)LobeType::DeltaTransmission;

            if (abs(wi.z) < kMinCosTheta || (wi.z > 0 != isReflection)) return false;

            return true;
        }
#endif

        // Sample the GGX distribution of (visible) normals. This is our half vector.
#if EnableVNDFSampling
        float3 h = sampleGGX_VNDF(alpha, wo, sampleNext2D(sg), pdf);    // pdf = G1(wo) * D(h) * max(0,dot(wo,h)) / wo.z
#else
        float3 h = sampleGGX_NDF(alpha, sampleNext2D(sg), pdf);         // pdf = D(h) * h.z
#endif

        // Reflect/refract the outgoing direction to find the incident direction.
        float woDotH = dot(wo, h);

        float cosThetaT;
        float F = evalFresnelDielectric(eta, woDotH, cosThetaT);
        bool isReflection = sampleNext1D(sg) < F;

        wi = isReflection ?
            2 * woDotH * h - wo :
            (eta * woDotH - cosThetaT) * h - eta * wo;

        if (abs(wi.z) < kMinCosTheta || (wi.z > 0 != isReflection)) return false;

        float wiDotH = dot(wi, h);

        lobe = isReflection ? (uint)LobeType::SpecularReflection : (uint)LobeType::SpecularTransmission;

#if ExplicitSampleWeights
        // For testing.
        pdf = evalPdf(wo, wi);
        weight = eval(wo, wi) / pdf;
        return true;
#endif

#if SpecularMaskingFunction == SpecularMaskingFunctionSmithGGXSeparable
        float G = evalMaskingSmithGGXSeparable(alpha, wo.z, abs(wi.z));
        float GOverG1wo = evalG1GGX(alpha * alpha, abs(wi.z));
#elif SpecularMaskingFunction == SpecularMaskingFunctionSmithGGXCorrelated
        float G = evalMaskingSmithGGXCorrelated(alpha, wo.z, abs(wi.z));
        float GOverG1wo = G * (1.f + evalLambdaGGX(alpha * alpha, wo.z));
#endif

        if (isReflection)
        {
            pdf *= F / (4 * wiDotH); // Jacobian of the reflection operator.
        }
        else
        {
            float sqrtDenom = woDotH + eta * wiDotH;
            pdf *= (1 - F) * eta * eta * wiDotH / (sqrtDenom * sqrtDenom); // Jacobian of the refraction operator.
        }

#if EnableVNDFSampling
        weight = GOverG1wo;
#else
        weight = G * woDotH / (wo.z * h.z);
#endif

        return true;
    }

    float evalPdf(float3 wo, float3 wi)
    {
        if (min(wo.z, abs(wi.z)) < kMinCosTheta) return 0;

#if EnableDeltaBSDF
        // Handle delta reflection/transmission.
        if (alpha == 0) return 0;
#endif

        bool isReflection = wi.z > 0;

        float3 h =
            isReflection ?
            normalize(wo + wi) :
            normalize(-(wo * eta + wi));

        float woDotH = dot(wo, h);
        float wiDotH = dot(wi, h);

        float F = evalFresnelDielectric(eta, woDotH);

#if EnableVNDFSampling
        float pdf = evalPdfGGX_VNDF(alpha, wo, h);
#else
        float pdf = evalPdfGGX_NDF(alpha, h.z);
#endif
        if (isReflection)
        {
            return F * pdf / (4 * wiDotH);
        }
        else
        {
            float sqrtDenom = woDotH + eta * wiDotH;
            return (1 - F) * pdf * eta * eta * wiDotH / (sqrtDenom * sqrtDenom);
        }
    }
};

/** Layered BSDF used as primary material in Falcor.

    This consists of a diffuse and specular BRDF.
    A specular BSDF is mixed in using the specularTransmission parameter.
*/
struct FalcorBSDF : IBxDF
{
#if DiffuseBrdf == DiffuseBrdfLambert
    DiffuseReflectionLambert diffuseReflection;
#elif DiffuseBrdf == DiffuseBrdfDisney
    DiffuseReflectionDisney diffuseReflection;
#elif DiffuseBrdf == DiffuseBrdfFrostbite
    DiffuseReflectionFrostbite diffuseReflection;
#endif

    SpecularReflectionMicrofacet specularReflection;
    SpecularReflectionTransmissionMicrofacet specularReflectionTransmission;

    float specularTransmission;             ///< Mix between dielectric BRDF and specular BSDF.

    float pDiffuseReflection;               ///< Probability for sampling the diffuse BRDF.
    float pSpecularReflection;              ///< Probability for sampling the specular BRDF.
    float pSpecularReflectionTransmission;  ///< Probability for sampling the specular BSDF.

    /** Setup the BSDF for sampling and evaluation.
        TODO: Currently specular reflection and transmission lobes are not properly separated.
        This leads to incorrect behaviour if only the specular reflection or transmission lobe is selected.
        Things work fine as long as both or none are selected.
        \param[in] sd Shading data.
    */
    [mutating] void setup(const ShadingData sd)
    {
        // Setup lobes.
        diffuseReflection.albedo = sd.diffuse;
#if DiffuseBrdf != DiffuseBrdfLambert
        diffuseReflection.linearRoughness = sd.linearRoughness;
#endif

        // Compute GGX alpha.
        float alpha = sd.linearRoughness * sd.linearRoughness;
#if EnableDeltaBSDF
        // Alpha below min alpha value means using delta reflection/transmission.
        if (alpha < kMinGGXAlpha) alpha = 0;
#else
        alpha = max(alpha, kMinGGXAlpha);
#endif

        specularReflection.albedo = sd.specular;
        specularReflection.alpha = alpha;

        specularReflectionTransmission.alpha = alpha;
        specularReflectionTransmission.eta = sd.eta;

        specularTransmission = sd.specularTransmission;

        // Compute sampling weights.
        float metallicBRDF = sd.metallic;
        float specularBSDF = (1 - sd.metallic) * sd.specularTransmission;
        float dielectricBRDF = (1 - sd.metallic) * (1 - sd.specularTransmission);

        float diffuseWeight = luminance(sd.diffuse);
        float specularWeight = luminance(evalFresnelSchlick(sd.specular, 1.f, dot(sd.V, sd.N)));

        pDiffuseReflection = (bool)(sd.activeLobes & (uint)LobeType::DiffuseReflection) ? diffuseWeight * dielectricBRDF : 0;
        pSpecularReflection = (bool)(sd.activeLobes & ((uint)LobeType::SpecularReflection | (uint)LobeType::DeltaReflection)) ? specularWeight * (metallicBRDF + dielectricBRDF) : 0;
        pSpecularReflectionTransmission = (bool)(sd.activeLobes & ((uint)LobeType::SpecularTransmission | (uint)LobeType::DeltaTransmission)) ? specularBSDF : 0;

        float normFactor = pDiffuseReflection + pSpecularReflection + pSpecularReflectionTransmission;
        if (normFactor > 0)
        {
            normFactor = 1 / normFactor;
            pDiffuseReflection *= normFactor;
            pSpecularReflection *= normFactor;
            pSpecularReflectionTransmission *= normFactor;
        }
    }

    /** Returns the set of BSDF lobes given some shading data.
        \param[in] sd Shading data.
        \return Returns a set of lobes (see LobeType in BxDFTypes.slang).
    */
    static uint getLobes(const ShadingData sd)
    {
#if EnableDeltaBSDF
        float alpha = sd.linearRoughness * sd.linearRoughness;
        bool isDelta = alpha < kMinGGXAlpha;
#else
        bool isDelta = false;
#endif
        uint lobes = isDelta ? (uint)LobeType::DeltaReflection : (uint)LobeType::SpecularReflection;
        if (any(sd.diffuse > 0) && sd.specularTransmission < 1) lobes |= (uint)LobeType::DiffuseReflection;
        if (sd.specularTransmission > 0) lobes |= (isDelta ? (uint)LobeType::DeltaTransmission : (uint)LobeType::SpecularTransmission);

        return lobes;
    }

    float3 eval(float3 wo, float3 wi)
    {
        float3 result = 0;
        if (pDiffuseReflection > 0) result += (1 - specularTransmission) * diffuseReflection.eval(wo, wi);
        if (pSpecularReflection > 0) result += (1 - specularTransmission) * specularReflection.eval(wo, wi);
        if (pSpecularReflectionTransmission > 0) result += specularTransmission * (specularReflectionTransmission.eval(wo, wi));
        return result;
    }

    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, out uint lobe, inout SampleGenerator sg)
    {
        bool valid = false;
        float uSelect = sampleNext1D(sg);

        if (uSelect < pDiffuseReflection)
        {
            valid = diffuseReflection.sample(wo, wi, pdf, weight, lobe, sg);
            weight /= pDiffuseReflection;
            weight *= (1 - specularTransmission);
            pdf *= pDiffuseReflection;
            if (pSpecularReflection > 0) pdf += pSpecularReflection * specularReflection.evalPdf(wo, wi);
            if (pSpecularReflectionTransmission > 0) pdf += pSpecularReflectionTransmission * specularReflectionTransmission.evalPdf(wo, wi);
        }
        else if (uSelect < pDiffuseReflection + pSpecularReflection)
        {
            valid = specularReflection.sample(wo, wi, pdf, weight, lobe, sg);
            weight /= pSpecularReflection;
            weight *= (1 - specularTransmission);
            pdf *= pSpecularReflection;
            if (pDiffuseReflection > 0) pdf += pDiffuseReflection * diffuseReflection.evalPdf(wo, wi);
            if (pSpecularReflectionTransmission > 0) pdf += pSpecularReflectionTransmission * specularReflectionTransmission.evalPdf(wo, wi);
        }
        else if (pSpecularReflectionTransmission > 0)
        {
            valid = specularReflectionTransmission.sample(wo, wi, pdf, weight, lobe, sg);
            weight /= pSpecularReflectionTransmission;
            weight *= specularTransmission;
            pdf *= pSpecularReflectionTransmission;
            if (pDiffuseReflection > 0) pdf += pDiffuseReflection * diffuseReflection.evalPdf(wo, wi);
            if (pSpecularReflection > 0) pdf += pSpecularReflection * specularReflection.evalPdf(wo, wi);
        }

        return valid;
    }

    float evalPdf(float3 wo, float3 wi)
    {
        float pdf = 0;
        if (pDiffuseReflection > 0) pdf += pDiffuseReflection * diffuseReflection.evalPdf(wo, wi);
        if (pSpecularReflection > 0) pdf += pSpecularReflection * specularReflection.evalPdf(wo, wi);
        if (pSpecularReflectionTransmission > 0) pdf += pSpecularReflectionTransmission * specularReflectionTransmission.evalPdf(wo, wi);
        return pdf;
    }
};



#endif