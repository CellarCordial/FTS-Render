// #define NANITE 1

#include "../common/gbuffer.hlsl"

cbuffer pass_constants : register(b0)
{
    float4x4 reverse_z_view_proj;
    float4x4 view_matrix;

    uint view_mode;
    uint vt_page_size;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 uv;
};

StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t0);
StructuredBuffer<uint> visible_cluster_id_buffer : register(t1);
StructuredBuffer<MeshCluster> mesh_cluster_buffer : register(t2);
StructuredBuffer<Vertex> cluster_vertex_buffer : register(t3);

#if NANITE
StructuredBuffer<uint> cluster_triangle_buffer : register(t4);
#endif

struct VertexOutput
{
    float4 sv_position : SV_Position;
    
    float3 color : COLOR;
    float3 world_space_position : WORLD_POSITION;
    float3 view_space_position : VIEW_POSITION;

    float3 world_space_normal : NORMAL;
    float3 world_space_tangent  : TANGENT;
    float2 uv : TEXCOORD;

    uint geometry_id : GEOMETRY_ID;
};

float3 color_hash(uint index)
{
    uint hash_key = murmur_mix(index);

    float3 color = float3(
        hash_key & 255,
        (hash_key >> 8) & 255,
        (hash_key >> 16) & 255
    );
    return color / 255.0f;
}

static uint quad_triangle_index[6] = { 0, 1, 2, 2, 3, 0 };

VertexOutput main(uint instance_id: SV_InstanceID, uint vertex_index : SV_VertexID)
{
    VertexOutput output;

    uint cluster_index = visible_cluster_id_buffer[instance_id];
    MeshCluster cluster = mesh_cluster_buffer[cluster_index];
    uint triangle_index = vertex_index / 3;

#if NANITE
    if (triangle_index >= cluster.triangle_count)
    {
        output.sv_position = float4(0.0, 0.0, 0.0, 0.0) / 0.0;
        return output;
    }
#endif

    switch (view_mode)
    {
    case 0: output.color = color_hash(triangle_index); break;
    case 1: output.color = color_hash(cluster_index); break;
#if NANITE
    case 2: output.color = color_hash(cluster.group_id); break;
    case 3: output.color = color_hash(cluster.mip_level); break;
#endif
    };

#if NANITE
    // 因为一个 cluster 最多有 MeshCluster::cluster_size 个顶点 (默认 128个), 小于 255, 故 index 占用 8 个字节.
    uint packed_triangle = cluster_triangle_buffer[cluster.triangle_offset + triangle_index];
    uint index_id = (packed_triangle >> (vertex_index % 3 * 8)) & 255;
#else
    uint index_id = (vertex_index / 6) * 4 + quad_triangle_index[vertex_index % 6];
#endif
    
    Vertex vertex = cluster_vertex_buffer[cluster.vertex_offset + index_id];
    
    GeometryConstant geometry = geometry_constant_buffer[cluster.geometry_id];

    float4x4 scale = float4x4(
        0.01, 0.0, 0.0, 0.0,
        0.0, -0.01, 0.0, 0.0, 
        0.0, 0.0, -0.01, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
    float4 world_pos = mul(mul(float4(vertex.position, 1.0f), geometry.world_matrix), scale);

    output.sv_position = mul(world_pos, reverse_z_view_proj);

    output.world_space_position = world_pos.xyz;

    output.view_space_position = mul(world_pos, view_matrix).xyz;

    output.world_space_normal = normalize(mul(float4(vertex.normal, 1.0f), geometry.inv_trans_world)).xyz;
    output.world_space_tangent = normalize(mul(float4(vertex.tangent, 1.0f), geometry.inv_trans_world)).xyz;
    output.uv = vertex.uv;
    output.geometry_id = cluster.geometry_id;

    return output;
}

