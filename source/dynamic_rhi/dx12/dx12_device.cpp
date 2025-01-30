#include "dx12_device.h"
#include <combaseapi.h>
#include <d3d12.h>
#include <d3dcommon.h>
#include <memory>
#include <minwindef.h>
#include <mutex>
#include <string>
#include <winbase.h>
#include <winerror.h>
#include <wtypesbase.h>

#include "dx12_cmdlist.h"
#include "dx12_descriptor.h"
#include "dx12_pipeline.h"
#include "dx12_frame_buffer.h"
#include "dx12_ray_tracing.h"
#include "../../core/tools/check_cast.h"
#include "../../core/tools/hash_table.h"
#include "../shader.h"
#include "dx12_resource.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace fantasy
{

    DeviceInterface* CreateDevice(const DX12DeviceDesc& desc)
    {
        DX12Device* device = new DX12Device(desc);
        if (!device->initialize())
        {
            LOG_ERROR("Create Device failed.");
            delete device;
            return nullptr;
        }
        return device;
    }
    bool DX12EventQuery::start(ID3D12Fence* d3d12_fence, uint64_t fence_counter)
    {
        if (d3d12_fence == nullptr) return false;

        _started = true;
        _d3d12_fence = d3d12_fence;
        _fence_counter = fence_counter;
        _resolved = false;

        return true;
    }

    bool DX12EventQuery::poll()
    {
        if (!_started) return false;
        if (_resolved) return true;
        
        if (_d3d12_fence == nullptr) return false;

        if (_d3d12_fence->GetCompletedValue() >= _fence_counter)
        {
             _resolved = true;
             _d3d12_fence = nullptr;
        }

        return _resolved;
    }

    void DX12EventQuery::wait(HANDLE wait_event)
    {
        if (!_started || _resolved || _d3d12_fence == nullptr) return;

        wait_for_fence(_d3d12_fence.Get(), _fence_counter, wait_event);
    }

    void DX12EventQuery::reset()
    {
        _started = false;
        _resolved = false;
        _d3d12_fence = nullptr;
    }

    DX12TimerQuery::DX12TimerQuery(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps, uint32_t query_index) :
        _context(context), 
        _descriptor_heaps(descriptor_heaps), 
        begin_query_index(query_index * 2), 
        end_query_index(query_index * 2 + 1)
    {
    }

    bool DX12TimerQuery::poll()
    {
        if (!_started) return false;
        if (_d3d12_fence == nullptr) return true; 

        if (_d3d12_fence->GetCompletedValue() >= _fence_counter)
        {
             _resolved = true;
             _d3d12_fence = nullptr;
        }
        
        return false;
    }

    float DX12TimerQuery::get_time(HANDLE wait_event, uint64_t stFrequency)
    {
        if (!_resolved)
        {
            if (_d3d12_fence != nullptr)
            {
                wait_for_fence(_d3d12_fence.Get(), _fence_counter, wait_event);
                _d3d12_fence = nullptr;
            }

            D3D12_RANGE BufferRange{
                begin_query_index * sizeof(uint64_t),
                (begin_query_index + 2) * sizeof(uint64_t)
            };

            uint64_t* pstData;
            ID3D12Resource* d3d12_resource = reinterpret_cast<ID3D12Resource*>(_context->timer_query_resolve_buffer->get_native_object());
            if (FAILED(d3d12_resource->Map(0, &BufferRange, reinterpret_cast<void**>(&pstData)))) return 0.0f;

            _resolved = true;
            _time = static_cast<float>(static_cast<double>(pstData[end_query_index] - pstData[begin_query_index]) - static_cast<double>(stFrequency));

            d3d12_resource->Unmap(0, nullptr);
        }

        return _time;
    }

    void DX12TimerQuery::reset()
    {
        _started = false;
        _resolved = false;
        _time = 0.0f;
        _d3d12_fence = nullptr;
    }

    DX12Device::DX12Device(const DX12DeviceDesc& desc) : _desc(desc)
    {
    }

    DX12Device::~DX12Device() noexcept
    {
        wait_for_idle();
        if (_fence_event)
        {
            CloseHandle(_fence_event);
            _fence_event = nullptr;
        }
    }


    bool DX12Device::initialize()
    {
        _context.device = _desc.d3d12_device;

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 feature_support_data{};
        _context.device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS5, 
            &feature_support_data, 
            sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5)
        );
        if (feature_support_data.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED) 
            _desc.d3d12_device->QueryInterface(_context.device5.GetAddressOf());

        if (_desc.d3d12_graphics_cmd_queue) _cmd_queues[static_cast<uint8_t>(CommandQueueType::Graphics)] = std::make_unique<DX12CommandQueue>(_context, _desc.d3d12_graphics_cmd_queue);
        if (_desc.d3d12_compute_cmd_queue) _cmd_queues[static_cast<uint8_t>(CommandQueueType::Compute)] = std::make_unique<DX12CommandQueue>(_context, _desc.d3d12_compute_cmd_queue);
        if (_desc.d3d12_copy_cmd_queue) _cmd_queues[static_cast<uint8_t>(CommandQueueType::Copy)] = std::make_unique<DX12CommandQueue>(_context, _desc.d3d12_copy_cmd_queue);

        D3D12_INDIRECT_ARGUMENT_DESC d3d12_indirect_argument_desc{};
        D3D12_COMMAND_SIGNATURE_DESC d3d12_command_signature_desc{};
        d3d12_command_signature_desc.NumArgumentDescs = 1;
        d3d12_command_signature_desc.pArgumentDescs = &d3d12_indirect_argument_desc;

        d3d12_command_signature_desc.ByteStride = 16;
        d3d12_indirect_argument_desc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        _context.device->CreateCommandSignature(
            &d3d12_command_signature_desc, 
            nullptr, 
            IID_PPV_ARGS(&_context.draw_indirect_signature)
        );

        d3d12_command_signature_desc.ByteStride = 20;
        d3d12_indirect_argument_desc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        _context.device->CreateCommandSignature(
            &d3d12_command_signature_desc, 
            nullptr, 
            IID_PPV_ARGS(&_context.draw_indexed_indirect_signature)
        );

        d3d12_command_signature_desc.ByteStride = 12;
        d3d12_indirect_argument_desc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        _context.device->CreateCommandSignature(
            &d3d12_command_signature_desc, 
            nullptr, 
            IID_PPV_ARGS(&_context.dispatch_indirect_signature)
        );

        _descriptor_heaps = std::make_unique<DX12DescriptorHeaps>(_context, _desc.max_timer_queries);
        _descriptor_heaps->render_target_heap.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, _desc.rtv_heap_size, false);
        _descriptor_heaps->depth_stencil_heap.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, _desc.dsv_heap_size, false);
        _descriptor_heaps->shader_resource_heap.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, _desc.srv_heap_size, true);
        _descriptor_heaps->sampler_heap.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, _desc.sampler_heap_size, true);

        _fence_event = CreateEvent(nullptr, false, false, nullptr);
        _d3d12_cmdlists_to_execute.reserve(64u);

        return true;
    }


    HeapInterface* DX12Device::create_heap(const HeapDesc& desc)
    {
        DX12Heap* heap = new DX12Heap(&_context, desc);
        if (!heap->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_heap failed.");
            delete heap;
            return nullptr;
        }
        return heap;
    }

    TextureInterface* DX12Device::create_texture(const TextureDesc& desc)
    {
        DX12Texture* texture = new DX12Texture(&_context, _descriptor_heaps.get(), desc);
        if (!texture->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_texture failed.");
            delete texture;
            return nullptr;
        }
        return texture;
    }
    
    StagingTextureInterface* DX12Device::create_staging_texture(const TextureDesc& desc, CpuAccessMode cpu_access)
    {
        if (cpu_access == CpuAccessMode::None)
        {
            LOG_ERROR("Call to DeviceInterface::create_staging_texture failed for using CpuAccessMode::None.");
            return nullptr;
        }

        DX12StagingTexture* staging_texture = new DX12StagingTexture(&_context, desc, cpu_access);
        if (!staging_texture->initialize(_descriptor_heaps.get()))
        {
            LOG_ERROR("Call to DeviceInterface::create_staging_texture failed.");
            delete staging_texture;
            return nullptr;
        }
        return staging_texture;
    }
    
    BufferInterface* DX12Device::create_buffer(const BufferDesc& desc)
    {
        DX12Buffer* buffer = new DX12Buffer(&_context, _descriptor_heaps.get(), desc);
        if (!buffer->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_buffer failed.");
            delete buffer;
            return nullptr;
        }
        return buffer;
    }

    TextureInterface* DX12Device::create_texture_from_native(void* pNativeTexture, const TextureDesc& desc)
    {
        DX12Texture* texture = new DX12Texture(&_context, _descriptor_heaps.get(), desc);
        texture->_d3d12_resource = static_cast<ID3D12Resource*>(pNativeTexture);
        return texture;
    }

    BufferInterface* DX12Device::create_buffer_from_native(void* pNativeBuffer, const BufferDesc& desc)
    {
        DX12Buffer* buffer = new DX12Buffer(&_context, _descriptor_heaps.get(), desc);
        buffer->_d3d12_resource = static_cast<ID3D12Resource*>(pNativeBuffer);
        return buffer;
    }

    SamplerInterface* DX12Device::create_sampler(const SamplerDesc& desc)
    {
        DX12Sampler* sampler = new DX12Sampler(&_context, desc);
        if (!sampler->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_sampler failed.");
            delete sampler;
            return nullptr;
        }
        return sampler;
    }

    InputLayoutInterface* DX12Device::create_input_layout(const VertexAttributeDesc* cpDesc, uint32_t dwAttributeNum, Shader* pVertexShader)
    {
        StackArray<VertexAttributeDesc, MAX_VERTEX_ATTRIBUTES> Attributes(dwAttributeNum);
        for (uint32_t ix = 0; ix < dwAttributeNum; ++ix)
        {
            Attributes[ix] = cpDesc[ix];
        }

        // pVertexShader 在 dx12 中不会用到.

        DX12InputLayout* input_layout = new DX12InputLayout(&_context);
        if (!input_layout->initialize(Attributes))
        {
            LOG_ERROR("Call to DeviceInterface::create_input_layout failed.");
            delete input_layout;
            return nullptr;
        }
        return input_layout;
    }
    
    EventQueryInterface* DX12Device::create_event_query()
    {
        DX12EventQuery* event_query = new DX12EventQuery();
        if (!event_query->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_event_query failed.");
            delete event_query;
            return nullptr;
        }
        return event_query;
    }

    bool DX12Device::set_event_query(EventQueryInterface* query, CommandQueueType queue_type)
    {
        if (query == nullptr) return false;

        DX12EventQuery* pDX12EventQuery = check_cast<DX12EventQuery*>(query);
        DX12CommandQueue* pQueue = GetQueue(queue_type);
        return pDX12EventQuery->start(pQueue->d3d12_fence.Get(), pQueue->last_submitted_value);
    }

    bool DX12Device::poll_event_query(EventQueryInterface* query)
    {
        if (query == nullptr) return false;
        
        DX12EventQuery* pDX12EventQuery = check_cast<DX12EventQuery*>(query);

        return pDX12EventQuery->poll();
    }

    bool DX12Device::wait_event_query(EventQueryInterface* query)
    {
        if (query == nullptr) return false;

        DX12EventQuery* pDX12EventQuery = check_cast<DX12EventQuery*>(query);

        pDX12EventQuery->wait(_fence_event);
        return true;
    }

    bool DX12Device::reset_event_query(EventQueryInterface* query)
    {
        if (query == nullptr) return false;

        DX12EventQuery* pDX12EventQuery = check_cast<DX12EventQuery*>(query);
        pDX12EventQuery->reset();
        
        return true;
    }
    
    TimerQueryInterface* DX12Device::create_timer_query()
    {
        if (_context.timer_query_heap != nullptr)
        {
            std::lock_guard lock_guard(_mutex);

            if (_context.timer_query_heap != nullptr)
            {
                D3D12_QUERY_HEAP_DESC desc{};
                desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
                desc.Count = static_cast<uint32_t>(_descriptor_heaps->time_queries.get_capacity() * 2); // Use 2 D3D12 queries per 1 TimerQuery

                auto timer_query_heap = _context.timer_query_heap;
                if (FAILED(_context.device->CreateQueryHeap(&desc, IID_PPV_ARGS(timer_query_heap.GetAddressOf())))) return nullptr;

                BufferDesc buffer_desc{};
                buffer_desc.byte_size = static_cast<uint64_t>(desc.Count) * 8;
                buffer_desc.cpu_access = CpuAccessMode::Read;
                buffer_desc.name = "TimerQueryResolveBuffer";

                DX12Buffer* buffer = new DX12Buffer(&_context, _descriptor_heaps.get(), buffer_desc);
                if (!buffer->initialize())
                {
                    LOG_ERROR("Call to DeviceInterface::create_buffer failed.");
                    delete buffer;
                    return nullptr;
                }
                _context.timer_query_resolve_buffer = std::unique_ptr<BufferInterface>(buffer);
            }  
        }

        DX12TimerQuery* timer_query = new DX12TimerQuery(&_context, _descriptor_heaps.get(), _descriptor_heaps->time_queries.allocate());
        if (!timer_query->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_timer_query failed.");
            _context.timer_query_resolve_buffer.reset();
            delete timer_query;
            return nullptr;
        }
        return timer_query;
    }

    bool DX12Device::poll_timer_query(TimerQueryInterface* query)
    {
        DX12TimerQuery* dx12_timer_query = check_cast<DX12TimerQuery*>(query);
        ReturnIfFalse(dx12_timer_query->poll());
        return true;
    }

    float DX12Device::get_timer_query_time(TimerQueryInterface* query)
    {
        DX12TimerQuery* dx12_timer_query = check_cast<DX12TimerQuery*>(query);

        uint64_t frequency = 0;
        GetQueue(CommandQueueType::Graphics)->d3d12_cmd_queue->GetTimestampFrequency(&frequency);
        return dx12_timer_query->get_time(_fence_event, frequency);
    }

    bool DX12Device::reset_timer_query(TimerQueryInterface* query)
    {
        DX12TimerQuery* dx12_timer_query = check_cast<DX12TimerQuery*>(query);

        dx12_timer_query->reset();

        return true;
    }
    
    GraphicsAPI DX12Device::get_graphics_api() const
    {
        return GraphicsAPI::D3D12;
    }

    void* DX12Device::get_native_descriptor_heap(DescriptorHeapType type) const
    {
        switch (type)
        {
        case DescriptorHeapType::RenderTargetView: return static_cast<void*>(_descriptor_heaps->render_target_heap.get_shader_visible_heap());
        case DescriptorHeapType::DepthStencilView: return static_cast<void*>(_descriptor_heaps->depth_stencil_heap.get_shader_visible_heap());
        case DescriptorHeapType::ShaderResourceView: return static_cast<void*>(_descriptor_heaps->shader_resource_heap.get_shader_visible_heap());
        case DescriptorHeapType::Sampler: return static_cast<void*>(_descriptor_heaps->sampler_heap.get_shader_visible_heap());      
        default: return nullptr;
        }
    }

    void* DX12Device::get_native_object() const
    {
        return static_cast<void*>(_desc.d3d12_device);
    }

    
    FrameBufferInterface* DX12Device::create_frame_buffer(const FrameBufferDesc& desc)
    {
        DX12FrameBuffer* frame_buffer = new DX12FrameBuffer(&_context, _descriptor_heaps.get(), desc);
        if (!frame_buffer->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_frame_buffer failed.");
            delete frame_buffer;
            return nullptr;
        }
        return frame_buffer;
    }

    GraphicsPipelineInterface* DX12Device::create_graphics_pipeline(const GraphicsPipelineDesc& desc, FrameBufferInterface* frame_buffer)
    {
        bool allow_input_layout = desc.input_layout != nullptr;

        uint64_t hash_key = 0;
        for (uint32_t ix = 0; ix < desc.binding_layouts.size(); ++ix)
        {
            hash_combine(hash_key, desc.binding_layouts[ix]);
        }

        hash_combine(hash_key, allow_input_layout ? 1u : 0u);

        DX12RootSignature* dx12_root_signature = _descriptor_heaps->dx12_root_signatures[hash_key];

        if (dx12_root_signature == nullptr)
        {
            dx12_root_signature = build_root_signature(desc.binding_layouts, allow_input_layout);
            if (!dx12_root_signature)
            {
                LOG_ERROR("Call to DeviceInterface::create_graphics_pipeline failed.");
                return nullptr;
            }

            dx12_root_signature->hash_index = hash_key;
            _descriptor_heaps->dx12_root_signatures[hash_key] = dx12_root_signature;
        }

        DX12GraphicsPipeline* graphics_pipeline = new DX12GraphicsPipeline(&_context, desc, dx12_root_signature, frame_buffer->get_info());
        if (!graphics_pipeline->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_graphics_pipeline failed.");
            delete graphics_pipeline;
            return nullptr;
        }
        return graphics_pipeline;
    }

    ComputePipelineInterface* DX12Device::create_compute_pipeline(const ComputePipelineDesc& desc)
    {
        bool allow_input_layout = false;

        uint64_t hash_key = 0;
        for (const auto& binding_layout : desc.binding_layouts)
        {
            hash_combine(hash_key, binding_layout);
        }

        hash_combine(hash_key, allow_input_layout ? 1u : 0u);

        DX12RootSignature* dx12_root_signature = _descriptor_heaps->dx12_root_signatures[hash_key];

        if (dx12_root_signature == nullptr)
        {
            dx12_root_signature = build_root_signature(desc.binding_layouts, allow_input_layout);

            dx12_root_signature->hash_index = hash_key;
            _descriptor_heaps->dx12_root_signatures[hash_key] = dx12_root_signature;
        }


        DX12ComputePipeline* compute_pipeline = new DX12ComputePipeline(&_context, desc, dx12_root_signature);
        if (!compute_pipeline->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_compute_pipeline failed.");
            delete compute_pipeline;
            return nullptr;
        }
        return compute_pipeline;
    }

    BindingLayoutInterface* DX12Device::create_binding_layout(const BindingLayoutDesc& desc)
    {
        DX12BindingLayout* binding_layout = new DX12BindingLayout(&_context, desc);
        if (!binding_layout->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_binding_layout failed.");
            delete binding_layout;
            return nullptr;
        }
        return binding_layout;
    }

    BindingLayoutInterface* DX12Device::create_bindless_layout(const BindlessLayoutDesc& desc)
    {
        DX12BindlessLayout* bindless_layout = new DX12BindlessLayout(&_context, desc);
        if (!bindless_layout->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_bindless_layout failed.");
            delete bindless_layout;
            return nullptr;
        }
        return bindless_layout;
    }

    BindingSetInterface* DX12Device::create_binding_set(const BindingSetDesc& desc, BindingLayoutInterface* pLayout)
    {
        DX12BindingSet* binding_set = new DX12BindingSet(&_context, _descriptor_heaps.get(), desc, pLayout);
        if (!binding_set->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_binding_set failed.");
            delete binding_set;
            return nullptr;
        }
        return binding_set;
    }
    
    BindlessSetInterface* DX12Device::create_bindless_set(BindingLayoutInterface* pLayout)
    {
        // pLayout is useless on dx12.

        DX12BindlessSet* bindless_set = new DX12BindlessSet(&_context, _descriptor_heaps.get());
        if (!bindless_set->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::CreateDescriptorTable failed.");
            delete bindless_set;
            return nullptr;
        }
        return bindless_set;
    }

    CommandListInterface* DX12Device::create_command_list(const CommandListDesc& desc)
    {
        DX12CommandList* cmdlist = new DX12CommandList(&_context, _descriptor_heaps.get(), this, GetQueue(desc.queue_type), desc);
        if (!cmdlist->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_command_list failed.");
            delete cmdlist;
            return nullptr;
        }
        return cmdlist;
    }

    uint64_t DX12Device::execute_command_lists(CommandListInterface* const* cmdlists, uint64_t cmd_count, CommandQueueType queue_type)
    {
        _d3d12_cmdlists_to_execute.resize(cmd_count);
        for (uint64_t ix = 0; ix < cmd_count; ++ix)
        {
            DX12CommandList* dx12_cmdlist = check_cast<DX12CommandList*>(cmdlists[ix]);
            _d3d12_cmdlists_to_execute[ix] = dx12_cmdlist->active_cmdlist->d3d12_cmdlist.Get();
        }

        DX12CommandQueue* cmd_queue = GetQueue(queue_type);
        cmd_queue->d3d12_cmd_queue->ExecuteCommandLists(static_cast<uint32_t>(_d3d12_cmdlists_to_execute.size()), _d3d12_cmdlists_to_execute.data());
        cmd_queue->last_submitted_value++;
        cmd_queue->d3d12_cmd_queue->Signal(cmd_queue->d3d12_fence.Get(), cmd_queue->last_submitted_value);

        for (uint64_t ix = 0; ix < cmd_count; ++ix)
        {
            DX12CommandList* dx12_cmdlist = check_cast<DX12CommandList*>(cmdlists[ix]);

            cmd_queue->dx12_cmdlists_in_flight.push_front(dx12_cmdlist->excuted(
                cmd_queue->d3d12_fence.Get(),
                cmd_queue->last_submitted_value
            ));
        }

        if (FAILED(_context.device->GetDeviceRemovedReason()))
        {
            LOG_CRITICAL("Device removed.");
            return INVALID_SIZE_64;
        }

        return cmd_queue->last_submitted_value;
    }

    bool DX12Device::queue_wait_for_cmdlist(CommandQueueType WaitQueueType, CommandQueueType queue_type, uint64_t instance)
    {
        DX12CommandQueue* wait_queue = GetQueue(WaitQueueType);
        DX12CommandQueue* execution_queue = GetQueue(queue_type);
        ReturnIfFalse(instance <= execution_queue->last_submitted_value);

        if (FAILED(wait_queue->d3d12_cmd_queue->Wait(execution_queue->d3d12_fence.Get(), instance))) return false;   
        return true;
    }

	ray_tracing::PipelineInterface* DX12Device::create_ray_tracing_pipline(const ray_tracing::PipelineDesc& desc)
	{
        ray_tracing::DX12Pipeline* dx12_pipeline = new ray_tracing::DX12Pipeline(&_context, desc);
        
        std::vector<std::unique_ptr<DX12RootSignature>> shader_root_signatures;
        std::vector<std::unique_ptr<DX12RootSignature>> hit_group_root_signatures;

        std::unique_ptr<DX12RootSignature> global_root_signature = 
            std::unique_ptr<DX12RootSignature>(build_root_signature(desc.global_binding_layouts, false));
        
        for (const auto& shader_desc : desc.shader_descs)
        {
            DX12RootSignature* dx12_root_signature = nullptr;
            if (shader_desc.binding_layout)
            {
                BindingLayoutInterfaceArray tmp;
                tmp.push_back(shader_desc.binding_layout);
                dx12_root_signature = build_root_signature(tmp, false);
            }
            shader_root_signatures.emplace_back(dx12_root_signature);
        }

        for (const auto& hit_group_desc : desc.hit_group_descs)
        {
            DX12RootSignature* dx12_root_signature = nullptr;
            if (hit_group_desc.binding_layout)
            {
                BindingLayoutInterfaceArray tmp;
                tmp.push_back(hit_group_desc.binding_layout);
                dx12_root_signature = build_root_signature(tmp, false);
            }
            hit_group_root_signatures.emplace_back(dx12_root_signature);
        }

        if (!dx12_pipeline->initialize(
            std::move(shader_root_signatures), 
            std::move(hit_group_root_signatures), 
            std::move(global_root_signature)))
        {
            LOG_ERROR("Create Ray Tracing Pipeilne Failed.");
            delete dx12_pipeline;
            return nullptr;
        }
        return dx12_pipeline;
	}

	ray_tracing::AccelStructInterface* DX12Device::create_accel_struct(const ray_tracing::AccelStructDesc& desc)
	{
        ray_tracing::DX12AccelStruct* dx12_accel_struct = new ray_tracing::DX12AccelStruct(&_context, _descriptor_heaps.get(), desc);
        if (!dx12_accel_struct->initialize())
        {
            LOG_ERROR("Create Accel Structure Failed.");
            delete dx12_accel_struct;
            return nullptr;
        }
		return dx12_accel_struct;
	}

	void DX12Device::wait_for_idle()
    {
        for (const auto& queue : _cmd_queues)
        {
            if (queue == nullptr) continue;

            if (queue->update_last_completed_value() < queue->last_submitted_value)
            {
                wait_for_fence(queue->d3d12_fence.Get(), queue->last_submitted_value, _fence_event);
            }
        }
    }
    
    void DX12Device::collect_garbage()
    {
        for (const auto& queue : _cmd_queues)
        {
            if (queue == nullptr) continue;
            
            queue->update_last_completed_value();

            while (!queue->dx12_cmdlists_in_flight.empty())
            {
                if (queue->last_compeleted_value >= queue->dx12_cmdlists_in_flight.back()->submitted_value) 
                {
                    queue->dx12_cmdlists_in_flight.pop_back();
                }
                else
                {
                    break;
                }
            }
        }
    }


    DX12RootSignature* DX12Device::build_root_signature(const BindingLayoutInterfaceArray& binding_layouts, bool allow_input_layout) const
    {
        DX12RootSignature* root_signature = new DX12RootSignature(&_context, _descriptor_heaps.get());
        if (!root_signature->initialize(binding_layouts, allow_input_layout))
        {
            LOG_ERROR("Call to DeviceInterface::CreateRootSignature failed.");
            delete root_signature;
            return nullptr;
        }
        return root_signature;
    }

    DX12CommandQueue* DX12Device::GetQueue(CommandQueueType type) const
    {
        return _cmd_queues[static_cast<uint8_t>(type)].get();
    }


}