#include "../gbuffer.slang"

struct VertexOutput
{
    float3 color : COLOR;
    float4 sv_position : SV_Position;

    float3 world_space_position : WORLD_POSITION;

    float3 view_space_position : VIEW_POSITION;
    float3 prev_view_space_position : PREV_VIEW_POSITION;
    
    float3 world_space_normal : NORMAL;
    float3 world_space_tangent  : TANGENT;
    float2 uv        : TEXCOORD;
};

struct PixelOutput
{
    float4 world_position_view_depth : SV_TARGET0;
    float4 view_space_velocity : SV_TARGET1;
};

StructuredBuffer<GeometryConstant> geometry_constant_buffer : register(t0);