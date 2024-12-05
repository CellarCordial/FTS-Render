#ifndef SHADER_BRDF_COMMON_HLSLI
#define SHADER_BRDF_COMMON_HLSLI

// #define GGX_VNDF_SAMPLE 0

#include "Constants.hlsli"

struct FBrdfSample
{
    float fPdf;
    float3 Value;
    float3 ValueDividePdf;
    float3 LightDir;
};

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
    float G1 = EvalG1GGX(alpha * alpha, wo.z);
    float D = EvalNdfGGX(alpha, h.z);
    return G1 * D * max(0.f, dot(wo, h)) / wo.z;
}


float3 SampleGGX_VNDF(float alpha, float3 wo, float2 u, out float pdf)
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

    pdf = EvalPdfGGX_VNDF(alpha, wo, h);
    return h;
}

float EvalPdfGGX_NDF(float alpha, float cosTheta)
{
    return EvalNdfGGX(alpha, cosTheta) * cosTheta;
}

float3 SampleGGX_NDF(float alpha, float2 u, out float pdf)
{
    float alphaSqr = alpha * alpha;
    float phi = u.y * (2 * PI);
    float tanThetaSqr = alphaSqr * u.x / (1 - u.x);
    float cosTheta = 1 / sqrt(1 + tanThetaSqr);
    float r = sqrt(max(1 - cosTheta * cosTheta, 0));

    pdf = EvalPdfGGX_NDF(alpha, cosTheta);
    return float3(cos(phi) * r, sin(phi) * r, cosTheta);
}

float3 ImportanceSampleGGX(float fRoughness, float3 ViewDir, float2 Random)
{
#ifdef GGX_VNDF_SAMPLE
    float fPdf;
    float3 h = SampleGGX_VNDF(fRoughness, ViewDir, Random, fPdf);
#else
    float fPdf;
    float3 h = SampleGGX_NDF(fRoughness, Random, fPdf);
#endif



    return float3(0.0f, 0.0f, 0.0f);
}



#endif