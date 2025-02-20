// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1

#include "../common/math.hlsl"


cbuffer pass_constants : register(b0)
{
    uint2 client_resolution;
    uint hzb_resolution;
    uint calc_mip_level;
};

SamplerState linear_clamp_sampler : register(s0);
Texture2D<float4> world_position_view_depth_texture : register(t0);
RWTexture2D<float> hierarchical_zbuffer_texture[] : register(u0);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    uint2 uv = thread_id.xy;

    float z0, z1, z2, z3;

    if (calc_mip_level == 0)
    {
        uint2 client_uv = float2(uv) / hzb_resolution * client_resolution;
        z0 = world_position_view_depth_texture.Sample(linear_clamp_sampler, (client_uv + 0.5f) / client_resolution).w;
        z1 = world_position_view_depth_texture.Sample(linear_clamp_sampler, (client_uv + 0.5f + uint2(1, 0)) / client_resolution).w;
        z2 = world_position_view_depth_texture.Sample(linear_clamp_sampler, (client_uv + 0.5f + uint2(1, 1)) / client_resolution).w;
        z3 = world_position_view_depth_texture.Sample(linear_clamp_sampler, (client_uv + 0.5f + uint2(0, 1)) / client_resolution).w;

        hierarchical_zbuffer_texture[calc_mip_level][uv] = min4(z0, z1, z2, z3);
    }
    else
    {
        uv >>= calc_mip_level - 1;
        z0 = hierarchical_zbuffer_texture[calc_mip_level - 1][uv].r;
        z1 = hierarchical_zbuffer_texture[calc_mip_level - 1][uv + uint2(1, 0)].r;
        z2 = hierarchical_zbuffer_texture[calc_mip_level - 1][uv + uint2(1, 1)].r;
        z3 = hierarchical_zbuffer_texture[calc_mip_level - 1][uv + uint2(0, 1)].r;

        hierarchical_zbuffer_texture[calc_mip_level][uv >> 1] = min4(z0, z1, z2, z3);
    }
}

#endif