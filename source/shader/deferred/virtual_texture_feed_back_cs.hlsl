// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1
// #define VT_FEED_BACK_SCALE_FACTOR 1

#include "../common/gbuffer.hlsl"


cbuffer pass_constants : register(b0)
{
    uint client_width;
    uint vt_page_size;
};

StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t0);
Texture2D<float4> geometry_uv_miplevel_id_texture : register(t1);

RWTexture2D<uint2> vt_page_uv_texture : register(u0);
RWStructuredBuffer<uint2> vt_feed_back_buffer : register(u1);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(VT_FEED_BACK_SCALE_FACTOR)


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{    
    uint2 pixel_id = thread_id.xy;

    float4 geometry_uv_miplevel_id = geometry_uv_miplevel_id_texture[pixel_id];
    float2 uv = geometry_uv_miplevel_id.xy;
    uint mip_level = asuint(geometry_uv_miplevel_id.z);
    uint geometry_id = asuint(geometry_uv_miplevel_id.w);

    uint2 feed_back_id = pixel_id / VT_FEED_BACK_SCALE_FACTOR;
    bool feed_back = all(pixel_id == feed_back_id * VT_FEED_BACK_SCALE_FACTOR + 2); 
    uint feed_back_index = feed_back_id.x + feed_back_id.y * (client_width / VT_FEED_BACK_SCALE_FACTOR);

    uint2 page_uv = uint2(INVALID_SIZE_32, INVALID_SIZE_32);
    uint2 feed_back_data = uint2(INVALID_SIZE_32, INVALID_SIZE_32);

    GeometryConstant geometry = geometry_constant_buffer[geometry_id];
    if (all(geometry.texture_resolution != 0))
    {
        uint2 geometry_texture_resolution = max(geometry.texture_resolution >> mip_level, vt_page_size);
        uint2 geometry_texture_pixel_id = uint2(uv * geometry_texture_resolution);

        vt_page_uv_texture[pixel_id] = geometry_texture_pixel_id % vt_page_size;

        if (feed_back)
        {
            uint2 page_id = geometry_texture_pixel_id / vt_page_size;
            uint page_id_mip_level = uint(
                (page_id.x << 20) |
                (page_id.y << 8) |
                (mip_level & 0xff)
            );

            vt_feed_back_buffer[feed_back_index] = uint2(geometry_id, page_id_mip_level);
        }
    }
}

#endif