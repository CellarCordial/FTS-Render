#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1


cbuffer pass_constants : register(b0)
{
    float4x4 shadow_view_proj;

    float3 camera_position;
    uint32_t virtual_shadow_resolution;
    
    uint32_t virtual_shadow_page_size;
    uint32_t client_width;
};

Texture2D<float4> world_position_view_depth : register(t0);

RWStructuredBuffer<uint2> virtual_shadow_page_buffer : register(u0);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    uint2 pixel_id = thread_id.xy;
    float3 world_pos = world_position_view_depth[pixel_id].xyz;
    float4 shadow_view_proj_pos = mul(float4(world_pos, 1.0f), shadow_view_proj);
    shadow_view_proj_pos.xyz = shadow_view_proj_pos.xyz / shadow_view_proj_pos.z;

    bool in_clip = shadow_view_proj_pos.w > 0.0f &&
                   shadow_view_proj_pos.x >= -1.0f && shadow_view_proj_pos.x <= 1.0f &&
                   shadow_view_proj_pos.y >= -1.0f && shadow_view_proj_pos.y <= 1.0f &&
                   shadow_view_proj_pos.z >= 0.0f && shadow_view_proj_pos.z <= 1.0f;

    if (!in_clip) return;

    float2 uv = shadow_view_proj_pos.xy * float2(0.5f, -0.5f) + 0.5f;
    uint2 shadow_page_id = (uint2)(uv * virtual_shadow_resolution) / virtual_shadow_page_size;

    virtual_shadow_page_buffer[pixel_id.x + pixel_id.y * client_width] = shadow_page_id;
}

#endif