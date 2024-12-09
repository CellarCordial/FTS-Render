

#include "ShadowHelper.slang"

[shader("miss")]
void Miss(inout FShadowRayPayLoad RayPayLoad: SV_RayPayload)
{
    RayPayLoad.bShadowed = false;
}