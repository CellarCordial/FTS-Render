// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1

#include "../common/math.slang"

cbuffer pass_constants : register(b0)
{
    uint2 client_resolution;
    uint32_t hzb_resolution;
    uint32_t last_mip_level;
};

RWTexture2D<float> hierarchical_zbuffer_texture[] : register(t0);
SamplerState linear_clamp_sampler : register(s0);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float2 uv : TEXCOORD;
};

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    uint2 uv = thread_id.xy;
    if (last_mip_level == 0) uv = uv / hzb_resolution * client_resolution;
    else uv *= 2;

    float z0 = hierarchical_zbuffer_texture[last_mip_level][uv].r;
    float z1 = hierarchical_zbuffer_texture[last_mip_level][uv + uint2(1, 0)].r;
    float z2 = hierarchical_zbuffer_texture[last_mip_level][uv + uint2(1, 1)].r;
    float z3 = hierarchical_zbuffer_texture[last_mip_level][uv + uint2(0, 1)].r;

    hierarchical_zbuffer_texture[last_mip_level + 1][uv] = min4(z0, z1, z2, z3);
}

#endif