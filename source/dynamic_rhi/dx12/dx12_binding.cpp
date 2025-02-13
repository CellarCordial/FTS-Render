#include "dx12_binding.h"
#include "dx12_resource.h"
#include "dx12_convert.h"
#include "../../core/tools/check_cast.h"

namespace fantasy 
{
    inline ResourceViewType get_normalized_resource_type(ResourceViewType type)
    {
        switch (type)
        {
        case ResourceViewType::StructuredBuffer_UAV:
        case ResourceViewType::RawBuffer_UAV:
            return ResourceViewType::TypedBuffer_UAV;
        case ResourceViewType::StructuredBuffer_SRV:
        case ResourceViewType::RawBuffer_SRV:
            return ResourceViewType::TypedBuffer_SRV;
        default:
            return type;
        }
    }

    inline bool are_resource_types_compatible(ResourceViewType type1, ResourceViewType type2)
    {
        if (type1 == type2) return true;

        type1 = get_normalized_resource_type(type1);
        type2 = get_normalized_resource_type(type2);

        if (
            type1 == ResourceViewType::TypedBuffer_SRV && type2 == ResourceViewType::Texture_SRV ||
            type2 == ResourceViewType::TypedBuffer_SRV && type1 == ResourceViewType::Texture_SRV ||

            type1 == ResourceViewType::TypedBuffer_SRV && type2 == ResourceViewType::AccelStruct ||
            type2 == ResourceViewType::TypedBuffer_SRV && type1 == ResourceViewType::AccelStruct ||
            
            type1 == ResourceViewType::Texture_SRV && type2 == ResourceViewType::AccelStruct ||
            type2 == ResourceViewType::Texture_SRV && type1 == ResourceViewType::AccelStruct 
        )
            return true;

        if (
            type1 == ResourceViewType::TypedBuffer_UAV && type2 == ResourceViewType::Texture_UAV ||
            type2 == ResourceViewType::TypedBuffer_UAV && type1 == ResourceViewType::Texture_UAV
        )
            return true;

        return false;
    }


    DX12BindingLayout::DX12BindingLayout(const DX12Context* context, const BindingLayoutDesc& desc) :
        _context(context), _desc(desc)
    {
    }

    bool DX12BindingLayout::initialize()
    {
        ResourceViewType current_resource_type = ResourceViewType::None;
        uint32_t current_slot = INVALID_SIZE_32;

        D3D12_ROOT_CONSTANTS d3d12_root_constants = {};

        for (const auto& binding : _desc.binding_layout_items)
        {
            if (binding.type == ResourceViewType::PushConstants)
            {
                ReturnIfFalse(push_constant_size == 0);

                d3d12_root_constants.ShaderRegister = binding.slot;
                d3d12_root_constants.RegisterSpace = _desc.register_space;
                d3d12_root_constants.Num32BitValues = binding.size / 4;
                
                push_constant_size = binding.size;
            }
            else if (binding.type == ResourceViewType::VolatileConstantBuffer)
            {
                D3D12_ROOT_DESCRIPTOR1 d3d12_root_descriptor;
                d3d12_root_descriptor.ShaderRegister = binding.slot;
                d3d12_root_descriptor.RegisterSpace = _desc.register_space;
                d3d12_root_descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
                
                root_param_index_volatile_cb_descriptor_map.push_back(std::make_pair(-1, d3d12_root_descriptor));
            }
            else if (!are_resource_types_compatible(binding.type, current_resource_type) || binding.slot != current_slot + 1)
            {
                if (binding.type == ResourceViewType::Sampler)
                {
                    auto& range = d3d12_descriptor_sampler_ranges.emplace_back();
                    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                    range.NumDescriptors = 1;
                    range.BaseShaderRegister = binding.slot;
                    range.RegisterSpace = _desc.register_space;
                    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
                    range.OffsetInDescriptorsFromTableStart = descriptor_table_sampler_size++;
                }
                else 
                {
                    D3D12_DESCRIPTOR_RANGE_TYPE range_type;

                    switch (binding.type) 
                    {
                    case ResourceViewType::Texture_SRV:
                    case ResourceViewType::TypedBuffer_SRV:
                    case ResourceViewType::StructuredBuffer_SRV:
                    case ResourceViewType::RawBuffer_SRV:
                    case ResourceViewType::AccelStruct:
                        range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                        break;

                    case ResourceViewType::Texture_UAV:
                    case ResourceViewType::TypedBuffer_UAV:
                    case ResourceViewType::StructuredBuffer_UAV:
                    case ResourceViewType::RawBuffer_UAV:
                        range_type = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                        break;

                    case ResourceViewType::ConstantBuffer:
                        range_type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                        break;

                    default:
                        assert(!"invalid Enumeration value");
                        continue;
                    }

                    auto& range = d3d12_descriptor_srv_etc_ranges.emplace_back();
                    range.RangeType = range_type;
                    range.NumDescriptors = 1;
                    range.BaseShaderRegister = binding.slot;
                    range.RegisterSpace = _desc.register_space;
                    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
                    range.OffsetInDescriptorsFromTableStart = descriptor_table_srv_etc_size++;
                }

                current_slot++;
                current_resource_type = binding.type;
            }
            else 
            {
                if (binding.type == ResourceViewType::Sampler)
                {
                    ReturnIfFalse(!d3d12_descriptor_sampler_ranges.empty());

                    d3d12_descriptor_sampler_ranges.back().NumDescriptors++;
                    descriptor_table_sampler_size++;
                }
                else 
                {
                    ReturnIfFalse(!d3d12_descriptor_srv_etc_ranges.empty());

                    d3d12_descriptor_srv_etc_ranges.back().NumDescriptors++;
                    descriptor_table_srv_etc_size++;
                }

                current_slot = binding.slot;
            }
        }

        D3D12_SHADER_VISIBILITY shader_visibility = convert_shader_stage(_desc.shader_visibility);
        d3d12_root_parameters.clear();

        if (push_constant_size != 0)
        {
            auto& d3d12_root_parameter = d3d12_root_parameters.emplace_back();
            d3d12_root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            d3d12_root_parameter.Constants = d3d12_root_constants;
            d3d12_root_parameter.ShaderVisibility = shader_visibility;

            push_constant_root_param_index = static_cast<uint32_t>(d3d12_root_parameters.size()) - 1;
        }

        for (auto& volatile_constant_buffer : root_param_index_volatile_cb_descriptor_map)
        {
            auto& d3d12_root_parameter = d3d12_root_parameters.emplace_back();
            d3d12_root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            d3d12_root_parameter.Descriptor = volatile_constant_buffer.second;
            d3d12_root_parameter.ShaderVisibility = shader_visibility;

            volatile_constant_buffer.first = static_cast<uint32_t>(d3d12_root_parameters.size()) - 1;
        }

        if (descriptor_table_srv_etc_size > 0)
        {
            auto& d3d12_root_parameter = d3d12_root_parameters.emplace_back();
            d3d12_root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            d3d12_root_parameter.DescriptorTable.NumDescriptorRanges = static_cast<uint32_t>(d3d12_descriptor_srv_etc_ranges.size());
            d3d12_root_parameter.DescriptorTable.pDescriptorRanges = d3d12_descriptor_srv_etc_ranges.data();
            d3d12_root_parameter.ShaderVisibility = shader_visibility;

            srv_root_param_start_index = static_cast<uint32_t>(d3d12_root_parameters.size()) - 1;
        }

        if (descriptor_table_sampler_size > 0)
        {
            auto& d3d12_root_parameter = d3d12_root_parameters.emplace_back();
            d3d12_root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            d3d12_root_parameter.DescriptorTable.NumDescriptorRanges = static_cast<uint32_t>(d3d12_descriptor_sampler_ranges.size());
            d3d12_root_parameter.DescriptorTable.pDescriptorRanges = d3d12_descriptor_sampler_ranges.data();
            d3d12_root_parameter.ShaderVisibility = shader_visibility;

            sampler_root_param_start_index = static_cast<uint32_t>(d3d12_root_parameters.size()) - 1;
        }

        return true;
    }

    DX12BindlessLayout::DX12BindlessLayout(const DX12Context* context, const BindlessLayoutDesc& desc_) :
        _context(context), desc(desc_)
    {
    }
    
    bool DX12BindlessLayout::initialize()
    {
        for (const auto& binding : desc.binding_layout_items)
        {
            D3D12_DESCRIPTOR_RANGE_TYPE range_type;

            switch (binding.type)
            {
            case ResourceViewType::Texture_SRV: 
            case ResourceViewType::TypedBuffer_SRV:
            case ResourceViewType::StructuredBuffer_SRV:
            case ResourceViewType::RawBuffer_SRV:
            case ResourceViewType::AccelStruct:
                range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                break;

            case ResourceViewType::ConstantBuffer:
                range_type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                break;

            case ResourceViewType::Texture_UAV:
            case ResourceViewType::TypedBuffer_UAV:
            case ResourceViewType::StructuredBuffer_UAV:
            case ResourceViewType::RawBuffer_UAV:
                range_type = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                break;

            case ResourceViewType::Sampler:
                range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                break;

            default:
                assert(!"invalid Enumeration value");
                continue;
            }

            // NumDescriptors: 范围中的描述符数。 使用 -1 或 UINT_MAX 指定无限大小。 只有表中的最后一个条目可以具有无限大小。
            auto& range = descriptor_ranges.emplace_back();
            range.RangeType = range_type;
            range.NumDescriptors = UINT_MAX; 
            range.BaseShaderRegister = desc.first_slot;
            range.RegisterSpace = binding.register_space;
            range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
            range.OffsetInDescriptorsFromTableStart = 0;
        }

        root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_parameter.DescriptorTable.NumDescriptorRanges = static_cast<uint32_t>(descriptor_ranges.size());
        root_parameter.DescriptorTable.pDescriptorRanges = descriptor_ranges.data();
        root_parameter.ShaderVisibility = convert_shader_stage(desc.shader_visibility);

        return true;
    }


    DX12BindingSet::DX12BindingSet(
        const DX12Context* context,
        DX12DescriptorManager* descriptor_heaps,
        const BindingSetDesc& desc,
        std::shared_ptr<BindingLayoutInterface> binding_layout
    ) :
        _context(context), 
        _descriptor_manager(descriptor_heaps), 
        _desc(desc), 
        _binding_layout(binding_layout)
    {
    }

    DX12BindingSet::~DX12BindingSet() noexcept
    {
        auto dx12_binding_layout = check_cast<DX12BindingLayout>(_binding_layout);
        _descriptor_manager->shader_resource_heap.release_descriptors(srv_start_index, dx12_binding_layout->descriptor_table_srv_etc_size);
        _descriptor_manager->sampler_heap.release_descriptors(sampler_view_start_index, dx12_binding_layout->descriptor_table_sampler_size);
    }

    bool DX12BindingSet::initialize()
    {
        auto binding_layout = check_cast<DX12BindingLayout>(_binding_layout);

        for (const auto& [index, d3d12_root_descriptor] : binding_layout->root_param_index_volatile_cb_descriptor_map)
        {
            BufferInterface* buffer = nullptr;
            for (const auto& binding : _desc.binding_items)
            {
                if (binding.type == ResourceViewType::VolatileConstantBuffer && binding.slot == d3d12_root_descriptor.ShaderRegister)
                {
                    buffer = static_cast<BufferInterface*>(binding.resource.get());
                    break;
                }
            }
            ReturnIfFalse(buffer != nullptr);
            root_param_index_volatile_cb_map.push_back(std::make_pair(index, buffer));
        }

        if (binding_layout->descriptor_table_srv_etc_size > 0)
        {
            is_descriptor_table_srv_etc_valid = true;

            srv_start_index = _descriptor_manager->shader_resource_heap.allocate_descriptors(binding_layout->descriptor_table_srv_etc_size);

            for (const auto& range : binding_layout->d3d12_descriptor_srv_etc_ranges)
            {
                for (uint32_t ix = 0; ix < range.NumDescriptors; ++ix)
                {
                    uint32_t slot = range.BaseShaderRegister + ix;
                    D3D12_CPU_DESCRIPTOR_HANDLE d3d12_descriptor_handle = _descriptor_manager->shader_resource_heap.get_cpu_handle(
                        srv_start_index + range.OffsetInDescriptorsFromTableStart + ix
                    );

                    bool found = false;
                    for (uint32_t jx = 0; jx < _desc.binding_items.size(); ++jx)
                    {
                        const auto& binding = _desc.binding_items[jx];
                        if (binding.slot != slot) continue;

                        ResourceViewType type = get_normalized_resource_type(binding.type);
                        if (type == ResourceViewType::TypedBuffer_SRV && range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
                        {
                            DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(binding.resource.get());
                            dx12_buffer->create_srv(d3d12_descriptor_handle, binding.range, binding.type, binding.format);

                            found = true;
                            break;
                        }
                        else if (type == ResourceViewType::TypedBuffer_UAV && range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
                        {
                            
                            DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(binding.resource.get());
                            dx12_buffer->create_uav(d3d12_descriptor_handle, binding.range, binding.type, binding.format);

                            found = true;
                            break;
                        }
                        else if (type == ResourceViewType::Texture_SRV && range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
                        {
                            DX12Texture* dx12_texture = check_cast<DX12Texture*>(binding.resource.get());
                            dx12_texture->create_srv(d3d12_descriptor_handle, binding.subresource, binding.format);

                            found = true;
                            break;
                        }
                        else if (type == ResourceViewType::Texture_UAV && range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
                        {
                            DX12Texture* dx12_texture = check_cast<DX12Texture*>(binding.resource.get());
                            dx12_texture->create_uav(d3d12_descriptor_handle, binding.subresource, binding.format);

                            found = true;
                            break;
                        }
                        else if (type == ResourceViewType::ConstantBuffer && range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
                        {
                            DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(binding.resource.get());
                            dx12_buffer->create_cbv(d3d12_descriptor_handle, binding.range);

                            found = true;
                            break;
                        }
                        else
                        {
                            continue;
                        }
                    }

                    if (!found) return false;
                }
            }

            _descriptor_manager->shader_resource_heap.copy_to_shader_visible_heap(srv_start_index, binding_layout->descriptor_table_srv_etc_size);
        }

        if (binding_layout->descriptor_table_sampler_size > 0)
        {
            sampler_view_start_index = _descriptor_manager->sampler_heap.allocate_descriptors(binding_layout->descriptor_table_sampler_size);
            is_descriptor_table_sampler_valid = true;

            for (const auto& range : binding_layout->d3d12_descriptor_sampler_ranges)
            {
                for (uint32_t ix = 0; ix < range.NumDescriptors; ++ix)
                {
                    uint32_t slot = range.BaseShaderRegister + ix;
                    D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor = _descriptor_manager->sampler_heap.get_cpu_handle(
                        sampler_view_start_index + range.OffsetInDescriptorsFromTableStart + ix
                    );

                    bool found = false;

                    for (const auto& binding : _desc.binding_items)
                    {
                        if (binding.type == ResourceViewType::Sampler && binding.slot == slot)
                        {
                            std::shared_ptr<DX12Sampler> dx12_sampler = check_cast<DX12Sampler>(binding.resource);
                            dx12_sampler->create_descriptor(d3d12_cpu_descriptor);

                            found = true;
                            break;
                        }
                    }
                    if (!found) return false;
                }
            }

            _descriptor_manager->sampler_heap.copy_to_shader_visible_heap(sampler_view_start_index, binding_layout->descriptor_table_sampler_size);
        }

        return true;
    }


    DX12BindlessSet::DX12BindlessSet(
        const DX12Context* context, 
        DX12DescriptorManager* descriptor_heaps, 
        std::shared_ptr<BindingLayoutInterface> binding_layout
    ) :
        _context(context), _descriptor_manager(descriptor_heaps), _binding_layout(binding_layout)
    {
    }

    bool DX12BindlessSet::initialize()
    {
        return true;
    }

	bool DX12BindlessSet::set_slot(const BindingSetItem& binding)
	{
		ReturnIfFalse(binding.slot < capacity)
        binding_items.push_back(binding);

		D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = _descriptor_manager->shader_resource_heap.get_cpu_handle(first_descriptor_index + binding.slot);

		switch (binding.type)
		{
		case ResourceViewType::Texture_SRV:
		{
			DX12Texture* dx12_texture = check_cast<DX12Texture*>(binding.resource.get());
			dx12_texture->create_srv(descriptor_handle, binding.subresource, binding.format);
			break;
		}
		case ResourceViewType::Texture_UAV:
		{
			DX12Texture* dx12_texture = check_cast<DX12Texture*>(binding.resource.get());
			dx12_texture->create_uav(descriptor_handle, binding.subresource, binding.format);
			break;
		}
		case ResourceViewType::TypedBuffer_SRV:
		case ResourceViewType::StructuredBuffer_SRV:
		case ResourceViewType::RawBuffer_SRV:
		{
			DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(binding.resource.get());
			dx12_buffer->create_srv(descriptor_handle, binding.range, binding.type, binding.format);
			break;
		}
		case ResourceViewType::TypedBuffer_UAV:
		case ResourceViewType::StructuredBuffer_UAV:
		case ResourceViewType::RawBuffer_UAV:
		{
			DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(binding.resource.get());
			dx12_buffer->create_uav(descriptor_handle, binding.range, binding.type, binding.format);
			break;
		}
		case ResourceViewType::ConstantBuffer:
		{
			DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(binding.resource.get());
			dx12_buffer->create_cbv(descriptor_handle, binding.range);
			break;
		}
		default:
			assert(!"Invalid enum");
			return false;
		}

		_descriptor_manager->shader_resource_heap.copy_to_shader_visible_heap(first_descriptor_index + binding.slot);

		return true;
	}

	bool DX12BindlessSet::resize(uint32_t new_size, bool keep_contents)
	{
		if (new_size == capacity) return true;

		uint32_t old_capacity = capacity;

		uint32_t old_first_view_index = first_descriptor_index;

		if (new_size <= old_capacity)
		{
			_descriptor_manager->shader_resource_heap.release_descriptors(old_first_view_index + new_size, old_capacity - new_size);
			capacity = new_size;
			return true;
		}
        if (!keep_contents && old_capacity > 0)
        {
            _descriptor_manager->shader_resource_heap.release_descriptors(old_first_view_index, old_capacity);
        }

        uint32_t new_first_view_index = _descriptor_manager->shader_resource_heap.allocate_descriptors(new_size);
        first_descriptor_index = new_first_view_index;

        if (keep_contents && old_capacity > 0)
        {
            _context->device->CopyDescriptorsSimple(
                old_capacity,
                _descriptor_manager->shader_resource_heap.get_cpu_handle(new_first_view_index),
                _descriptor_manager->shader_resource_heap.get_cpu_handle(old_first_view_index),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            );
            _context->device->CopyDescriptorsSimple(
                old_capacity,
                _descriptor_manager->shader_resource_heap.get_shader_visible_cpu_handle(new_first_view_index),
                _descriptor_manager->shader_resource_heap.get_cpu_handle(old_first_view_index),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            );

            _descriptor_manager->shader_resource_heap.release_descriptors(old_first_view_index, old_capacity);
        }

        capacity = new_size;

        return true;
	}

	DX12BindlessSet::~DX12BindlessSet() noexcept
    {
        _descriptor_manager->shader_resource_heap.release_descriptors(first_descriptor_index, capacity);
    }
}