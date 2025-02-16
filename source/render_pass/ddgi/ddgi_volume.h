#ifndef RENDER_PASS_DDGI_VOLUMN_H
#define RENDER_PASS_DDGI_VOLUMN_H

#include "../../core/math/vector.h"

namespace fantasy 
{
    struct DDGIVolumeDataGpu
    {
        float3 origin_position;
        float probe_interval_size = 0.0f;
        
        uint3 probe_count;
        uint32_t ray_count = 0;

        uint2 volume_texture_resolution;
        uint32_t single_volume_texture_size = 0;
        float normal_bias = 0.0f;
    };
}






#endif