// #define THREAD_GROUP_SIZE_X 1 
// #define THREAD_GROUP_SIZE_Y 1
// #define THREAD_GROUP_SIZE_Z 1

struct ModelSdfData
{
    float4x4 coord_matrix;

    float3 sdf_lower;
    float3 sdf_upper;

    uint32_t mesh_sdf_index;
};

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(THREAD_GROUP_SIZE_Z)
    
cbuffer pass_constant : register(b0)
{
    float4x4 voxel_world_matrix;

    uint3 voxel_offset;  
    float gi_max_distance;

    uint32_t mesh_sdf_begin; 
    uint32_t mesh_sdf_end;
};


StructuredBuffer<ModelSdfData> model_sdf_data_buffer : register(t0);
Texture3D<float> model_sdf_textures[] : register(t1);
SamplerState sampler_ : register(s0);

RWTexture3D<float> global_sdf_texture : register(u0);

float calculate_sdf(float min_sdf, uint32_t sdf_index, float3 voxel_world_pos);
float read_sdf(uint3 thread_id, uint3 voxel_id, float min_sdf);

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, THREAD_GROUP_SIZE_Z)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint3 voxel_id = voxel_offset + thread_id;
    float3 voxel_world_pos = mul(float4(voxel_id, 1.0f), voxel_world_matrix).xyz;

    float min_sdf = gi_max_distance;

    for (uint32_t ix = mesh_sdf_begin; ix < mesh_sdf_end; ++ix)
    {
        min_sdf = calculate_sdf(min_sdf, ix, voxel_world_pos);
    }

    global_sdf_texture[voxel_id] = min_sdf;
}

float calculate_sdf(float min_sdf, uint32_t sdf_index, float3 voxel_world_pos)
{
    ModelSdfData sdf_data = model_sdf_data_buffer[sdf_index];

    float3 world_pos_clamped = clamp(voxel_world_pos, sdf_data.sdf_lower, sdf_data.sdf_upper);

    float distance_to_sdf = length(voxel_world_pos - world_pos_clamped);
    // 到 sdf 包围盒的距离已经大于当前最小距离.
    if (min_sdf <= distance_to_sdf) return min_sdf;

    float3 uvw = mul(float4(world_pos_clamped, 1.0f), sdf_data.coord_matrix).xyz;
    uvw.y = 1.0f - uvw.y;

    float sdf = model_sdf_textures[sdf_data.mesh_sdf_index].SampleLevel(sampler_, uvw, 0);
    // Voxel 在 MeshSdf 内.
    if (distance_to_sdf < 0.001f) return min(sdf, min_sdf);

    // 精度非常低.
    return min(min_sdf, distance_to_sdf + sdf);
}

#endif