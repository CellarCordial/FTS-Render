#ifndef SHADER_BRDF_SLANG
#define SHADER_BRDF_SLANG

#include "MathHelper.slang"

#define GGX_VNDF_SAMPLE

// 0: SmithGGXSeparable
// 1: SmithGGXCorrelated
#define SPECULAR_MASKING_FUNCTION 0

// Minimum cos(theta) for the view and light vectors.
static const float gfMinCosTheta = 1e-6f;

float EvalNdfGGX(float alpha, float cosTheta)
{
    float a2 = alpha * alpha;
    float d = ((cosTheta * a2 - cosTheta) * cosTheta + 1);
    return a2 / (d * d * PI);
}

float EvalG1GGX(float alphaSqr, float cosTheta)
{
    if (cosTheta <= 0) return 0;
    float cosThetaSqr = cosTheta * cosTheta;
    float tanThetaSqr = max(1 - cosThetaSqr, 0) / cosThetaSqr;
    return 2 / (1 + sqrt(1 + alphaSqr * tanThetaSqr));
}

float EvalPdfGGX_VNDF(float alpha, float3 wo, float3 h)
{
    //              G_1(v, h) * D(h) * dot(v, h)
    // PDF(w_i) = --------------------------------
    //                      dot(n, v)
    float G1 = EvalG1GGX(alpha * alpha, wo.z);
    float D = EvalNdfGGX(alpha, h.z);
    return G1 * D * max(0.f, dot(wo, h)) / wo.z;
}

float3 SampleGGX_VNDF(float alpha, float3 wo, float2 u, out float pdf)
{
    // alpha 为粗糙度, 这里便是各向同性.
    float alpha_x = alpha;
    float alpha_y = alpha;

    // 将椭球上的视向量 V_e 转换为半球上的视向量 V_h.
    // 等同于
    // normalize(mul(
    //    wo,
    //    float3x3(
    //      alpha_x, 0.0f, 0.0f,
    //      0.0f, alpha_y, 0.0f,
    //      0.0f, 0.0f, 1.0f
    //    )
    // ))
    float3 Vh = normalize(float3(alpha_x * wo.x, alpha_y * wo.y, wo.z));

    // 以 V_h 为轴的正交基底 (Vh,T1,T2).
    float3 T1 = (Vh.z < 0.9999f) ? normalize(cross(float3(0, 0, 1), Vh)) : float3(1, 0, 0);
    float3 T2 = cross(Vh, T1);

    // 在半球投影区域上采样.
    float r = sqrt(u.x);
    float phi = (2.f * PI) * u.y;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5f * (1.f + Vh.z);
    t2 = (1.f - s) * sqrt(1.f - t1 * t1) + s * t2;

    // 在半球中，通过采样半球的投影面积来采样随机法线 N_h.
    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.f, 1.f - t1 * t1 - t2 * t2)) * Vh;

    // 将这个随机法线转换回椭球，我们得到一个从椭球的VNDF中采样得到的法线 N_e, 即需要的 half vector.
    float3 h = normalize(float3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.f, Nh.z)));

    pdf = EvalPdfGGX_VNDF(alpha, wo, h);
    return h;
}

float EvalPdfGGX_NDF(float alpha, float cosTheta)
{
    return EvalNdfGGX(alpha, cosTheta) * cosTheta;
}

float3 SampleGGX_NDF(float alpha, float2 u, out float pdf)
{
    // BRDF 重要性采样
    // 0 ~ 1 的均匀随机数 float2(u), 我们可以得到采样方向(theta，phi),
    //                              1 - u.x
    // theta = arccos(sqrt(-------------------------))
    //                      1 + u.x * (alpha^2 - 1)
    // phi = 2 * PI * u.y

    float alphaSqr = alpha * alpha;
    float phi = u.y * (2 * PI);
    float tanThetaSqr = alphaSqr * u.x / (1 - u.x);
    float cosTheta = 1 / sqrt(1 + tanThetaSqr);
    float r = sqrt(max(1 - cosTheta * cosTheta, 0));

    pdf = EvalPdfGGX_NDF(alpha, cosTheta);
    return float3(cos(phi) * r, sin(phi) * r, cosTheta);
}

float3 EvalFresnelSchlick(float3 f0, float3 f90, float cosTheta)
{
    return f0 + (f90 - f0) * pow(max(1 - cosTheta, 0), 5); // Clamp to avoid NaN if cosTheta = 1+epsilon
}

float EvalLambdaGGX(float alphaSqr, float cosTheta)
{
    if (cosTheta <= 0) return 0;
    float cosThetaSqr = cosTheta * cosTheta;
    float tanThetaSqr = max(1 - cosThetaSqr, 0) / cosThetaSqr;
    return 0.5 * (-1 + sqrt(1 + alphaSqr * tanThetaSqr));
}

float EvalMaskingSmithGGXSeparable(float alpha, float cosThetaI, float cosThetaO)
{
    float alphaSqr = alpha * alpha;
    float lambdaI = EvalLambdaGGX(alphaSqr, cosThetaI);
    float lambdaO = EvalLambdaGGX(alphaSqr, cosThetaO);
    return 1 / ((1 + lambdaI) * (1 + lambdaO));
}

float EvalMaskingSmithGGXCorrelated(float alpha, float cosThetaI, float cosThetaO)
{
    float alphaSqr = alpha * alpha;
    float lambdaI = EvalLambdaGGX(alphaSqr, cosThetaI);
    float lambdaO = EvalLambdaGGX(alphaSqr, cosThetaO);
    return 1 / (1 + lambdaI + lambdaO);
}

interface IBrdf
{
    float3 Eval(float3 wo, float3 wi);
    bool Sample(float3 wo, float2 Random, out float3 wi, out float fPdf, out float3 weight);
    float EvalPdf(float3 wo, float3 wi);
};

struct FDiffuseReflection : IBrdf
{
    float3 Eval(float3 wo, float3 wi)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    bool Sample(float3 wo, float2 Random, out float3 wi, out float fPdf, out float3 weight)
    {
        return false;
    }

    float EvalPdf(float3 wo, float3 wi)
    {
        return 0.0f;
    }
};

struct FSpecularReflection : IBrdf
{
    float3 Albedo = float3(0.0f, 0.0f, 0.0f); ///< Specular albedo.
    float fAlpha = 0.0f;   ///< GGX width parameter.

    float3 Eval(float3 wo, float3 wi)
    {
        if (min(wo.z, wi.z) < gfMinCosTheta) return float3(0.0f, 0.0f, 0.0f);

        float3 h = normalize(wo + wi);
        float woDotH = dot(wo, h);

        float D = EvalNdfGGX(fAlpha, h.z);
#if SPECULAR_MASKING_FUNCTION
        float G = EvalMaskingSmithGGXSeparable(fAlpha, wo.z, wi.z);
#else
        float G = EvalMaskingSmithGGXCorrelated(fAlpha, wo.z, wi.z);
#endif
        float3 F = EvalFresnelSchlick(Albedo, 1, woDotH);
        return F * D * G * 0.25 / wo.z;
    }

    bool Sample(float3 wo, float2 Random, out float3 wi, out float fPdf, out float3 Weight)
    {
        if (wo.z < gfMinCosTheta) return false;

        // Sample the GGX distribution to find a microfacet normal (half vector).
#ifdef GGX_VNDF_SAMPLE
        float3 h = SampleGGX_VNDF(fAlpha, wo, Random, fPdf); // pdf = G1(wo) * D(h) * max(0,dot(wo,h)) / wo.z
#else
        float3 h = SampleGGX_NDF(fAlpha, Random, fPdf); // pdf = D(h) * h.z
#endif

        // Reflect the outgoing direction to find the incident direction.
        float woDotH = dot(wo, h);
        wi = 2 * woDotH * h - wo;
        if (wi.z < gfMinCosTheta) return false;

#ifdef SPECULAR_MASKING_FUNCTION
        float G = EvalMaskingSmithGGXSeparable(fAlpha, wo.z, wi.z);
        float GOverG1wo = EvalG1GGX(fAlpha * fAlpha, wi.z);
#else
        float G = evalMaskingSmithGGXCorrelated(fAlpha, wo.z, wi.z);
        float GOverG1wo = G * (1.f + evalLambdaGGX(fAlpha * fAlpha, wo.z));
#endif
        float3 F = EvalFresnelSchlick(Albedo, 1.0f, woDotH);

        fPdf /= (4 * woDotH); // Jacobian of the reflection operator.

#ifdef GGX_VNDF_SAMPLE
        Weight = F * GOverG1wo;
#else
        Weight = F * G * woDotH / (wo.z * h.z);
#endif
        return true;
    }

    float EvalPdf(float3 wo, float3 wi)
    {
        return 0.0f;
    }
};

struct FBrdf : IBrdf
{
    float3 Eval(float3 wo, float3 wi)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    bool Sample(float3 wo, float2 Random, out float3 wi, out float fPdf, out float3 weight)
    {
        return false;
    }

    float EvalPdf(float3 wo, float3 wi)
    {
        return 0.0f;
    }
};


bool ImportanceSampleGGX(float fRoughness, float3 ViewDir, float2 Random, out float pdf)
{
#ifdef GGX_VNDF_SAMPLE
    float fPdf;
    float3 h = SampleGGX_VNDF(fRoughness, ViewDir, Random, fPdf);
#else
    float fPdf;
    float3 h = SampleGGX_NDF(fRoughness, Random, fPdf);
#endif

    return false;
}



#endif