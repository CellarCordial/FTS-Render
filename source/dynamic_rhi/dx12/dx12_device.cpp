#include "dx12_device.h"
#include <combaseapi.h>
#include <cstdint>
#include <d3d12.h>
#include <d3dcommon.h>
#include <memory>
#include <minwindef.h>
#include <string>
#include <winbase.h>
#include <winerror.h>
#include <wtypesbase.h>

#include "dx12_cmdlist.h"
#include "dx12_binding.h"
#include "dx12_pipeline.h"
#include "dx12_frame_buffer.h"
#include "dx12_ray_tracing.h"
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
            delete device;
            return nullptr;
        }
        return device;
    }


    DX12Device::DX12Device(const DX12DeviceDesc& desc_) : 
        descriptor_manager(&context), desc(desc_)
    {
    }

    DX12Device::~DX12Device() noexcept
    {
        wait_for_idle();
    }


    bool DX12Device::initialize()
    {
        context.device = desc.d3d12_device;

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 feature_support_data{};
        context.device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS5, 
            &feature_support_data, 
            sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5)
        );
        if (feature_support_data.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED) 
            desc.d3d12_device->QueryInterface(context.device5.GetAddressOf());


        if (desc.d3d12_graphics_cmd_queue) cmdqueues[static_cast<uint8_t>(CommandQueueType::Graphics)] = 
            std::make_unique<DX12CommandQueue>(&context, CommandQueueType::Graphics, desc.d3d12_graphics_cmd_queue);
        if (desc.d3d12_compute_cmd_queue) cmdqueues[static_cast<uint8_t>(CommandQueueType::Compute)] = 
            std::make_unique<DX12CommandQueue>(&context, CommandQueueType::Compute, desc.d3d12_compute_cmd_queue);

        
        D3D12_INDIRECT_ARGUMENT_DESC d3d12_indirect_argument_desc{};
        D3D12_COMMAND_SIGNATURE_DESC d3d12_command_signature_desc{};
        d3d12_command_signature_desc.NumArgumentDescs = 1;
        d3d12_command_signature_desc.pArgumentDescs = &d3d12_indirect_argument_desc;

        d3d12_command_signature_desc.ByteStride = 16;
        d3d12_indirect_argument_desc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        context.device->CreateCommandSignature(
            &d3d12_command_signature_desc, 
            nullptr, 
            IID_PPV_ARGS(&context.draw_indirect_signature)
        );

        d3d12_command_signature_desc.ByteStride = 20;
        d3d12_indirect_argument_desc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        context.device->CreateCommandSignature(
            &d3d12_command_signature_desc, 
            nullptr, 
            IID_PPV_ARGS(&context.draw_indexed_indirect_signature)
        );

        d3d12_command_signature_desc.ByteStride = 12;
        d3d12_indirect_argument_desc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        context.device->CreateCommandSignature(
            &d3d12_command_signature_desc, 
            nullptr, 
            IID_PPV_ARGS(&context.dispatch_indirect_signature)
        );

        descriptor_manager.render_target_heap.initialize();
        descriptor_manager.depth_stencil_heap.initialize();
        descriptor_manager.shader_resource_heap.initialize();
        descriptor_manager.sampler_heap.initialize();

         return true;
    }


    HeapInterface* DX12Device::create_heap(const HeapDesc& desc)
    {
        DX12Heap* heap = new DX12Heap(&context, desc);
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
        DX12Texture* texture = new DX12Texture(&context, &descriptor_manager, desc);
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

        DX12StagingTexture* staging_texture = new DX12StagingTexture(&context, desc, cpu_access);
        if (!staging_texture->initialize(&descriptor_manager))
        {
            LOG_ERROR("Call to DeviceInterface::create_staging_texture failed.");
            delete staging_texture;
            return nullptr;
        }
        return staging_texture;
    }
    
    BufferInterface* DX12Device::create_buffer(const BufferDesc& desc)
    {
        DX12Buffer* buffer = new DX12Buffer(&context, &descriptor_manager, desc);
        if (!buffer->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_buffer failed.");
            delete buffer;
            return nullptr;
        }
        return buffer;
    }

    TextureInterface* DX12Device::create_texture_from_native(void* native_texture, const TextureDesc& desc)
    {
        DX12Texture* texture = new DX12Texture(&context, &descriptor_manager, desc);
        texture->d3d12_resource = static_cast<ID3D12Resource*>(native_texture);
        return texture;
    }

    BufferInterface* DX12Device::create_buffer_from_native(void* native_buffer, const BufferDesc& desc)
    {
        DX12Buffer* buffer = new DX12Buffer(&context, &descriptor_manager, desc);
        buffer->d3d12_resource = static_cast<ID3D12Resource*>(native_buffer);
        return buffer;
    }

    SamplerInterface* DX12Device::create_sampler(const SamplerDesc& desc)
    {
        DX12Sampler* sampler = new DX12Sampler(&context, desc);
        if (!sampler->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_sampler failed.");
            delete sampler;
            return nullptr;
        }
        return sampler;
    }

    InputLayoutInterface* DX12Device::create_input_layout(const VertexAttributeDesc* cpDesc, uint32_t attribute_count)
    {
        StackArray<VertexAttributeDesc, MAX_VERTEX_ATTRIBUTES> Attributes(attribute_count);
        for (uint32_t ix = 0; ix < attribute_count; ++ix)
        {
            Attributes[ix] = cpDesc[ix];
        }

        // pVertexShader 在 dx12 中不会用到.

        DX12InputLayout* input_layout = new DX12InputLayout(&context);
        if (!input_layout->initialize(Attributes))
        {
            LOG_ERROR("Call to DeviceInterface::create_input_layout failed.");
            delete input_layout;
            return nullptr;
        }
        return input_layout;
    }
    
    GraphicsAPI DX12Device::get_graphics_api() const
    {
        return GraphicsAPI::D3D12;
    }

    void* DX12Device::get_native_object() const
    {
        return static_cast<void*>(desc.d3d12_device.Get());
    }

    
    FrameBufferInterface* DX12Device::create_frame_buffer(const FrameBufferDesc& desc)
    {
        DX12FrameBuffer* frame_buffer = new DX12FrameBuffer(&context, &descriptor_manager, desc);
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
        DX12GraphicsPipeline* graphics_pipeline = new DX12GraphicsPipeline(&context, desc);
        if (!graphics_pipeline->initialize(frame_buffer->get_info()))
        {
            LOG_ERROR("Call to DeviceInterface::create_graphics_pipeline failed.");
            delete graphics_pipeline;
            return nullptr;
        }
        return graphics_pipeline;
    }

    ComputePipelineInterface* DX12Device::create_compute_pipeline(const ComputePipelineDesc& desc)
    {
        DX12ComputePipeline* compute_pipeline = new DX12ComputePipeline(&context, desc);
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
        DX12BindingLayout* binding_layout = new DX12BindingLayout(&context, desc);
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
        DX12BindlessLayout* bindless_layout = new DX12BindlessLayout(&context, desc);
        if (!bindless_layout->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_bindless_layout failed.");
            delete bindless_layout;
            return nullptr;
        }
        return bindless_layout;
    }

    BindingSetInterface* DX12Device::create_binding_set(const BindingSetDesc& desc, std::shared_ptr<BindingLayoutInterface> binding_layout)
    {
        DX12BindingSet* binding_set = new DX12BindingSet(&context, &descriptor_manager, desc, binding_layout);
        if (!binding_set->initialize())
        {
            LOG_ERROR("Call to DeviceInterface::create_binding_set failed.");
            delete binding_set;
            return nullptr;
        }
        return binding_set;
    }
    
    BindlessSetInterface* DX12Device::create_bindless_set(std::shared_ptr<BindingLayoutInterface> binding_layout)
    {
        DX12BindlessSet* bindless_set = new DX12BindlessSet(&context, &descriptor_manager, binding_layout);
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
        DX12CommandList* cmdlist = new DX12CommandList(&context, &descriptor_manager, this, desc);
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
        return get_queue(queue_type)->execute(cmdlists, cmd_count);
    }

    void DX12Device::queue_wait_for_cmdlist(CommandQueueType wait_queue_type, CommandQueueType execution_queue_type, uint64_t submit_id)
    {
        DX12CommandQueue* wait_queue = get_queue(wait_queue_type);
        DX12CommandQueue* execution_queue = get_queue(execution_queue_type);

        wait_queue->add_wait_fence(execution_queue->d3d12_tracking_fence, submit_id);
    }

	void DX12Device::wait_for_idle()
    {
        for (const auto& queue : cmdqueues)
        {
            queue->wait_for_idle();
        }
    }

    uint64_t DX12Device::queue_get_completed_id(CommandQueueType type)
    {
        return get_queue(type)->get_last_finished_id();
    }

    void DX12Device::collect_garbage()
    {
        for (auto& queue : cmdqueues)
        {
            queue->retire_command_lists();
        }
    }

    DX12CommandQueue* DX12Device::get_queue(CommandQueueType type) const
    {
        return cmdqueues[static_cast<uint32_t>(type)].get();
    }

	// ray_tracing::PipelineInterface* DX12Device::create_ray_tracing_pipline(const ray_tracing::PipelineDesc& desc)
	// {
    //     ray_tracing::DX12Pipeline* dx12_pipeline = new ray_tracing::DX12Pipeline(&context, desc);
        
    //     std::vector<std::unique_ptr<DX12RootSignature>> shader_root_signatures;
    //     std::vector<std::unique_ptr<DX12RootSignature>> hit_group_root_signatures;

    //     std::unique_ptr<DX12RootSignature> global_root_signature = 
    //         std::unique_ptr<DX12RootSignature>(build_root_signature(desc.global_binding_layouts, false));
        
    //     for (const auto& shader_desc : desc.shader_descs)
    //     {
    //         DX12RootSignature* dx12_root_signature = nullptr;
    //         if (shader_desc.binding_layout)
    //         {
    //             BindingLayoutInterfaceArray tmp;
    //             tmp.push_back(shader_desc.binding_layout);
    //             dx12_root_signature = build_root_signature(tmp, false);
    //         }
    //         shader_root_signatures.emplace_back(dx12_root_signature);
    //     }

    //     for (const auto& hit_group_desc : desc.hit_group_descs)
    //     {
    //         DX12RootSignature* dx12_root_signature = nullptr;
    //         if (hit_group_desc.binding_layout)
    //         {
    //             BindingLayoutInterfaceArray tmp;
    //             tmp.push_back(hit_group_desc.binding_layout);
    //             dx12_root_signature = build_root_signature(tmp, false);
    //         }
    //         hit_group_root_signatures.emplace_back(dx12_root_signature);
    //     }

    //     if (!dx12_pipeline->initialize(
    //         std::move(shader_root_signatures), 
    //         std::move(hit_group_root_signatures), 
    //         std::move(global_root_signature)))
    //     {
    //         LOG_ERROR("Create Ray Tracing Pipeilne Failed.");
    //         delete dx12_pipeline;
    //         return nullptr;
    //     }
    //     return dx12_pipeline;
	// }

	// ray_tracing::AccelStructInterface* DX12Device::create_accel_struct(const ray_tracing::AccelStructDesc& desc)
	// {
    //     ray_tracing::DX12AccelStruct* dx12_accel_struct = new ray_tracing::DX12AccelStruct(&context, &descriptor_manager, desc);
    //     if (!dx12_accel_struct->initialize())
    //     {
    //         LOG_ERROR("Create Accel Structure Failed.");
    //         delete dx12_accel_struct;
    //         return nullptr;
    //     }
	// 	return dx12_accel_struct;
	// }

    
}