
cbuffer gPassConstants : register(b0)
{
    float4x4 InvViewMatrix;
};

Texture2D<float> gDepthTexture : register(t0);
Texture2D<float> gNormalVTexture : register(t1);
Texture2D<float> gPositionVTexture : register(t2);

RWTexture2D<float4> gOutputShadowMaskTexture : register(u0);


[shader("raygeneration")]
void RayGeneration()
{
    uint2 RayID = DispatchRaysIndex().xy;
    float2 UV = (RayID + 0.5f) / DispatchRaysDimensions().xy;

    if (gDepthTexture[RayID] == 0.0f)
    {
        gOutputShadowMaskTexture[RayID] = 1;
        return;
    }

    float3 PositionV = gPositionVTexture[RayID];
    float3 PositionW = mul(float4(PositionV, 1.0f), InvViewMatrix).xyz;
    float3 NormalV = gNormalVTexture[RayID];
    float3 NormalW = mul(float4(NormalV, 1.0f), InvViewMatrix).xyz;

    float3 BiasStep = normalize(NormalW) * (length(PositionW) - PositionV.z) * 1e-5;
    float3 RayOrigin = PositionW + BiasStep;

    bool bShadowed = true;

    gOutputShadowMaskTexture[RayID] = bShadowed ? float4(0.0f, 0.0f, 0.0f, 0.0f) : float4(1.0f, 1.0f, 1.0f, 1.0f);
}

