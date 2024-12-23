#ifndef RENDER_GRAPH_RESOURCE_CACHE_H
#define RENDER_GRAPH_RESOURCE_CACHE_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include "../core/tools/ecs.h"
#include "../dynamic_rhi/resource.h"

namespace fantasy
{
    
    class RenderResourceCache
    {
    public:
        RenderResourceCache(World* world) : _world(world)
        {
        }

        bool initialize();

        void collect(const std::shared_ptr<ResourceInterface>& resource, ResourceType type);
        std::shared_ptr<ResourceInterface> require(const char* name);

		bool collect_constants(const char* name, void* pvData, uint64_t stElementNum = 1);
		bool require_constants(const char* name, void** ppvData, uint64_t* pstElementNum = nullptr);

        World* get_world() const { return _world; }


        uint64_t frame_index = 0;
    private:
        struct ResourceData
        {
            std::shared_ptr<ResourceInterface> resource;
        };

        std::unordered_map<std::string, ResourceData> _resource_names;
		std::unordered_map<std::string, std::pair<void*, uint64_t>> _data_names;

        World* _world;
    };


}













#endif
