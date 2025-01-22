

#include "../common/shadow_helper.slang"

[shader("miss")]
void miss_shader(inout ShadowRayPayload payload: SV_RayPayload)
{
    payload.shadowed = 0;
}