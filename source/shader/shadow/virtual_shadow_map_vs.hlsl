#include "../common/gbuffer.hlsl"
#include "../common/octahedral.hlsl"


cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;
    float4x4 view_matrix;
    uint2 tile_size_in_pixel;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 uv;
};

struct ShadowVisibleInfo
{
    uint cluster_id;
    uint2 tile_id;
};

StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t0);
StructuredBuffer<ShadowVisibleInfo> virtual_shadow_visible_cluster_id_buffer : register(t1);
StructuredBuffer<MeshCluster> cluster_buffer : register(t2);
StructuredBuffer<Vertex> cluster_vertex_buffer : register(t3);
StructuredBuffer<uint> cluster_triangle_buffer : register(t4);

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float3 view_space_position : VIEW_POSITION;
    uint2 tile_id : TILE_ID;
};


VertexOutput main(uint instance_id: SV_InstanceID, uint vertex_index : SV_VertexID)
{
    ShadowVisibleInfo visible_info = virtual_shadow_visible_cluster_id_buffer[instance_id];

    VertexOutput output;

    uint cluster_index = visible_info.cluster_id;
    MeshCluster cluster = cluster_buffer[cluster_index];
    uint triangle_index = vertex_index / 3;
    if (triangle_index >= cluster.triangle_count)
    {
        output.sv_position = float4(0.0, 0.0, 0.0, 0.0) / 0.0;
        return output;
    }

    uint packed_triangle = cluster_triangle_buffer[cluster.triangle_offset + triangle_index];
    uint index_id = (packed_triangle >> (vertex_index % 3 * 8)) & 255;
    Vertex vertex = cluster_vertex_buffer[cluster.vertex_offset + index_id];

    float4 world_pos = mul(float4(vertex.position, 1.0f), geometry_constant_buffer[cluster.geometry_id].world_matrix);

    output.sv_position = mul(world_pos, view_proj);
    output.view_space_position = mul(world_pos, view_matrix).xyz;
    output.tile_id = visible_info.tile_id;
    return output;
}
