

#include "../common/shadow_helper.hlsl"


void miss_shader(inout ShadowRayPayload payload: SV_RayPayload)
{
    payload.shadowed = 0;
}