#ifndef RENDER_GRAPH_RENDER_PASS_H
#define RENDER_GRAPH_RENDER_PASS_H

#include "../dynamic_rhi/device.h"
#include "render_resource_cache.h"
#include <unordered_set>

namespace fantasy 
{
    enum class RenderPassType : uint8_t
    {
        Invalid         = 0,
        Graphics        = 1 << 1,
        Compute         = 1 << 2,
        
        Precompute      = 1 << 3,
		Exclude         = 1 << 4
    };  
	ENUM_CLASS_FLAG_OPERATORS(RenderPassType)

    
    struct RenderPassInterface
    {
        virtual ~RenderPassInterface() = default;

        virtual bool compile(DeviceInterface* device, RenderResourceCache* cache) = 0;
        virtual bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) = 0;
        
        virtual bool finish_pass() { return true; }

        void precede(RenderPassInterface* pass) 
        {
            if (pass && pass->index != INVALID_SIZE_32)
			{
				successors_index.insert(pass->index);
				pass->dependents_index.insert(index);
            }
        }

        void succeed(RenderPassInterface* pass) 
        {
            if (pass && pass->index != INVALID_SIZE_32)
			{
				dependents_index.insert(pass->index);
				pass->successors_index.insert(index);
            }
        }

        void recompute()
        {
            type &= ~RenderPassType::Exclude;
        }

        std::unordered_set<uint32_t> dependents_index;
        std::unordered_set<uint32_t> successors_index;
        RenderPassType type = RenderPassType::Invalid;
        uint32_t index = INVALID_SIZE_32;
    };
}

#endif