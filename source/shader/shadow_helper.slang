#ifndef SHADER_RAY_TRACING_SHADOW_HELPER_SLANG
#define SHADER_RAY_TRACING_SHADOW_HELPER_SLANG

struct ShadowRayPayload
{
    bool shadowed = true;
};

bool ray_tracing_if_shadowed(RaytracingAccelerationStructure accel_struct, RayDesc ray)
{
    ShadowRayPayload payload;
    TraceRay(
        accel_struct,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
        0xff, 
        0, 
        0, 
        1, 
        ray, 
        payload
    );

    return payload.shadowed;
}

#endif 