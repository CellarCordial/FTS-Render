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
    float fSdf;
};

FSdfHitData TraceGlobalSdf(float3 o, float3 d, FGlobalSdfData SdfData, Texture3D<float> GlobalSdf, SamplerState Sampler)
{
    FSdfHitData Ret;

    float3 p = o;
    float fInitStep;
    float3 SceneGridEnd = SdfData.SceneGridOrigin + SdfData.fSceneGridSize;
    if (any(abs(p) > SdfData.fSceneGridSize * 0.5f) && IntersectRayBoxInside(p, d, SdfData.SceneGridOrigin, SceneGridEnd, fInitStep))
    {
        p += d * fInitStep;
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
        if (abs(Udf) <= SdfData.AbsThreshold) break;

        p += Udf * d;
    }

    Ret.fStepSize = length(p - o) / float(SdfData.dwMaxTraceSteps - 1);
    Ret.dwStepCount = ix;
    Ret.fSdf = fSdf;

    return Ret;
}












#endif