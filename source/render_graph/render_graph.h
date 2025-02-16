#ifndef RENDER_GRAPH_H
#define RENDER_GRAPH_H

#include "../dynamic_rhi/device.h"
#include "../core/tools/ecs.h"
#include "render_resource_cache.h"
#include "render_pass.h"
#include <memory>

namespace fantasy 
{
    enum class PassAsyncType
    {
        None        = 0,
        Wait        = 0x001,
        Signal      = 0x002,
        WaitSignal  = 0x004
    };
    ENUM_CLASS_FLAG_OPERATORS(PassAsyncType)


    class RenderGraph
    {
    public:
        RenderGraph(const std::shared_ptr<DeviceInterface>& device, std::function<void()> present_func);
        ~RenderGraph() { reset(); }

        bool initialize(World* world);

        bool compile();
        bool execute();
        
        void reset();

        DeviceInterface* get_deivce() const { return _device.get(); }
        RenderResourceCache* get_resource_cache() const { return _resource_cache.get(); }

        RenderPassInterface* add_pass(const std::shared_ptr<RenderPassInterface>& pass);

    private:
        bool topology_passes(bool precompute);
        bool precompute();

    private:
        std::shared_ptr<DeviceInterface> _device;
        std::function<void()> _present_func;

        std::unique_ptr<RenderResourceCache> _resource_cache;

        std::vector<PassAsyncType> _pass_async_types;

		std::vector<std::unique_ptr<CommandListInterface>> _cmdlists;
		std::vector<std::unique_ptr<CommandListInterface>> _precompute_cmdlists;

        std::vector<std::shared_ptr<RenderPassInterface>> _passes;
        std::vector<std::shared_ptr<RenderPassInterface>> _precompute_passes;

        uint64_t _graphics_wait_value = 0;
        uint64_t _compute_wait_value = 0;

        uint64_t _frame_count = 0;
    };
}


















#endif