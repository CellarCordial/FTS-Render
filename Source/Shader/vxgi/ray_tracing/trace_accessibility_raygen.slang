#include "../../vxgi_helper.slang"
#include "../../restir_helper.slang"
#include "../../shadow_helper.slang"

using namespace voxel_irradiance;

ByteAddressBuffer ircache_meta_buffer : register(t0);
StructuredBuffer<uint> ircache_life_buffer : register(t1);
RaytracingAccelerationStructure accel_struct : register(t2);
StructuredBuffer<uint> ircache_entry_indirection_buffer : register(t3);
StructuredBuffer<CompressedVertex> ircache_spatial_buffer : register(t4);

RWStructuredBuffer<float4> ircache_auxiliary_buffer : register(u0);

[shader("raygeneration")]
void ray_generation_shader()
{
    uint ray_index = DispatchRaysIndex().x;
    uint total_allocate_num = ircache_meta_buffer.Load(IRCACHE_META_TRACING_ALLOC_COUNT);
    if (ray_index >= total_allocate_num * IRCACHE_OCTAHEDRAL_DIMS2) return;

    uint entry_index = ircache_entry_indirection_buffer[ray_index / IRCACHE_OCTAHEDRAL_DIMS2];
    uint age = ircache_life_buffer[entry_index];
    if (!is_ircache_entry_life_valid(age)) return;

    uint octahedral_index = ray_index % IRCACHE_OCTAHEDRAL_DIMS2;
    uint output_index = entry_index * IRCACHE_AUXILIARY_STRIDE + octahedral_index;

    uint2 auxiliary = asuint(ircache_auxiliary_buffer[output_index].xy);
    restir::Reservoir reservoir = restir::Reservoir(auxiliary);

    Vertex vertex = ircache_spatial_buffer[entry_index].decompress();
    Vertex prev_vertex = CompressedVertex(ircache_auxiliary_buffer[output_index + IRCACHE_OCTAHEDRAL_DIMS2 * 2]).decompress();

    RayDesc ray;
    ray.Origin = vertex.position;
    ray.direction = prev_vertex.position - vertex.position;
    ray.TMin = 0.001;
    ray.TMax = 0.999;

    if (ray_tracing_if_shadowed(accel_struct, ray))
    {
        reservoir._M *= 0.8f;
        ircache_auxiliary_buffer[output_index].xy = asfloat(reservoir.compress());
    }
}