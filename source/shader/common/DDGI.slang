#ifndef SHADER_DDGI_COMMON_SLANG
#define SHADER_DDGI_COMMON_SLANG

#include "math_helper.slang"
#include "octahedral.slang"

struct DDGIVolumeData
{
    float3 origin_position;
    float probe_interval_size;
    
    uint3 probe_count;
    uint32_t ray_count;

    uint2 irradiance_texture_resolution;
    uint2 depth_texture_resolution;

    uint32_t single_irradiance_texture_size;
    uint32_t single_depth_texture_size;
    float normal_bias;
};

float2 get_probe_texture_uv(
    float3 unit_probe_to_pixel,
    uint32_t probe_index,
    uint2 probe_textures_resolution, 
    uint32_t single_probe_texture_size
)
{
    float2 dst_texture_oct_uv = (unit_vector_to_octahedron(unit_probe_to_pixel) + 1.0f) * 0.5f;
    float2 uv_offset = (dst_texture_oct_uv * single_probe_texture_size) / (float2)probe_textures_resolution;

    uint32_t single_probe_texture_size_with_border = single_probe_texture_size + 2;
    uint32_t row_probes_count = probe_textures_resolution.x / single_probe_texture_size_with_border;

    uint2 dst_texture_start_pos = uint2(
        (probe_index % row_probes_count) * single_probe_texture_size_with_border,
        (probe_index / row_probes_count) * single_probe_texture_size_with_border
    );

    float2 uv_start = (dst_texture_start_pos + 1) / (float2)probe_textures_resolution;

    return uv_start + uv_offset;
}

float3 sample_probe_irradiance(
    DDGIVolumeData volume_data, 
    float3 pixel_position, 
    float3 pixel_normal, 
    float3 pixel_to_camera, 
    Texture2D<float3> irradiance_texture, 
    Texture2D<float2> depth_texture,
    SamplerState sampler_
)
{
    uint3 base_probe_id = (uint3)(clamp(
        (pixel_position - volume_data.origin_position) / volume_data.probe_interval_size, 
        uint3(0, 0, 0), 
        volume_data.probe_count - uint3(1, 1, 1)
    ));

    float3 irradiance_sum = 0.0f;
    float wieght_sum = 0.0f;

    for (uint32_t ix = 0; ix < 8; ++ix)
    {
        uint3 offset = uint3(ix, ix >> 1, ix >> 2) & uint3(1, 1, 1);
        uint3 probe_id = (uint3)(clamp(
            base_probe_id + offset, 
            uint3(0, 0, 0), 
            volume_data.probe_count - uint3(1, 1, 1)
        ));
        float3 probe_pos = volume_data.origin_position + volume_data.probe_interval_size * probe_id;

        float weight = 1.0f;

        // 方向权重.
        float3 pixel_to_probe = normalize(probe_pos - pixel_position);
        float theta = (dot(pixel_normal, pixel_to_probe) + 1.0f) * 0.5f;
        weight *= theta * theta + 0.2f;

        // 切比雪夫权重.
        uint32_t probe_index = 
            probe_id.z * volume_data.probe_count.y * volume_data.probe_count.x +
            probe_id.y * volume_data.probe_count.x +
            probe_id.x;

        float3 bias = (pixel_normal * 0.2f + pixel_to_camera * 0.8f) * volume_data.normal_bias;
        float3 probe_to_pixel_with_bias = normalize(pixel_position + bias - probe_pos);
        float2 depth_uv = get_probe_texture_uv(
            probe_to_pixel_with_bias,
            probe_index,
            volume_data.depth_texture_resolution, 
            volume_data.single_depth_texture_size
        );

        float2 fMu = depth_texture.SampleLevel(sampler_, depth_uv, 0.0f);
        float fSigma = abs(fMu.x * fMu.x - fMu.y);

        float fProbeToPixelLength = length(probe_to_pixel_with_bias);
        float fTemp = fProbeToPixelLength - fMu.x;

        float fChebyshev = fSigma / (fSigma + fTemp * fTemp);
        fChebyshev = max(fChebyshev * fChebyshev * fChebyshev, 0.0f);

        weight *= fProbeToPixelLength <= fMu.x ? 1.0f : fChebyshev;


        // 权重压缩.
        float fCrushThreshold = 0.2f;
        if (weight < fCrushThreshold) weight *= weight * weight * (1.0f / fCrushThreshold * fCrushThreshold); 


        // 三线性插值权重.
        float3 BaseProbePos = volume_data.origin_position + volume_data.probe_interval_size * float3(base_probe_id);
        float3 fInterpolation = clamp(
            (pixel_position - BaseProbePos) / volume_data.probe_interval_size, 
            float3(0.0f, 0.0f, 0.0f), 
            float3(1.0f, 1.0f, 1.0f)
        );
        float3 Trilinear = max(0.001f, lerp(1.0f - fInterpolation, fInterpolation, offset));
        weight *= Trilinear.x * Trilinear.y * Trilinear.z;


        float3 IrradianceDir = normalize(pixel_normal);
        float2 IrradianceUV = get_probe_texture_uv(
            IrradianceDir,
            probe_index,
            volume_data.irradiance_texture_resolution, 
            volume_data.single_irradiance_texture_size
        );
        float3 Irradiance = irradiance_texture.SampleLevel(sampler_, IrradianceUV, 0.0f);
        Irradiance = pow(Irradiance, float3(1.0f / 2.2f));


        irradiance_sum += Irradiance * weight;
        wieght_sum += weight;
    }

    float3 Irradiance = irradiance_sum / wieght_sum;
    return 2.0f * PI * Irradiance * Irradiance;
}













#endif