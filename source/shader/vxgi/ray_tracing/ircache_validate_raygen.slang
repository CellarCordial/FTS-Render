#include "../../restir_helper.slang"
#include "../../shadow_helper.slang"
#include "../../vxgi_helper.slang"

using namespace voxel_irradiance;

cbuffer pass_constants : register(b0)
{
    uint32_t frame_index;
};

ByteAddressBuffer ircache_meta_buffer : register(t0);
StructuredBuffer<uint32_t> ircache_life_buffer : register(t1);
RaytracingAccelerationStructure accel_struct : register(t2);
StructuredBuffer<uint32_t> ircache_entry_indirection_buffer : register(t3);
StructuredBuffer<CompressedVertex> ircache_spatial_buffer : register(t4);

RWStructuredBuffer<float4> ircache_auxiliary_buffer : register(u0);

[shader("raygeneration")]
void ray_generation_shader()
{
    uint32_t ray_index = DispatchRaysIndex().x;
    uint32_t total_allocate_num = ircache_meta_buffer.Load(IRCACHE_META_TRACING_ALLOC_COUNT);
    if (ray_index >= total_allocate_num * IRCACHE_VALIDATION_SAMPLES_PER_FRAME) return;

    uint32_t entry_index = ircache_entry_indirection_buffer[ray_index / IRCACHE_VALIDATION_SAMPLES_PER_FRAME];
    uint32_t sample_index = ray_index % IRCACHE_VALIDATION_SAMPLES_PER_FRAME;
    uint32_t age = ircache_life_buffer[entry_index * IRCACHE_VALIDATION_SAMPLES_PER_FRAME];

    Vertex vertex = ircache_spatial_buffer[entry_index].decompress();

    brdf::DiffuseReflection diffuse_reflection = brdf::DiffuseReflection(float3(1.0f, 1.0f, 1.0f));
    SampleParameter sample_param = SampleParameter(IRCACHE_VALIDATION_SAMPLES_PER_FRAME, entry_index, sample_index, frame_index);

    uint32_t octahedral_pixel_index = sample_param.octahedral_pixel_index();
    uint32_t output_index = entry_index * IRCACHE_AUXILIARY_STRIDE + octahedral_pixel_index;

    float invalid = 0.0f;
    restir::Reservoir reservoir = restir::Reservoir(asuint(ircache_auxiliary_buffer[output_index].xy));

    if (reservoir._M > 0.0f)
    {
        
    }

}


