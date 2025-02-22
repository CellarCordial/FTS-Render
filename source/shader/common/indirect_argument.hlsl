#ifndef SHADER_INDIRECT_ARGUMENT_HLSL
#define SHADER_INDIRECT_ARGUMENT_HLSL

struct DrawIndirectArguments
{
    uint vertex_count;
    uint instance_count;
    uint start_vertex_location;
    uint start_instance_location;
};

struct DrawIndexedIndirectArguments
{
    uint index_count;
    uint instance_count;
    uint start_index_location;
    uint start_vertex_location;
    uint start_instance_location;
};

#endif