#ifndef RENDER_PASS_GLOBAL_SDF_INFO_H
#define RENDER_PASS_GLOBAL_SDF_INFO_H

#include "../../core/math/vector.h"

namespace fantasy 
{
    struct GlobalSDFInfo
    {
        float sdf_grid_size = 0.0f;
        float3 sdf_grid_origin; 

        uint32_t max_trace_steps = 1024;
        float abs_threshold = 0.01f;
        float default_march = 0.0f;
    };
}








#endif