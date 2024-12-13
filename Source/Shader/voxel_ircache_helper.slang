#ifndef SHADER_IRCACHE_HELPER_SLANG
#define SHADER_IRCACHE_HELPER_SLANG

#define IRCACHE_ENTRY_LIFE_RECYCLE 0x8000000u
#define IRCACHE_ENTRY_LIFE_RECYCLED (IRCACHE_ENTRY_LIFE_RECYCLE + 1u)

#define IRCACHE_META_ENTRY_COUNT (2 * sizeof(uint))
#define IRCACHE_META_ALLOC_COUNT (3 * sizeof(uint))


namespace voxel_irradiance
{
    static const uint IRCACHE_ENTRY_LIFE_PER_RANK = 4;
    static const uint IRCACHE_ENTRY_RANK_COUNT = 3;
    
    static const uint IRCACHE_ENTRY_META_OCCUPIED = 1u;
    static const uint IRCACHE_ENTRY_META_JUST_ALLOCATED = 2u;

    static const uint IRCACHE_IRRADIANCE_STRIDE = 3u;

    static const uint IRCACHE_CASCADE_SIZE = 32u;
    static const uint IRCACHE_CASCADE_COUNT = 12u;

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

        uint3 _id;
        uint _cascade_index;
    };

    struct IrcacheCascadeDesc
    {
        int4 origin;
        int4 voxels_scrolled;
    };

    struct Vertex
    {
        uint3 position;
        uint3 normal;

        static Vertex get_invalid()
        {
            return { uint3(0, 0, 0), uint3(0, 0, 0) };
        }

        bool is_valid()
        {
            return all(normal == uint3(0, 0, 0));
        }
    };

    bool is_ircache_entry_life_valid(uint age)
    {
        return age < IRCACHE_ENTRY_RANK_COUNT * IRCACHE_ENTRY_LIFE_PER_RANK;
    }
}

#endif