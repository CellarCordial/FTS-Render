#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../gbuffer.slang"

cbuffer pass_constants : register(b0)
{
    uint32_t client_height;
    uint32_t camera_fov_y;
};

bool check_lod(float3 view_space_position, float radius, float error);

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void compute_shader(uint3 thread_id: SV_DispatchThreadID)
{
    uint32_t thread_index = thread_id.x;
    
}

bool check_lod(float3 view_space_position, float radius, float error)
{
    float distance = max(length(view_space_position) - radius, 0.0f);
    float theta = radians(camera_fov_y) / client_height;
    return distance * theta >= error;
}

#endif