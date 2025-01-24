#ifndef SHADER_MEDIUM_SLANG
#define SHADER_MEDIUM_SLANG
#include "math.slang"


struct AtmosphereProperties
{
    float3 rayleigh_scatter;
    float rayleigh_density;

    float mie_scatter;
    float mie_density;
    float mie_absorb;
    float mie_asymmetry;

    float3 ozone_absorb;
    float ozone_center_height;

    float ozone_thickness;
    float planet_radius;
    float atmosphere_radius;
    float pad;
};

// height 为海拔高度, 即离地高度.
float3 get_transmittance(AtmosphereProperties AP, float height)
{
    // 近似后, rayleigh 中, 衰减率只有散射, Mie 中则包括吸收和散射.
    float3 rayleigh = AP.rayleigh_scatter * exp(-height / AP.rayleigh_density);
    float mie = (AP.mie_scatter + AP.mie_absorb) * exp(-height / AP.mie_density);
    
    // 取决于海拔和大气层高度的差值与大气层厚度的比例.
    float ozone_density = max(0.0f, 1.0f - 0.5f * abs((AP.ozone_center_height - height) / AP.ozone_thickness));
    float3 ozone = AP.ozone_absorb * ozone_density;

    return rayleigh + mie + ozone;
}

void get_scatter_transmittance(AtmosphereProperties AP, float height, out float3 scatter, out float3 transmittance)
{
    float3 rayleigh = AP.rayleigh_scatter * exp(-height / AP.rayleigh_density);
    float mie_s = (AP.mie_scatter) * exp(-height / AP.mie_density);
    float mie_t = (AP.mie_scatter + AP.mie_absorb) * exp(-height / AP.mie_density);
    
    float ozone_density = max(0.0f, 1.0f - 0.5f * (abs(AP.ozone_center_height - height) / AP.ozone_thickness));
    float3 ozone = AP.ozone_absorb * ozone_density;

    scatter = rayleigh + mie_s;
    transmittance = rayleigh + mie_t + ozone;
}

float2 get_transmittance_uv(AtmosphereProperties AP, float height, float sun_theta)
{
    float u = height / (AP.atmosphere_radius - AP.planet_radius);
    float v = 0.5 + 0.5 * sin(sun_theta);
    return float2(u, v);
}


float3 estimate_phase_func(AtmosphereProperties AP, float height, float theta)
{
    float3 rayleigh = AP.rayleigh_scatter * exp(-height / AP.rayleigh_density);
    float mie = AP.mie_scatter * exp(-height / AP.mie_density);
    float3 scatter = rayleigh + mie;

    float mie_asymmetry2 = AP.mie_asymmetry * AP.mie_asymmetry;
    float theta2 = theta *theta;

    float rayleigh_phase = (3.0f / (16.0f * PI)) * (1.0f + theta2);

    float tmp = 1.0f + mie_asymmetry2 - 2.0f * AP.mie_asymmetry * theta;
    float mie_phase = (3.0f / (8.0f * PI)) * ((1.0f - mie_asymmetry2) * (1.0f + theta2) / ((2.0f + mie_asymmetry2) * tmp * sqrt(tmp)));

    float3 ret;
    ret.x = scatter.x > 0.0f ? (rayleigh_phase * rayleigh.x + mie_phase * mie) / scatter.x : 0.0f;
    ret.y = scatter.y > 0.0f ? (rayleigh_phase * rayleigh.y + mie_phase * mie) / scatter.y : 0.0f;
    ret.z = scatter.z > 0.0f ? (rayleigh_phase * rayleigh.z + mie_phase * mie) / scatter.z : 0.0f;
    return ret;
}







#endif