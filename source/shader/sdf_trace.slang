#ifndef SHADER_SDF_COMMON_SLANG
#define SHADER_SDF_COMMON_SLANG

#include "intersect.slang"

struct GlobalSdfData
{
    float scene_grid_size;
    float3 scene_grid_origin;

    uint max_trace_steps;
    float abs_threshold;
    float default_march;
    float pad;
};

struct SdfHitData
{
    uint step_count;
    float step_size;
    float sdf;
};

SdfHitData trace_global_sdf(float3 o, float3 d, GlobalSdfData sdf_data, Texture3D<float> global_sdf, SamplerState sampler)
{
    SdfHitData ret;

    float3 p = o;
    float init_step;
    float3 scene_grid_end = sdf_data.scene_grid_origin + sdf_data.scene_grid_size;
    if (any(abs(p) > sdf_data.scene_grid_size * 0.5f) && IntersectRayBoxInside(p, d, sdf_data.scene_grid_origin, scene_grid_end, init_step))
    {
        p += d * init_step;
    }

    uint ix = 0;
    float sdf = 0.0f;
    for (; ix < sdf_data.max_trace_steps; ++ix)
    {
        float3 uvw = (p - sdf_data.scene_grid_origin) / sdf_data.scene_grid_size;
        if (any(saturate(uvw) != uvw)) { p = o; break; }

        sdf = global_sdf.Sample(sampler, uvw);

        // 若发现为空 chunk, 则加速前进.
        float Udf = abs(sdf) < 0.00001f ? sdf_data.default_march : sdf;
        if (abs(Udf) <= sdf_data.abs_threshold) break;

        p += Udf * d;
    }

    ret.step_size = length(p - o) / float(sdf_data.max_trace_steps - 1);
    ret.step_count = ix;
    ret.sdf = sdf;

    return ret;
}












#endif