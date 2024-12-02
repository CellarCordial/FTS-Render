#ifndef SHADER_SDF_COMMON_HLSLI
#define SHADER_SDF_COMMON_HLSLI

#include "Intersect.hlsli"

struct FGlobalSdfData
{
    float fSceneGridSize;
    float3 SceneGridOrigin;

    uint dwMaxTraceSteps;
    float AbsThreshold;
    float fDefaultMarch;
    float PAD;
};

struct FSdfHitData
{
    uint dwStepCount;
    float fStepSize;
    float3 Normal;
    float fSdf;
};

FSdfHitData TraceGlobalSdf(float3 o, float3 d, FGlobalSdfData SdfData, bool bNeedNormal, Texture3D<float> GlobalSdf, SamplerState Sampler)
{
    FSdfHitData Ret;

    float3 p = o;
    float fInitStep;
    float3 SceneGridEnd = SdfData.SceneGridOrigin + SdfData.fSceneGridSize;
    if (any(abs(p) > SdfData.fSceneGridSize * 0.5f) && IntersectRayBoxInside(p, d, SdfData.SceneGridOrigin, SceneGridEnd, fInitStep))
    {
        p += d * fInitStep;
    }

    float fGlobalSdfResolution = 0.0f;
    if (bNeedNormal) 
    {
        float fTemp;
        GlobalSdf.GetDimensions(0, fGlobalSdfResolution, fGlobalSdfResolution, fGlobalSdfResolution, fTemp);
    }

    uint ix = 0;
    float fSdf = 0.0f;
    for (; ix < SdfData.dwMaxTraceSteps; ++ix)
    {
        float3 uvw = (p - SdfData.SceneGridOrigin) / SdfData.fSceneGridSize;
        if (any(saturate(uvw) != uvw)) { p = o; break; }

        fSdf = GlobalSdf.Sample(Sampler, uvw);

        // 若发现为空 chunk, 则加速前进.
        float Udf = abs(fSdf) < 0.00001f ? SdfData.fDefaultMarch : fSdf;
        if (abs(Udf) <= SdfData.AbsThreshold)
        {
            if (bNeedNormal)
            {
                float fOffset = 1.0f / fGlobalSdfResolution;
                float xp = GlobalSdf.Sample(Sampler, float3(uvw.x + fOffset, uvw.y, uvw.z));
                float xn = GlobalSdf.Sample(Sampler, float3(uvw.x - fOffset, uvw.y, uvw.z));
                float yp = GlobalSdf.Sample(Sampler, float3(uvw.x, uvw.y + fOffset, uvw.z));
                float yn = GlobalSdf.Sample(Sampler, float3(uvw.x, uvw.y - fOffset, uvw.z));
                float zp = GlobalSdf.Sample(Sampler, float3(uvw.x, uvw.y, uvw.z + fOffset));
                float zn = GlobalSdf.Sample(Sampler, float3(uvw.x, uvw.y, uvw.z - fOffset));
                Ret.Normal = normalize(float3(xp - xn, yp - yn, zp - zn));
            }
            break;
        }

        p += Udf * d;
    }

    Ret.fStepSize = length(p - o) / float(SdfData.dwMaxTraceSteps - 1);
    Ret.dwStepCount = ix;
    Ret.fSdf = fSdf;

    return Ret;
}












#endif