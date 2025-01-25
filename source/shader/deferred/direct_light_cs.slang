#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../common/light.slang"

cbuffer pass_constants : register(b0)
{
    uint32_t point_light_count;
    uint32_t spot_light_count;
};

StructuredBuffer<PointLight> point_light_buffer : register(t0);
StructuredBuffer<PointLight> spot_light_buffer : register(t1);
StructuredBuffer<uint32_t> light_index_buffer : register(t2);
StructuredBuffer<uint2> light_cluster_buffer : register(t3);

RWTexture2D<float4> direct_light_texture : register(u0);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{

}


#endif