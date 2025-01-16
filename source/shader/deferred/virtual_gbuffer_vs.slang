#include "../gbuffer.slang"
#include "../octahedral.slang"


cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;

    float4x4 view_matrix;
    float4x4 prev_view_matrix;

    uint32_t view_mode;
    uint32_t mip_level;
    uint32_t vt_page_size;
    uint32_t client_width;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 uv;
};

StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t0);
StructuredBuffer<uint32_t> visible_cluster_id_buffer : register(t1);
StructuredBuffer<MeshCluster> cluster_buffer : register(t2);
StructuredBuffer<Vertex> vertex_buffer : register(t3);
StructuredBuffer<uint32_t> triangle_buffer : register(t4);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    
    float3 color : COLOR;
    float3 world_space_position : WORLD_POSITION;

    float3 view_space_position : VIEW_POSITION;
    float3 prev_view_space_position : PREV_VIEW_POSITION;
    
    float3 world_space_normal : NORMAL;
    float3 world_space_tangent  : TANGENT;
    float2 uv : TEXCOORD;

    uint32_t geometry_id : GEOMETRY_ID;
};

float3 color_hash(uint32_t index)
{
    uint32_t hash_key = murmur_mix(index);

    float3 color = float3(
        hash_key & 255,
        (hash_key >> 8) & 255,
        (hash_key >> 16) & 255
    );
    return color / 255.0f;
}

[shader("vertex")]
VertexOutput vertex_shader(uint32_t instance_id: SV_InstanceID, uint32_t vertex_index : SV_VertexID)
{
    VertexOutput output;

    uint32_t cluster_index = visible_cluster_id_buffer[instance_id];

    MeshCluster cluster = cluster_buffer[cluster_index];
    uint32_t triangle_index = vertex_index / 3;
    if (triangle_index >= cluster.triangle_count)
    {
        output.sv_position.z = 0 / 0;
        return output;
    }

    switch (view_mode)
    {
    case 0: output.color = color_hash(triangle_index);
    case 1: output.color = color_hash(cluster_index);
    case 2: output.color = color_hash(cluster.group_id);
    case 3: output.color = color_hash(cluster.mip_level);
    };

    uint32_t packed_triangle = triangle_buffer[cluster.triangle_offset + triangle_index];
    uint32_t index_id = (packed_triangle >> (vertex_index % 3 * 8)) & 255;
    Vertex vertex = vertex_buffer[cluster.vertex_offset + index_id];
    
    GeometryConstant geometry = geometry_constant_buffer[cluster.geometry_id];
    float4 world_pos = mul(float4(vertex.position, 1.0f), geometry.world_matrix);

    output.sv_position = mul(world_pos, view_proj);

    output.world_space_position = world_pos.xyz;

    output.view_space_position = mul(world_pos, view_matrix).xyz;
    output.prev_view_space_position = mul(world_pos, prev_view_matrix).xyz;

    output.world_space_normal = normalize(mul(float4(vertex.normal, 1.0f), geometry.inv_trans_world)).xyz;
    output.world_space_tangent = normalize(mul(float4(vertex.tangent, 1.0f), geometry.inv_trans_world)).xyz;
    output.uv = vertex.uv;

    return output;
}

