#ifndef SHADER_IRCACHE_HELPER_SLANG
#define SHADER_IRCACHE_HELPER_SLANG

#include "octahedral.hlsl"
#include "math.hlsl"
#include "gbuffer.hlsl"
#include "shadow_helper.hlsl"

#define IRCACHE_ENTRY_LIFE_RECYCLE 0x8000000u
#define IRCACHE_ENTRY_LIFE_RECYCLED (IRCACHE_ENTRY_LIFE_RECYCLE + 1u)

#define IRCACHE_META_ENTRY_COUNT (2 * sizeof(uint))
#define IRCACHE_META_ALLOC_COUNT (3 * sizeof(uint))
#define IRCACHE_META_TRACING_ALLOC_COUNT 0

namespace voxel_irradiance
{
    static const uint IRCACHE_ENTRY_LIFE_PER_RANK = 4;
    static const uint IRCACHE_ENTRY_RANK_COUNT = 3;
    
    static const uint IRCACHE_ENTRY_META_OCCUPIED = 1u;
    static const uint IRCACHE_ENTRY_META_JUST_ALLOCATED = 2u;

    static const uint IRCACHE_OCTAHEDRAL_DIMS = 4; // octahedral texture size.
    static const uint IRCACHE_OCTAHEDRAL_DIMS2 = IRCACHE_OCTAHEDRAL_DIMS * IRCACHE_OCTAHEDRAL_DIMS; 
    static const uint IRCACHE_AUXILIARY_STRIDE = 4 * IRCACHE_OCTAHEDRAL_DIMS2;
    static const uint IRCACHE_IRRADIANCE_STRIDE = 3u;

    static const uint IRCACHE_CASCADE_SIZE = 32u;
    static const uint IRCACHE_CASCADE_COUNT = 12u;

    static const uint IRCACHE_SAMPLES_PER_FRAME = 4;
    static const uint IRCACHE_VALIDATION_SAMPLES_PER_FRAME = 4;

    static const uint SAMPLER_SEQUENCE_LENGTH = 1024;

    struct IrcacheGridCoord
    {
        __init(uint3 id, uint cascade_index)
        {
            _id = min(id, uint3(IRCACHE_CASCADE_SIZE - 1, IRCACHE_CASCADE_SIZE - 1, IRCACHE_CASCADE_SIZE - 1));
            _cascade_index = min(cascade_index, IRCACHE_CASCADE_COUNT - 1);
        }

        uint get_voxel_index()
        {
            uint cascade_offset = IRCACHE_CASCADE_SIZE * IRCACHE_CASCADE_SIZE * IRCACHE_CASCADE_SIZE;
            uint voxel_id = _id.x + _id.y * IRCACHE_CASCADE_SIZE + _id.z * IRCACHE_CASCADE_SIZE * IRCACHE_CASCADE_SIZE;
            return voxel_id + cascade_offset;
        }

        uint3 _id = uint3(0, 0, 0);
        uint _cascade_index = 0;
    };

    struct IrcacheCascadeDesc
    {
        int4 origin;
        int4 voxels_scrolled;
    };

    struct Vertex
    {
        float3 position;
        float3 normal;
    };

    struct CompressedVertex
    {
        Vertex decompress()
        {
            Vertex ret;
            ret.position = _data.xyz;
            ret.normal = decompress_unit_direction(asuint(_data.w));
            return ret;
        }

        static CompressedVertex invalid()
        {
            return CompressedVertex();
        }

        float4 _data = float4(0, 0, 0, 0);
    };

    struct SampleParameter
    {
        __init(uint sample_num_per_frame, uint entry_index, uint sample_index, uint frame_index)
        {
            uint period = IRCACHE_OCTAHEDRAL_DIMS2 / sample_num_per_frame;
            uint xy = sample_index * period + frame_index % period;

            // checker board 变换, 使采样均匀.
            xy ^= (xy & 4) >> 2;

            _pixel_position = xy + ((frame_index << 16) ^ entry_index) * IRCACHE_OCTAHEDRAL_DIMS2;
        }

        uint octahedral_pixel_index()
        {
            return _pixel_position % IRCACHE_OCTAHEDRAL_DIMS2;
        }

        float3 get_direction()
        {
            uint index = octahedral_pixel_index();
            uint2 id = uint2(index % IRCACHE_OCTAHEDRAL_DIMS, index / IRCACHE_OCTAHEDRAL_DIMS);

            float2 random = generate_random2(hash(_pixel_position >> 4) % SAMPLER_SEQUENCE_LENGTH);
            float2 uv = (float2(id) + random) / IRCACHE_OCTAHEDRAL_DIMS;

            return octahedron_to_unit_vector(uv);
        }

        // (x, y)
        // x = _pixel_position & 0xffff,
        // y = (_pixel_position >> 16) & 0xffff.
        uint _pixel_position; 
    };

    struct IrcacheTraceResult
    {
        float3 direction;
        float3 hit_position;
        float3 incident_radiance;
    };

    bool is_ircache_entry_life_valid(uint age)
    {
        return age < IRCACHE_ENTRY_RANK_COUNT * IRCACHE_ENTRY_LIFE_PER_RANK;
    }
}

#endif