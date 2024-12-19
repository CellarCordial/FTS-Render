// #define THREAD_GROUP_SIZE_X 1 
// #define THREAD_GROUP_SIZE_Y 1
// #define THREAD_GROUP_SIZE_Z 1


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(THREAD_GROUP_SIZE_Z)
    
cbuffer pass_consstant : register(b0)
{
    float4x4 voxel_world_matrix;

    uint3 voxel_offset;  
    float gi_max_distance;
    
    uint model_sdf_begin; 
    uint model_sdf_end;
};

RWTexture3D<float> global_sdf_texture : register(u0);

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, THREAD_GROUP_SIZE_Z)]
void compute_shader(uint3 thread_id : SV_DispatchThreadID)
{
    uint3 voxel_id = voxel_offset + thread_id;
    global_sdf_texture[uint3(voxel_id.x, voxel_id.y, voxel_id.z)] = 0.0f;
}

#endif
