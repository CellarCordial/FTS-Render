// #define NANITE 1

#include "../common/gbuffer.hlsl"

cbuffer pass_constants : register(b0)
{
    float4x4 view_proj;
    uint page_size;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 uv;
};

StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t0);
StructuredBuffer<uint2> vt_shadow_visible_cluster_buffer : register(t1);
StructuredBuffer<MeshCluster> cluster_buffer : register(t2);
StructuredBuffer<Vertex> cluster_vertex_buffer : register(t3);

#if NANITE
StructuredBuffer<uint> cluster_triangle_buffer : register(t4);
#endif

struct VertexOutput
{
    float4 sv_position : SV_Position;
    uint2 page_id : PAGE_ID;
};

static uint quad_triangle_index[6] = { 0, 1, 2, 2, 3, 0 };

VertexOutput main(uint instance_id: SV_InstanceID, uint vertex_index : SV_VertexID)
{
    uint2 visible_info = vt_shadow_visible_cluster_buffer[instance_id];

    VertexOutput output;

    uint cluster_index = visible_info.x;
    MeshCluster cluster = cluster_buffer[cluster_index];
    uint triangle_index = vertex_index / 3;

#if NANITE
    if (triangle_index >= cluster.triangle_count)
    {
        output.sv_position = float4(0.0, 0.0, 0.0, 0.0) / 0.0;
        return output;
    }
#endif

#if NANITE
    // 因为一个 cluster 最多有 MeshCluster::cluster_size 个顶点 (默认 128个), 小于 255, 故 index 占用 8 个字节.
    uint packed_triangle = cluster_triangle_buffer[cluster.triangle_offset + triangle_index];
    uint index_id = (packed_triangle >> (vertex_index % 3 * 8)) & 255;
#else
    uint index_id = (vertex_index / 6) * 4 + quad_triangle_index[vertex_index % 6];
#endif

    Vertex vertex = cluster_vertex_buffer[cluster.vertex_offset + index_id];

    float4 world_pos = mul(float4(vertex.position, 1.0f), geometry_constant_buffer[cluster.geometry_id].world_matrix);

    output.sv_position = mul(world_pos, view_proj);
    output.page_id = uint2(visible_info.y >> 16, visible_info.y & 0xffff);
    return output;
}
