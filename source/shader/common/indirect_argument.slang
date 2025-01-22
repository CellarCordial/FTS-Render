#ifndef SHADER_INDIRECT_ARGUMENT_SLANG
#define SHADER_INDIRECT_ARGUMENT_SLANG

struct DrawIndirectArguments
{
    uint32_t vertex_count = 0;
    uint32_t instance_count = 1;
    uint32_t start_vertex_location = 0;
    uint32_t start_instance_location = 0;
};

struct DrawIndexedIndirectArguments
{
    uint32_t index_count = 0;
    uint32_t instance_count = 1;
    uint32_t start_index_location = 0;
    uint32_t start_vertex_location = 0;
    uint32_t start_instance_location = 0;
};

#endif