// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1

cbuffer pass_constant : register(b0)
{
    int show_type;
    uint2 client_resolution;
};

RWTexture2D<float4> final_texture : register(u0);
Texture2D world_position_view_depth_texture : register(t0);
Texture2D world_space_normal_texture : register(t1);
Texture2D base_color_texture : register(t2);
Texture2D pbr_texture : register(t3);
Texture2D emissive_texture : register(t4);


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    if (any(thread_id.xy >= client_resolution)) return;

    uint2 pixel_id = thread_id.xy;
    switch (show_type)
    {
    case 0: break;
    case 1: final_texture[pixel_id] = float4(world_position_view_depth_texture[pixel_id].xyz, 1.0f); break;
    case 2: final_texture[pixel_id] = float4(world_position_view_depth_texture[pixel_id].w, 0.0f, 0.0f, 1.0f); break;
    case 3: final_texture[pixel_id] = world_space_normal_texture[pixel_id]; break;
    case 4: final_texture[pixel_id] = base_color_texture[pixel_id]; break;
    case 5: final_texture[pixel_id] = float4(pbr_texture[pixel_id].x, 0.0f, 0.0f, 1.0f); break;
    case 6: final_texture[pixel_id] = float4(pbr_texture[pixel_id].y, 0.0f, 0.0f, 1.0f); break;
    case 7: final_texture[pixel_id] = float4(pbr_texture[pixel_id].z, 0.0f, 0.0f, 1.0f); break;
    case 8: final_texture[pixel_id] = emissive_texture[pixel_id]; break;
    // case 9: color = float4(shadow_map_texture[pixel_id], 0.0f, 0.0f, 1.0f); break;
    };
}

#endif