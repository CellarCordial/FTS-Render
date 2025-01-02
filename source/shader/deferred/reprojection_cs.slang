// #define THREAD_GROUP_SIZE_X 1
// #define THREAD_GROUP_SIZE_Y 1

cbuffer pass_constants : register(b0)
{
    float4x4 view_matrix;
    float4x4 proj_matrix;
};

Texture2D<float3> view_space_velocity_texture : register(t0);
Texture2D<float3> world_space_position_texture : register(t1);

RWTexture2D<float2> reprojection_texture : register(u0);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void compute_shader(uint3 thread_id : SV_DispatchThreadID)
{
    uint32_t width, height;
    reprojection_texture.GetDimensions(width, height);

    if (thread_id.x >= width || thread_id.y >= height) return;

    float2 uv = float2(
        (thread_id.x + 0.5f) / width,
        (thread_id.y + 0.5f) / height
    );

    float3 view_space_position = mul(float4(world_space_position_texture[thread_id.xy], 1.0f), view_matrix).xyz;
    float3 prev_view_space_position = view_space_position + view_space_velocity_texture[thread_id.xy];
    float4 prev_clip_space_position = mul(float4(prev_view_space_position, 1.0f), proj_matrix);
    float2 prev_uv = (prev_clip_space_position.xy / prev_clip_space_position.w) * float2(0.5f, -0.5f) + 0.5f;

    // 确保 uv 坐标在纹理精度 R16G16_SNORM 的范围内.
    float2 uv_offset = floor((prev_uv - uv) * 32767.0 + 0.5) / 32767.0;

    reprojection_texture[thread_id.xy] = uv_offset;
}


#endif