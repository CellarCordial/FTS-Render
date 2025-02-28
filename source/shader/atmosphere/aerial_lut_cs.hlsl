// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1

#include "../common/atmosphere_properties.hlsl"
#include "../common/intersect.hlsl"



cbuffer pass_constants : register(b0)
{
    float3 sun_dir;          float sun_theta;
    float3 frustum_A;        float max_distance;
    float3 frustum_B;        int per_slice_march_step_count;
    float3 frustum_C;        float atmos_eye_height;
    float3 frustum_D;        uint enable_multi_scattering;
    float3 camera_position;  uint enable_shadow;
    float world_scale;      float3 pad;
    float4x4 shadow_view_proj;
};


cbuffer atomsphere_properties : register(b1)
{
    AtmosphereProperties AP;
};


Texture2D<float3> multi_scattering_texture : register(t0);
Texture2D<float3> transmittance_texture : register(t1);
Texture2D<float> shadow_map_texture : register(t2);

SamplerState MT_sampler : register(s0);
SamplerState shadow_map_sampler : register(s1);

RWTexture3D<float4> aerial_perspective_lut_texture : register(u0);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)


float RelativeLuminance(float3 c)
{
    return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
}

// TODO: fix shadow map.

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint width, height, depth;
    aerial_perspective_lut_texture.GetDimensions(width, height, depth);

    if (thread_id.x >= width || thread_id.y >= height) return;

    float u = (thread_id.x + 0.5f) / width;
    float v = (thread_id.y + 0.5f) / height;

    float3 ori = float3(0.0f, atmos_eye_height, 0.0f);
    float3 dir = normalize(lerp(
        lerp(frustum_A, frustum_B, u),
        lerp(frustum_C, frustum_D, u),
        v
    ));

    float cos_theta = dot(-sun_dir, dir);

    float distance = 0.0f;
    float3 world_ori = ori + float3(0.0f, AP.planet_radius, 0.0f);
    if (!intersect_ray_sphere(world_ori, dir, AP.planet_radius, distance))
    {
        intersect_ray_sphere(world_ori, dir, AP.atmosphere_radius, distance);
    }

    float fSliceDepth = max_distance / depth;
    float fBegin = 0.0f, fEnd = min(0.5f * fSliceDepth, distance);

    float3 TransmittanceSum = float3(0.0f, 0.0f, 0.0f);
    float3 InScatterSum = float3(0.0f, 0.0f, 0.0f);

    float fRandom = frac(sin(dot(float2(u, v), float2(12.9898f, 78.233f) * 2.0f)) * 43758.5453f);

    for (uint z = 0; z < depth; ++z)
    {
        float dt = (fEnd - fBegin) / per_slice_march_step_count;
        float t = fBegin;
        for (uint ix = 0; ix < per_slice_march_step_count; ++ix)
        {
            float fNextT = t + dt;
            float fMidT = lerp(t, fNextT, fRandom);

            float3 Pos = world_ori + dir * fMidT;
            float height = length(Pos) - AP.planet_radius;

            float3 transmittance;
            float3 InScatter;
            get_scatter_transmittance(AP, height, InScatter, transmittance);

            float3 DeltaTransmittance = dt * transmittance;
            float3 EyeTransmittance = exp(-TransmittanceSum - 0.5f * DeltaTransmittance);
            float2 uv = get_transmittance_uv(AP, height, sun_theta);

            if (!intersect_ray_sphere(Pos, -sun_dir, AP.planet_radius))
            {
                float3 ShadowPos = camera_position + dir * fMidT / world_scale;
                float4 shadow_clip = mul(float4(ShadowPos, 1.0f), shadow_view_proj);
                float2 shadow_ndc = shadow_clip.xy / shadow_clip.w;
                float2 shadow_uv = 0.5f + float2(0.5f, -0.5f) * shadow_ndc;

                bool bInShdow = bool(enable_shadow);
                if (bInShdow && all(saturate(shadow_uv) == shadow_uv))
                {
                    float fRayZ = shadow_clip.z;
                    float fShadowMapZ = shadow_map_texture.SampleLevel(shadow_map_sampler, shadow_uv, 0);
                    bInShdow = fRayZ >= fShadowMapZ;
                }

                if (!bInShdow)
                {
                    float3 Phase = estimate_phase_func(AP, height, cos_theta);
                    float3 SunTransmittance = transmittance_texture.SampleLevel(MT_sampler, uv, 0);

                    InScatterSum += dt * (EyeTransmittance * InScatter * Phase) * SunTransmittance;
                }
            }

            if (bool(enable_multi_scattering))
            {
                float3 MultiScattering = multi_scattering_texture.SampleLevel(MT_sampler, uv, 0);
                InScatterSum += dt * EyeTransmittance * InScatter * MultiScattering;
            }

            TransmittanceSum += DeltaTransmittance;
            t = fNextT;
        }

        aerial_perspective_lut_texture[uint3(thread_id.xy, z)] = float4(InScatterSum, RelativeLuminance(exp(-TransmittanceSum)));

        fBegin = fEnd;
        fEnd = min(fEnd + fSliceDepth, distance);
    }
}






#endif