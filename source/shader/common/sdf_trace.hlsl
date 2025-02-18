#ifndef SHADER_SDF_COMMON_SLANG
#define SHADER_SDF_COMMON_SLANG

#include "intersect.hlsl"
#include "math.hlsl"

struct GlobalSDFInfo
{
    float3 sdf_grid_origin;
    float sdf_grid_size;

    uint max_trace_steps;
    float abs_threshold;
    float default_march;
    float pad;
};

struct SDFChunkData
{
    int32_t mesh_index_begin;
    int32_t mesh_index_end;
};

struct SDFHitData
{
    uint step_count;
    float step_size;
    float sdf;

    bool is_hit() { return step_count != INVALID_SIZE_32; }
    bool is_inside() { return step_size <= 0.0f; }
};

SDFHitData trace_global_sdf(float3 o, float3 d, GlobalSDFInfo sdf_data, Texture3D<float> global_sdf, SamplerState sampler)
{
    SDFHitData ret;

    float3 p = o;
    float init_step;

    float sdf_grid_half_size = sdf_data.sdf_grid_size * 0.5f;
    float3 sdf_grid_center = sdf_data.sdf_grid_origin + sdf_grid_half_size;
    float3 sdf_grid_end = sdf_grid_center + sdf_grid_half_size;
    if (
        any(abs(length(p - sdf_grid_center)) > sdf_grid_half_size) && 
        intersect_ray_box_inside(p, d, sdf_data.sdf_grid_origin, sdf_grid_end, init_step)
    )
    {
        p += d * init_step;
    }

    uint step_count = 0;
    float sdf = 0.0f;
    for (; step_count < sdf_data.max_trace_steps; ++step_count)
    {
        float3 uvw = (p - sdf_data.sdf_grid_origin) / sdf_data.sdf_grid_size;
        if (any(saturate(uvw) != uvw)) { step_count = INVALID_SIZE_32; break; }

        sdf = global_sdf.Sample(sampler_, uvw);

        // 若发现为空 chunk, 则加速前进.
        float udf = abs(sdf) < 0.00001f ? sdf_data.default_march : sdf;
        if (abs(udf) <= sdf_data.abs_threshold) break;

        p += udf * d;
    }

    ret.step_size = length(p - o) / float(sdf_data.max_trace_steps - 1);
    ret.step_count = step_count;
    ret.sdf = sdf;

    return ret;
}












#endif