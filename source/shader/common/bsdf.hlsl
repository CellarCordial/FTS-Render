#ifndef SHADER_COMMON_BXDF_H
#define SHADER_COMMON_BXDF_H

#include "math.hlsl"

#define DiffuseBrdfLambert 0
#define DiffuseBrdfDisney 1
#define DiffuseBrdfFrostbite 2

static const float min_ggx_alpha = 0.0064f;

static const float min_cos_theta = 1e-6f;

// 单位圆盘上的均匀采样.
// random 为单位矩形上的点, 范围为 [0, 1).
float2 sample_disk_concentric(float2 random)
{
    float2 u = 2.0f * random - 1.0f;
    if (u.x == 0.f && u.y == 0.f) return u;
    float phi, radius;

    // 将单位矩形用 x 划分为上下左右四个区块, 避免中心区域的点密度过高.
    if (abs(u.x) > abs(u.y)) // 矩形左右区块, 生成的采样点同样在圆盘的左右两侧.
    {
        radius = u.x;
        phi = (u.y / u.x) * PI / 4;
    }
    else // 矩形上下区块, 生成的采样点同样在圆盘的上下两侧.
    {
        radius = u.y;
        phi = PI / 2 - (u.x / u.y) * PI / 4;
    }
    return radius * float2(cos(phi), sin(phi));
}

// 半球上余弦加权采样.
float3 sample_cosine_hemisphere_concentric(float2 random, out float pdf)
{
    // 先生成单位圆盘上的点, 再计算出其在半球上的 z 值.
    // 余弦加权采样的 pdf 为 cosθ / π，其中 cosθ = z.

    float2 d = sample_disk_concentric(random);
    float z = sqrt(max(0.f, 1.f - dot(d, d)));
    pdf = z * INV_PI;
    return float3(d, z);
}

float3 eval_fresnel_schlick(float3 f0, float3 f90, float cos_theta)
{
    return f0 + (f90 - f0) * pow(max(1 - cos_theta, 0), 5);
}

float eval_ndf_ggx(float alpha, float cos_theta)
{
    float a2 = alpha * alpha;
    float d = ((cos_theta * a2 - cos_theta) * cos_theta + 1);
    return a2 / (d * d * PI);
}

float eval_lambda_ggx(float alpha_sqrt, float cos_theta)
{
    if (cos_theta <= 0) return 0;
    float cos_theta_sqrt = cos_theta * cos_theta;
    float tan_theta_sqrt = max(1 - cos_theta_sqrt, 0) / cos_theta_sqrt;
    return 0.5 * (-1 + sqrt(1 + alpha_sqrt * tan_theta_sqrt));
}

float eval_masking_smith_ggx_correlated(float alpha, float cos_theta_in, float cos_theta_out)
{
    float alpha_sqrt = alpha * alpha;
    float lambda_in = eval_lambda_ggx(alpha_sqrt, cos_theta_in);
    float lambda_out = eval_lambda_ggx(alpha_sqrt, cos_theta_out);
    return 1 / (1 + lambda_in + lambda_out);
}

float eval_pdf_ggx_ndf(float alpha, float cos_theta)
{
    return eval_ndf_ggx(alpha, cos_theta) * cos_theta;
}

float3 sample_ggx_ndf(float alpha, float2 random, out float pdf)
{
    float alpha_sqrt = alpha * alpha;
    float phi = random.y * (2 * PI);
    float tan_theta_sqrt = alpha_sqrt * random.x / (1 - random.x);
    float cos_theta = 1 / sqrt(1 + tan_theta_sqrt);
    float r = sqrt(max(1 - cos_theta * cos_theta, 0));

    pdf = eval_pdf_ggx_ndf(alpha, cos_theta);
    return float3(cos(phi) * r, sin(phi) * r, cos_theta);
}

float eval_fresnel_dielectric(float eta, float cos_theta_in, out float cos_theta_t)
{
    if (cos_theta_in < 0)
    {
        eta = 1 / eta;
        cos_theta_in = -cos_theta_in;
    }

    float sinThetaTSq = eta * eta * (1 - cos_theta_in * cos_theta_in);
    // Check for total internal reflection
    if (sinThetaTSq > 1)
    {
        cos_theta_t = 0;
        return 1;
    }

    cos_theta_t = sqrt(1 - sinThetaTSq); // No clamp needed

    // Note that at eta=1 and cos_theta_in=0, we get cos_theta_t=0 and NaN below.
    // It's important the framework clamps |cos_theta_in| or eta to small epsilon.
    float Rs = (eta * cos_theta_in - cos_theta_t) / (eta * cos_theta_in + cos_theta_t);
    float Rp = (eta * cos_theta_t - cos_theta_in) / (eta * cos_theta_t + cos_theta_in);

    return 0.5 * (Rs * Rs + Rp * Rp);
}

float eval_fresnel_dielectric(float eta, float cos_theta_in)
{
    float cos_theta_t;
    return eval_fresnel_dielectric(eta, cos_theta_in, cos_theta_t);
}


// Lambertian diffuse reflection: f_r(wo, wi) = albedo / pi.
struct DiffuseReflectionLambert
{
    float3 albedo;

    float3 eval(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < min_cos_theta) return float3(0.0f, 0.0f, 0.0f);

        return albedo * INV_PI * wi.z;
    }

    float eval_pdf(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < min_cos_theta) return 0.0f;

        return INV_PI * wi.z;
    }

    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, float2 random)
    {
        wi = sample_cosine_hemisphere_concentric(random, pdf);

        if (min(wo.z, wi.z) < min_cos_theta) return false;

        weight = albedo;
        return true;
    }
};

// Disney's diffuse reflection.
struct DiffuseReflectionDisney
{
    float3 albedo;
    float linear_roughness;

    // returns f(wo, wi) * pi.
    float3 eval_weight(float3 wo, float3 wi)
    {
        float3 h = normalize(wo + wi);
        float wi_dot_h = dot(wi, h);
        float fd90 = 0.5 + 2 * wi_dot_h * wi_dot_h * linear_roughness;
        float fd0 = 1;
        float wi_scatter = eval_fresnel_schlick(fd0, fd90, wi.z).r;
        float wo_scatter = eval_fresnel_schlick(fd0, fd90, wo.z).r;
        return albedo * wi_scatter * wo_scatter;
    }

    float3 eval(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < min_cos_theta) return float3(0.0f, 0.0f, 0.0f);

        return eval_weight(wo, wi) * INV_PI * wi.z;
    }

    float eval_pdf(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < min_cos_theta) return 0;

        return INV_PI * wi.z;
    }

    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, float2 random)
    {
        wi = sample_cosine_hemisphere_concentric(random, pdf);

        if (min(wo.z, wi.z) < min_cos_theta) return false;

        weight = eval_weight(wo, wi);
        return true;
    }
};

// Frostbites's diffuse reflection.
struct DiffuseReflectionFrostbite
{
    float3 albedo;
    float linear_roughness;


    float3 eval_weight(float3 wo, float3 wi)
    {
        float3 h = normalize(wo + wi);
        float wi_dot_h = dot(wi, h);
        float energy_bias = lerp(0, 0.5, linear_roughness);
        float energy_factor = lerp(1, 1.0 / 1.51, linear_roughness);
        float fd90 = energy_bias + 2 * wi_dot_h * wi_dot_h * linear_roughness;
        float fd0 = 1;
        float wi_scatter = eval_fresnel_schlick(fd0, fd90, wi.z).r;
        float wo_scatter = eval_fresnel_schlick(fd0, fd90, wo.z).r;
        return albedo * wi_scatter * wo_scatter * energy_factor;
    }
    
    float3 eval(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < min_cos_theta) return float3(0.0f, 0.0f, 0.0f);

        return eval_weight(wo, wi) * INV_PI * wi.z;
    }

    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, float2 random)
    {
        wi = sample_cosine_hemisphere_concentric(random, pdf);

        if (min(wo.z, wi.z) < min_cos_theta) return false;

        weight = eval_weight(wo, wi);
        return true;
    }

    float eval_pdf(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < min_cos_theta) return 0;

        return INV_PI * wi.z;
    }
};


// Specular reflection using microfacets.
struct SpecularReflectionMicrofacet
{
    float3 albedo;
    float alpha;

    float3 eval(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < min_cos_theta) return float3(0);

        float3 h = normalize(wo + wi);
        float wo_dot_h = dot(wo, h);

        float D = eval_ndf_ggx(alpha, h.z);
        float G = eval_masking_smith_ggx_correlated(alpha, wo.z, wi.z);
        float3 F = eval_fresnel_schlick(albedo, 1, wo_dot_h);
        return F * D * G * 0.25 / wo.z;
    }

    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, float2 random)
    {
        if (wo.z < min_cos_theta) return false;

        // Sample the GGX distribution to find a microfacet normal (half vector).
        float3 h = sample_ggx_ndf(alpha, random, pdf); // pdf = D(h) * h.z

        // Reflect the outgoing direction to find the incident direction.
        float wo_dot_h = dot(wo, h);
        wi = 2 * wo_dot_h * h - wo;
        if (wi.z < min_cos_theta) return false;

        float G = eval_masking_smith_ggx_correlated(alpha, wo.z, wi.z);
        float G_over_G1_wo = G * (1.f + eval_lambda_ggx(alpha * alpha, wo.z));
        float3 F = eval_fresnel_schlick(albedo, 1, wo_dot_h);

        pdf /= (4 * wo_dot_h); // Jacobian of the reflection operator.
        weight = F * G * wo_dot_h / (wo.z * h.z);
        return true;
    }

    float eval_pdf(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < min_cos_theta) return 0;

        float3 h = normalize(wo + wi);
        float wo_dot_h = dot(wo, h);
        float pdf = eval_pdf_ggx_ndf(alpha, h.z);
        return pdf / (4 * wo_dot_h);
    }
};

// Specular reflection and transmission using microfacets.
struct SpecularReflectionTransmissionMicrofacet
{
    float alpha;
    float eta;

    float3 eval(float3 wo, float3 wi)
    {
        if (min(wo.z, abs(wi.z)) < min_cos_theta) return float3(0);

        bool is_reflection = wi.z > 0;

        float3 h = is_reflection ? normalize(wo + wi) : normalize(-(wo * eta + wi));

        float wo_dot_h = dot(wo, h);
        float wi_dot_h = dot(wi, h);

        float D = eval_ndf_ggx(alpha, h.z);
        float G = eval_masking_smith_ggx_correlated(alpha, wo.z, abs(wi.z));
        float F = eval_fresnel_dielectric(eta, wo_dot_h);

        if (is_reflection)
        {
            return F * D * G * 0.25 / wo.z;
        }
        else
        {
            float sqrt_denom = wo_dot_h + eta * wi_dot_h;
            float t = eta * eta * abs(wo_dot_h * wi_dot_h) / (abs(wo.z * wi.z) * sqrt_denom * sqrt_denom);
            return (1 - F) * D * G * t * abs(wi.z);
        }
    }

    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, float2 random)
    {
        if (wo.z < min_cos_theta) return false;

        float3 h = sample_ggx_ndf(alpha, random, pdf); // pdf = D(h) * h.z

        float wo_dot_h = dot(wo, h);

        float cos_theta_t;
        float F = eval_fresnel_dielectric(eta, wo_dot_h, cos_theta_t);
        bool is_reflection = random.x < F;

        wi = is_reflection ? 2 * wo_dot_h * h - wo : (eta * wo_dot_h - cos_theta_t) * h - eta * wo;

        if (abs(wi.z) < min_cos_theta || (wi.z > 0 != is_reflection)) return false;

        float wi_dot_h = dot(wi, h);

        float G = eval_masking_smith_ggx_correlated(alpha, wo.z, abs(wi.z));
        float G_over_G1_wo = G * (1.f + eval_lambda_ggx(alpha * alpha, wo.z));

        if (is_reflection)
        {
            pdf *= F / (4 * wi_dot_h);
        }
        else
        {
            float sqrt_denom = wo_dot_h + eta * wi_dot_h;
            pdf *= (1 - F) * eta * eta * wi_dot_h / (sqrt_denom * sqrt_denom);
        }

        weight = G * wo_dot_h / (wo.z * h.z);
        return true;
    }

    float eval_pdf(float3 wo, float3 wi)
    {
        if (min(wo.z, abs(wi.z)) < min_cos_theta) return 0;

        bool is_reflection = wi.z > 0;

        float3 h = is_reflection ? normalize(wo + wi) : normalize(-(wo * eta + wi));

        float wo_dot_h = dot(wo, h);
        float wi_dot_h = dot(wi, h);

        float F = eval_fresnel_dielectric(eta, wo_dot_h);

        float pdf = eval_pdf_ggx_ndf(alpha, h.z);
        if (is_reflection)
        {
            return F * pdf / (4 * wi_dot_h);
        }
        else
        {
            float sqrt_denom = wo_dot_h + eta * wi_dot_h;
            return (1 - F) * pdf * eta * eta * wi_dot_h / (sqrt_denom * sqrt_denom);
        }
    }
};

struct ShadingData
{
    float3 V;    // Direction to the eye at shading hit
    float3 N;    // Shading normal at shading hit
    float n_dot_v; // Unclamped, can be negative.

    float3 diffuse;  // Diffuse albedo.
    float3 specular;       // Specular albedo.
    float linear_roughness; // This is the original roughness, before re-mapping.
    float metallic;             // Metallic parameter, blends between dielectric and conducting BSDFs.
    float specular_transmission_mix; // Specular transmission, blends between opaque dielectric BRDF and specular transmissive BSDF.
    float eta;                  // Relative index of refraction (incident IoR / transmissive IoR).
};

inline float luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

struct BSDF
{
#if DiffuseBrdfLambert
    DiffuseReflectionLambert diffuse_reflection;
#elif DiffuseBrdfDisney
    DiffuseReflectionDisney diffuse_reflection;
#elif DiffuseBrdfFrostbite
    DiffuseReflectionFrostbite diffuse_reflection;
#endif

    SpecularReflectionMicrofacet specular_reflection;
    SpecularReflectionTransmissionMicrofacet specular_reflection_transmitssion;

    float specular_transmission_mix; // Mix between dielectric BRDF and specular BSDF.

    float diffuse_reflection_weight;              // Probability for sampling the diffuse BRDF.
    float specular_reflection_weight;             // Probability for sampling the specular BRDF.
    float specular_reflection_transmission_weight; // Probability for sampling the specular BSDF.

    void setup(const ShadingData data)
    {
        // Setup lobes.
        diffuse_reflection.albedo = data.diffuse;
#if !DiffuseBrdfLambert
        diffuse_reflection.linear_roughness = data.linear_roughness;
#endif

        // Compute GGX alpha.
        float alpha = data.linear_roughness * data.linear_roughness;
        alpha = max(alpha, min_ggx_alpha);

        specular_reflection.albedo = data.specular;
        specular_reflection.alpha = alpha;

        specular_reflection_transmitssion.alpha = alpha;
        specular_reflection_transmitssion.eta = data.eta;

        specular_transmission_mix = data.specular_transmission_mix;

        // Compute sampling weights.
        float metallic_brdf = data.metallic;
        float specular_brdf = (1 - data.metallic) * data.specular_transmission_mix;
        float dielectric_brdf = (1 - data.metallic) * (1 - data.specular_transmission_mix);

        diffuse_reflection_weight = luminance(data.diffuse) * dielectric_brdf;
        specular_reflection_weight = luminance(eval_fresnel_schlick(data.specular, 1.f, dot(data.V, data.N))) * (metallic_brdf + dielectric_brdf);
        specular_reflection_transmission_weight = specular_brdf;

        float normal_factor = diffuse_reflection_weight + specular_reflection_weight + specular_reflection_transmission_weight;
        if (normal_factor > 0)
        {
            normal_factor = 1 / normal_factor;
            diffuse_reflection_weight *= normal_factor;
            specular_reflection_weight *= normal_factor;
            specular_reflection_transmission_weight *= normal_factor;
        }
    }

    float3 eval(float3 wo, float3 wi)
    {
        float3 result = 0;
        if (diffuse_reflection_weight > 0) result += (1 - specular_transmission_mix) * diffuse_reflection.eval(wo, wi);
        if (specular_reflection_weight > 0) result += (1 - specular_transmission_mix) * specular_reflection.eval(wo, wi);
        if (specular_reflection_transmission_weight > 0) result += specular_transmission_mix * (specular_reflection_transmitssion.eval(wo, wi));
        return result;
    }

    bool sample(float3 wo, out float3 wi, out float pdf, out float3 weight, float2 random)
    {
        bool valid = false;
        float uSelect = 0.0f;

        if (uSelect < diffuse_reflection_weight)
        {
            valid = diffuse_reflection.sample(wo, wi, pdf, weight, random);
            weight /= diffuse_reflection_weight;
            weight *= (1 - specular_transmission_mix);
            pdf *= diffuse_reflection_weight;
            if (specular_reflection_weight > 0) pdf += specular_reflection_weight * specular_reflection.eval_pdf(wo, wi);
            if (specular_reflection_transmission_weight > 0) pdf += specular_reflection_transmission_weight * specular_reflection_transmitssion.eval_pdf(wo, wi);
        }
        else if (uSelect < diffuse_reflection_weight + specular_reflection_weight)
        {
            valid = specular_reflection.sample(wo, wi, pdf, weight, random);
            weight /= specular_reflection_weight;
            weight *= (1 - specular_transmission_mix);
            pdf *= specular_reflection_weight;
            if (diffuse_reflection_weight > 0) pdf += diffuse_reflection_weight * diffuse_reflection.eval_pdf(wo, wi);
            if (specular_reflection_transmission_weight > 0) pdf += specular_reflection_transmission_weight * specular_reflection_transmitssion.eval_pdf(wo, wi);
        }
        else if (specular_reflection_transmission_weight > 0)
        {
            valid = specular_reflection_transmitssion.sample(wo, wi, pdf, weight, random);
            weight /= specular_reflection_transmission_weight;
            weight *= specular_transmission_mix;
            pdf *= specular_reflection_transmission_weight;
            if (diffuse_reflection_weight > 0) pdf += diffuse_reflection_weight * diffuse_reflection.eval_pdf(wo, wi);
            if (specular_reflection_weight > 0) pdf += specular_reflection_weight * specular_reflection.eval_pdf(wo, wi);
        }

        return valid;
    }

    float eval_pdf(float3 wo, float3 wi)
    {
        float pdf = 0;
        if (diffuse_reflection_weight > 0) pdf += diffuse_reflection_weight * diffuse_reflection.eval_pdf(wo, wi);
        if (specular_reflection_weight > 0) pdf += specular_reflection_weight * specular_reflection.eval_pdf(wo, wi);
        if (specular_reflection_transmission_weight > 0) pdf += specular_reflection_transmission_weight * specular_reflection_transmitssion.eval_pdf(wo, wi);
        return pdf;
    }
};

#endif
