// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1
// #define COPY_DEPTH 1

#include "../common/math.slang"

[vk_binding(0, 0)]
cbuffer pass_constants : register(b0)
{
    uint2 client_resolution;
    uint32_t hzb_resolution;
    uint32_t last_mip_level;
};

[vk_binding(1, 0)] SamplerState linear_clamp_sampler : register(s0);
[vk_binding(2, 0)] Texture2D<float4> world_position_view_depth_texture : register(t0);
[vk_binding(3, 0)] RWTexture2D<float> hierarchical_zbuffer_texture[] : register(u0);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    uint2 uv = thread_id.xy;

#if COPY_DEPTH
    uv = uv / hzb_resolution * client_resolution;
    float z0 = world_position_view_depth_texture[uv].w;
    float z1 = world_position_view_depth_texture[uv + uint2(1, 0)].w;
    float z2 = world_position_view_depth_texture[uv + uint2(1, 1)].w;
    float z3 = world_position_view_depth_texture[uv + uint2(0, 1)].w;
#else
    float z0 = hierarchical_zbuffer_texture[last_mip_level][uv].r;
    float z1 = hierarchical_zbuffer_texture[last_mip_level][uv + uint2(1, 0)].r;
    float z2 = hierarchical_zbuffer_texture[last_mip_level][uv + uint2(1, 1)].r;
    float z3 = hierarchical_zbuffer_texture[last_mip_level][uv + uint2(0, 1)].r;
#endif

    hierarchical_zbuffer_texture[last_mip_level + 1][uv] = min4(z0, z1, z2, z3);
}

#endif