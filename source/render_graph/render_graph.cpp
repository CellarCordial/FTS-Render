#include "render_graph.h"
#include "render_pass.h"
#include "render_resource_cache.h"
#include <cstdint>

#include <d3d12.h>
#include <dxgiformat.h>
#include <memory>
#include <minwindef.h>
#include <queue>
#include <span>
#include <string>
#include <synchapi.h>
#include <unordered_map>
#include <unordered_set>

namespace fantasy
{
    RenderGraph::RenderGraph(const std::shared_ptr<DeviceInterface>& device, std::function<void()> present_func) :
        _device(device), _present_func(present_func)
    {
    }

    bool RenderGraph::initialize(World* world)
    {
        ReturnIfFalse(_device != nullptr && world != nullptr);

		_resource_cache = std::make_unique<RenderResourceCache>(world);
        if (!_resource_cache->initialize())
        {
            LOG_ERROR("Render Graph initialize Failed.");
			_resource_cache.reset();
            return false;
        }
        return true;
    }
        
    void RenderGraph::reset()
    {
        _graphics_wait_value = 0;
        _compute_wait_value = 0;

        _pass_async_types.clear();
        _cmdlists.clear();
		_precompute_cmdlists.clear();
    }

    RenderPassInterface* RenderGraph::add_pass(const std::shared_ptr<RenderPassInterface>& pass)
    {
        if (pass != nullptr)
        {
			if ((pass->type & RenderPassType::Precompute) == RenderPassType::Precompute)
			{
				_precompute_passes.emplace_back(pass);
				pass->index = static_cast<uint32_t>(_precompute_passes.size() - 1);
			}
			else
			{
				_passes.emplace_back(pass);
				pass->index = static_cast<uint32_t>(_passes.size() - 1);
			}
			return pass.get();
        }
		return nullptr;
    }

	bool RenderGraph::topology_passes(bool precompute)
	{
        std::span<std::shared_ptr<RenderPassInterface>> passes;
        if (precompute) passes = _precompute_passes;
        else            passes = _passes;

		std::queue<uint32_t> nodes;
		std::vector<uint32_t> topology_order;
		std::vector<uint32_t> dependent_list(passes.size());

		for (uint32_t ix = 0; ix < passes.size(); ++ix)
		{
			if (passes[ix]->dependents_index.empty()) nodes.push(ix);
			dependent_list[ix] = passes[ix]->dependents_index.size();
		}

		while (!nodes.empty())
		{
			uint32_t index = nodes.front();
			topology_order.push_back(index);
			nodes.pop();

			for (uint32_t ix : passes[index]->successors_index)
			{
				dependent_list[ix]--;
				if (dependent_list[ix] == 0) nodes.push(ix);
			}
		}

		if (topology_order.size() != passes.size())
		{
			LOG_ERROR("Render pass topology occurs error.");
			return false;
		}

		for (uint32_t ix : dependent_list)
		{
			if (ix != 0)
			{
				LOG_ERROR("There is a DAG in RenderPass.");
				return false;
			}
		}

		std::unordered_map<uint32_t, uint32_t> topology_map;
		for (uint32_t ix = 0; ix < passes.size(); ++ix)
		{
			topology_map[topology_order[ix]] = ix;
		}

		std::sort(
			passes.begin(),
			passes.end(),
			[&topology_map](const auto& pass0, const auto& pass1)
			{
				return topology_map[pass0->index] < topology_map[pass1->index];
			}
		);

		for (uint32_t ix = 0; ix < passes.size(); ++ix)
		{
			passes[ix]->index = topology_map[passes[ix]->index];

			std::unordered_set<uint32_t> dependents_index;
			std::unordered_set<uint32_t> successors_index;
			for (auto index : passes[ix]->dependents_index) dependents_index.insert(topology_map[index]);
			for (auto index : passes[ix]->successors_index) successors_index.insert(topology_map[index]);
			passes[ix]->dependents_index = dependents_index;
			passes[ix]->successors_index = successors_index;
		}

		return true;
	}

	bool RenderGraph::compile()
    {
		ReturnIfFalse(topology_passes(true));
		_precompute_cmdlists.resize(_precompute_passes.size());
		for (uint32_t ix = 0; ix < _precompute_passes.size(); ++ix)
		{
			_precompute_cmdlists[ix] = std::unique_ptr<CommandListInterface>(_device->create_command_list(
				CommandListDesc{ 
					.name = std::string("precompute_commandlist") + std::to_string(ix), 
					.queue_type = CommandQueueType::Graphics 
				}
			));
			ReturnIfFalse(
				_precompute_cmdlists[ix] != nullptr && 
				_precompute_passes[ix]->compile(_device.get(), _resource_cache.get())
			);
		}


		ReturnIfFalse(topology_passes(false));

		_cmdlists.resize(_passes.size());
        _pass_async_types.resize(_passes.size(), PassAsyncType::None);
        for (uint32_t ix = 0; ix < _passes.size(); ++ix)
        {
            if ((_passes[ix]->type & RenderPassType::Graphics) != 0)
            {
                _cmdlists[ix] = std::unique_ptr<CommandListInterface>(_device->create_command_list(
                    CommandListDesc{ 
						.name = std::string("commandlist") + std::to_string(ix), 
						.queue_type = CommandQueueType::Graphics
					}
                ));
            }
            else if ((_passes[ix]->type & RenderPassType::Compute) != 0)
            {
                _cmdlists[ix] = std::unique_ptr<CommandListInterface>(_device->create_command_list(
                    CommandListDesc{ 
						.name = std::string("commandlist") + std::to_string(ix), 
						.queue_type = CommandQueueType::Compute
					} 
                ));
            }
            else 
            {
                LOG_ERROR("invalid render pass type.");
                return false;
            }

            ReturnIfFalse(_passes[ix]->compile(_device.get(), _resource_cache.get()));

            for (uint32_t index : _passes[ix]->dependents_index) 
            {
                if (_passes[index]->type != _passes[ix]->type)
                {
                    _pass_async_types[ix] |= PassAsyncType::Wait;
                }
            }
            
            for (uint32_t index : _passes[ix]->successors_index) 
            {
                if (_passes[index]->type != _passes[ix]->type)
                {
                    _pass_async_types[ix] |= PassAsyncType::Signal;
                }
            }
        }

        return true;
    }


	bool RenderGraph::precompute()
	{
		std::vector<CommandListInterface*> cmdlists;

		for (uint32_t ix = 0; ix < _precompute_passes.size(); ++ix)
		{
			if ((_precompute_passes[ix]->type & RenderPassType::Exclude) == RenderPassType::Exclude) continue;

			ReturnIfFalse(_precompute_passes[ix]->execute(_precompute_cmdlists[ix].get(), _resource_cache.get()));
			cmdlists.push_back(_precompute_cmdlists[ix].get());
		}

		_device->execute_command_lists(cmdlists.data(), cmdlists.size(), CommandQueueType::Graphics);
		_device->wait_for_idle();

		for (const auto& pass : _precompute_passes)
		{
			if ((pass->type & RenderPassType::Exclude) == RenderPassType::Exclude) continue;
			pass->type |= RenderPassType::Exclude;
			ReturnIfFalse(pass->finish_pass(_resource_cache.get()));
		}

		return true;
	}


    bool RenderGraph::execute()
    {
		ReturnIfFalse(precompute());

        for (uint32_t ix = 0; ix < _passes.size(); ++ix)
        {
            ReturnIfFalse(_passes[ix]->execute(_cmdlists[ix].get(), _resource_cache.get()));
        }

        std::vector<CommandListInterface*> graphics_cmdlists;
        std::vector<CommandListInterface*> compute_cmdlists;

        for (uint32_t ix = 0; ix < _cmdlists.size(); ++ix)
        {
			bool wait = (_pass_async_types[ix] & PassAsyncType::Wait) != 0;
			bool signal = (_pass_async_types[ix] & PassAsyncType::Signal) != 0;

			if ((_passes[ix]->type & RenderPassType::Graphics) == RenderPassType::Graphics)
			{
				if (wait)
				{
					_device->queue_wait_for_cmdlist(
						CommandQueueType::Graphics,
						CommandQueueType::Compute,
						_graphics_wait_value
					);
				}
				
				graphics_cmdlists.emplace_back(_cmdlists[ix].get());

				if (signal)
				{
					_compute_wait_value = _device->execute_command_lists(
						graphics_cmdlists.data(),
						graphics_cmdlists.size(),
						CommandQueueType::Graphics
					);
					graphics_cmdlists.clear();
					ReturnIfFalse(_compute_wait_value != INVALID_SIZE_64);
				}
			}
			else if ((_passes[ix]->type & RenderPassType::Compute) == RenderPassType::Compute)
			{
				if (wait)
				{
					_device->queue_wait_for_cmdlist(
						CommandQueueType::Compute,
						CommandQueueType::Graphics,
						_compute_wait_value
					);
				}

				compute_cmdlists.emplace_back(_cmdlists[ix].get());

				if (signal)
				{
					_graphics_wait_value = _device->execute_command_lists(
						compute_cmdlists.data(),
						compute_cmdlists.size(),
						CommandQueueType::Compute
					);
					compute_cmdlists.clear();
					ReturnIfFalse(_graphics_wait_value != INVALID_SIZE_64);
				}
			}
		}

        _device->execute_command_lists(graphics_cmdlists.data(), graphics_cmdlists.size(), CommandQueueType::Graphics);
		for (const auto& pass : _passes) ReturnIfFalse(pass->finish_pass(_resource_cache.get()));
		
		_device->wait_for_idle();
		_device->collect_garbage();

		_present_func();

		_resource_cache->frame_index++;

        return true;
    }


}