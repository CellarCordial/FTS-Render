#include "../../ray_tracing_helper/restir_helper.slang"
#include "../../ray_tracing_helper/shadow_helper.slang"
#include "../../voxel_irradiance_helper.slang"

using namespace voxel_irradiance;

ByteAddressBuffer ircache_meta_buffer : register(t0);
StructuredBuffer<uint> ircache_life_buffer : register(t1);
RaytracingAccelerationStructure accel_struct : register(t2);
StructuredBuffer<uint> ircache_entry_indirection_buffer : register(t3);

[shader("raygeneration")]
void ray_generation_shader()
{
    uint ray_index = DispatchRaysIndex().x;
    uint total_allocate_num = ircache_meta_buffer.Load(IRCACHE_META_TRACING_ALLOC_COUNT);
    if (ray_index >= total_allocate_num * IRCACHE_VALIDATION_SAMPLES_PER_FRAME) return;

    uint entry_index = ircache_entry_indirection_buffer[ray_index / IRCACHE_VALIDATION_SAMPLES_PER_FRAME];
    uint sample_index = ray_index % IRCACHE_VALIDATION_SAMPLES_PER_FRAME;
    uint age = ircache_life_buffer[entry_index * IRCACHE_VALIDATION_SAMPLES_PER_FRAME];
}


