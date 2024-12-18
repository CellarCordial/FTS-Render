#ifndef SHADER_DDGI_COMMON_HLSLI
#define SHADER_DDGI_COMMON_HLSLI

#include "Constants.hlsli"
#include "octahedral.hlsli"

struct FDDGIVolumeData
{
    float3 OriginPos;
    float fProbeIntervalSize;
    
    uint3 ProbesNum;
    uint dwRaysNum;

    uint dwIrradianceTexturesWidth;
    uint dwIrradianceTexturesHeight;
    uint dwDepthTexturesWidth;
    uint dwDepthTexturesHeight;

    uint dwSingleIrradianceTextureSize;
    uint dwSingleDepthTextureSize;
    float fNormalBias;
    float fProbeGamma;
};

float2 GetProbeTextureUV(
    float3 UnitProbeToPixel, 
    uint dwProbeIndex, 
    uint dwProbeTexturesWidth, 
    uint dwProbeTexturesHeight, 
    uint dwSingleProbeTextureSize
)
{
    float2 DstTextureOctUV = (UnitVectorToOctahedron(UnitProbeToPixel) + 1.0f) * 0.5f;
    float2 UVOffset = (DstTextureOctUV * dwSingleProbeTextureSize) / float2(dwProbeTexturesWidth, dwProbeTexturesHeight);

    uint dwSingleProbeTextureSizeWithBorder = dwSingleProbeTextureSize + 2;
    uint dwRowProbesNum = dwProbeTexturesWidth / dwSingleProbeTextureSizeWithBorder;

    uint2 DstTextureStartPos = uint2(
        (dwProbeIndex % dwRowProbesNum) * dwSingleProbeTextureSizeWithBorder,
        (dwProbeIndex / dwRowProbesNum) * dwSingleProbeTextureSizeWithBorder
    );

    float2 UVStart = (DstTextureStartPos + 1) / float2(dwProbeTexturesWidth, dwProbeTexturesHeight);

    return UVStart + UVOffset;
}

float3 SampleProbeIrradiance(
    FDDGIVolumeData VolumeData, 
    float3 position, 
    float3 normal, 
    float3 PixelToCamera, 
    Texture2D<float3> IrradianceTexture, 
    Texture2D<float2> DepthTexture,
    SamplerState Sampler
)
{
    uint3 BaseProbeID = uint3(clamp(position - VolumeData.OriginPos, uint3(0, 0, 0), uint3(VolumeData.ProbesNum) - uint3(1, 1, 1)));

    float3 IrradianceSum = 0.0f;
    float fWeightSum = 0.0f;

    for (uint ix = 0; ix < 8; ++ix)
    {
        uint3 offset = uint3(ix, ix >> 1, ix >> 2) & uint3(1, 1, 1);
        uint3 ProbeID = uint3(clamp(
            float3(BaseProbeID + offset) - VolumeData.OriginPos, 
            uint3(0, 0, 0), 
            uint3(VolumeData.ProbesNum) - uint3(1, 1, 1)
        ));
        float3 ProbePos = VolumeData.OriginPos + VolumeData.fProbeIntervalSize * float3(ProbeID);

        float fWeight = 1.0f;

        // 方向权重.
        float3 PixelToProbe = normalize(ProbePos - position);
        float theta = (dot(normal, PixelToProbe) + 1.0f) * 0.5f;
        fWeight *= theta * theta + 0.2f;

        // 切比雪夫权重.
        uint dwProbeIndex = ProbeID.x + ProbeID.y * VolumeData.ProbesNum.x + ProbeID.z * VolumeData.ProbesNum.y * VolumeData.ProbesNum.x;

        float3 Bias = (normal + 3.0f * PixelToCamera) * VolumeData.fNormalBias;
        float3 ProbeToPixelWithBias = normalize(position - ProbePos + Bias);
        float2 DepthUV = GetProbeTextureUV(
            ProbeToPixelWithBias, 
            dwProbeIndex,
            VolumeData.dwIrradianceTexturesWidth, 
            VolumeData.dwIrradianceTexturesHeight, 
            VolumeData.dwSingleDepthTextureSize
        );

        float2 fMu = DepthTexture.SampleLevel(Sampler, DepthUV, 0.0f);
        float fSigma = abs(fMu.x * fMu.x - fMu.y);

        float fProbeToPixelLength = length(ProbeToPixelWithBias);
        float fTemp = fProbeToPixelLength - fMu;

        float fChebyshev = fSigma / (fSigma + fTemp * fTemp);
        fChebyshev = max(fChebyshev * fChebyshev * fChebyshev, 0.0f);

        fWeight *= fProbeToPixelLength <= fMu ? 1.0f : fChebyshev;


        // 权重压缩.
        float fCrushThreshold = 0.2f;
        if (fWeight < fCrushThreshold) fWeight *= fWeight * fWeight * (1.0f / fCrushThreshold * fCrushThreshold); 


        // 三线性插值权重.
        float3 BaseProbePos = VolumeData.OriginPos + VolumeData.fProbeIntervalSize * float3(BaseProbeID);
        float fInterpolation = clamp(
            (position - BaseProbePos) / VolumeData.fProbeIntervalSize, 
            float3(0.0f, 0.0f, 0.0f), 
            float3(1.0f, 1.0f, 1.0f)
        );
        float3 Trilinear = max(0.001f, lerp(1.0f - fInterpolation, fInterpolation, offset));
        fWeight *= Trilinear.x * Trilinear.y * Trilinear.z;


        float3 IrradianceDir = normalize(normal);
        float2 IrradianceUV = GetProbeTextureUV(
            IrradianceDir, 
            dwProbeIndex, 
            VolumeData.dwIrradianceTexturesWidth, 
            VolumeData.dwIrradianceTexturesHeight, 
            VolumeData.dwSingleIrradianceTextureSize
        );
        float3 Irradiance = IrradianceTexture.SampleLevel(Sampler, IrradianceUV, 0.0f);
        Irradiance = pow(Irradiance, float3(1.0f / 2.2f));


        IrradianceSum += Irradiance * fWeight;
        fWeightSum += fWeight;
    }

    float3 Irradiance = IrradianceSum / fWeightSum;
    return 2.0f * PI * Irradiance * Irradiance;
}













#endif