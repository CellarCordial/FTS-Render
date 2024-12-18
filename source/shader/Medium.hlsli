#ifndef SHADER_MEDIUM_HLSLI
#define SHADER_MEDIUM_HLSLI
#include "Constants.hlsli"


struct FAtmosphereProperties
{
    float3 RayleighScatter;
    float fRayleighDensity;

    float fMieScatter;
    float fMieDensity;
    float fMieAbsorb;
    float fMieAsymmetry;

    float3 OzoneAbsorb;
    float fOzoneCenterHeight;

    float fOzoneThickness;
    float fPlanetRadius;
    float fAtmosphereRadius;
    float PAD;
};

// fHeight 为海拔高度, 即离地高度.
float3 GetTransmittance(FAtmosphereProperties AP, float fHeight)
{
    // 近似后, Rayleigh 中, 衰减率只有散射, Mie 中则包括吸收和散射.
    float3 Rayleigh = AP.RayleighScatter * exp(-fHeight / AP.fRayleighDensity);
    float fMie = (AP.fMieScatter + AP.fMieAbsorb) * exp(-fHeight / AP.fMieDensity);
    
    // 取决于海拔和大气层高度的差值与大气层厚度的比例.
    float fOzoneDensity = max(0.0f, 1.0f - 0.5f * abs((AP.fOzoneCenterHeight - fHeight) / AP.fOzoneThickness));
    float3 Ozone = AP.OzoneAbsorb * fOzoneDensity;

    return Rayleigh + fMie + Ozone;
}

void GetScatterTransmittance(FAtmosphereProperties AP, float fHeight, out float3 Scatter, out float3 Transmittance)
{
    float3 Rayleigh = AP.RayleighScatter * exp(-fHeight / AP.fRayleighDensity);
    float fMieS = (AP.fMieScatter) * exp(-fHeight / AP.fMieDensity);
    float fMieT = (AP.fMieScatter + AP.fMieAbsorb) * exp(-fHeight / AP.fMieDensity);
    
    float fOzoneDensity = max(0.0f, 1.0f - 0.5f * (abs(AP.fOzoneCenterHeight - fHeight) / AP.fOzoneThickness));
    float3 Ozone = AP.OzoneAbsorb * fOzoneDensity;

    Scatter = Rayleigh + fMieS;
    Transmittance = Rayleigh + fMieT + Ozone;
}

float2 GetTransmittanceUV(FAtmosphereProperties AP, float fHeight, float fSunTheta)
{
    float u = fHeight / (AP.fAtmosphereRadius - AP.fPlanetRadius);
    float v = 0.5 + 0.5 * sin(fSunTheta);
    return float2(u, v);
}


float3 EstimatePhaseFunc(FAtmosphereProperties AP, float fHeight, float fTheta)
{
    float3 Rayleigh = AP.RayleighScatter * exp(-fHeight / AP.fRayleighDensity);
    float fMie = AP.fMieScatter * exp(-fHeight / AP.fMieDensity);
    float3 Scatter = Rayleigh + fMie;

    float fMieAsymmetry2 = AP.fMieAsymmetry * AP.fMieAsymmetry;
    float fTheta2 = fTheta *fTheta;

    float fRayleighPhase = (3.0f / (16.0f * PI)) * (1.0f + fTheta2);

    float fTmp = 1.0f + fMieAsymmetry2 - 2.0f * AP.fMieAsymmetry * fTheta;
    float fMiePhase = (3.0f / (8.0f * PI)) * ((1.0f - fMieAsymmetry2) * (1.0f + fTheta2) / ((2.0f + fMieAsymmetry2) * fTmp * sqrt(fTmp)));

    float3 Ret;
    Ret.x = Scatter.x > 0.0f ? (fRayleighPhase * Rayleigh.x + fMiePhase * fMie) / Scatter.x : 0.0f;
    Ret.y = Scatter.y > 0.0f ? (fRayleighPhase * Rayleigh.y + fMiePhase * fMie) / Scatter.y : 0.0f;
    Ret.z = Scatter.z > 0.0f ? (fRayleighPhase * Rayleigh.z + fMiePhase * fMie) / Scatter.z : 0.0f;
    return Ret;
}







#endif