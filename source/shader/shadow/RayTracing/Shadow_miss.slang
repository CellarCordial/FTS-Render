

#include "../../shadow_helper.slang"

[shader("miss")]
void Miss(inout ShadowRayPayload payload: SV_RayPayload)
{
    payload.shadowed = false;
}