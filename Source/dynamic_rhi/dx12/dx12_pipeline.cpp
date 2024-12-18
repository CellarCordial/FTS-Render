#include "dx12_pipeline.h"
#include "dx12_resource.h"
#include "dx12_converts.h"
#include <cassert>
#include <combaseapi.h>
#include <d3d12.h>
#include <d3dcommon.h>
#include <memory>
#include <sstream>
#include <utility>
#include "../../core/tools/check_cast.h"

namespace fantasy 
{

    inline ResourceViewType get_normalized_resource_type(ResourceViewType type)
    {
        switch (type)  // NOLINT(clang-diagnostic-switch-enum)
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

        if (type1 == ResourceViewType::TypedBuffer_SRV && type2 == ResourceViewType::Texture_SRV ||
            type2 == ResourceViewType::TypedBuffer_SRV && type1 == ResourceViewType::Texture_SRV)
            return true;

        if (type1 == ResourceViewType::TypedBuffer_UAV && type2 == ResourceViewType::Texture_UAV ||
            type2 == ResourceViewType::TypedBuffer_UAV && type1 == ResourceViewType::Texture_UAV)
            return true;

        return false;
    }

    void create_null_buffer_srv(uint64_t descriptor_address, Format format, const DX12Context* context)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC view_desc{};
        view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        view_desc.Format = get_dxgi_format_mapping(format == Format::UNKNOWN ? Format::R32_UINT : format).srv_format;
        context->device->CreateShaderResourceView(nullptr, &view_desc, D3D12_CPU_DESCRIPTOR_HANDLE{descriptor_address });
    }
    
    void CreateNullBufferUAV(uint64_t descriptor_address, Format format, const DX12Context* context)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC view_desc{};
        view_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        view_desc.Format = get_dxgi_format_mapping(format == Format::UNKNOWN ? Format::R32_UINT : format).srv_format;
        context->device->CreateUnorderedAccessView(nullptr, nullptr, &view_desc, D3D12_CPU_DESCRIPTOR_HANDLE{descriptor_address });
    }

    DX12InputLayout::DX12InputLayout(const DX12Context* context) :
        _context(context)
    {
    }

    bool DX12InputLayout::initialize(const VertexAttributeDescArray& vertex_attribute_descs)
    {
        _vertex_attribute_descs.resize(vertex_attribute_descs.size());
        for (uint32_t ix = 0; ix < _vertex_attribute_descs.size(); ++ix)
        {
            auto& attribute_desc = _vertex_attribute_descs[ix];
            attribute_desc = vertex_attribute_descs[ix];
            
            if (attribute_desc.array_size == 0)
            {
                std::stringstream ss;
                ss << "Create DX12InputLayout failed for VertexAttributeDesc.array_size = " << attribute_desc.array_size << "";
                LOG_ERROR(ss.str());
                return false;
            }

            const auto& crFormatMapping = get_dxgi_format_mapping(attribute_desc.format);
            const auto& crFormatInfo = get_format_info(attribute_desc.format);

            for (uint32_t semantic_index = 0; semantic_index < attribute_desc.array_size; ++semantic_index)
            {
                D3D12_INPUT_ELEMENT_DESC& d3d12_input_element_desc = d3d12_input_element_descs.emplace_back();
                d3d12_input_element_desc.SemanticName = attribute_desc.name.c_str();
                d3d12_input_element_desc.SemanticIndex = semantic_index;
                d3d12_input_element_desc.InputSlot = attribute_desc.buffer_index;
                d3d12_input_element_desc.Format = crFormatMapping.srv_format;
                d3d12_input_element_desc.AlignedByteOffset = attribute_desc.offset + semantic_index * crFormatInfo.byte_size_per_pixel;

                if (attribute_desc.is_instanced)
                {
                    d3d12_input_element_desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                    d3d12_input_element_desc.InstanceDataStepRate = 1; 
                }
                else 
                {
                    d3d12_input_element_desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                    d3d12_input_element_desc.InstanceDataStepRate = 0;
                }
            }

            if (!slot_strides.contains(attribute_desc.buffer_index))
            {
                slot_strides[attribute_desc.buffer_index] = attribute_desc.element_stride;
            }
            else 
            {
                if (slot_strides[attribute_desc.buffer_index] != attribute_desc.element_stride)
                {
                    std::stringstream ss;
                    ss  << "Create DX12InputLayout failed for "
                        <<"m_SlotStrideMap[rAttributeDesc.buffer_index]: " << slot_strides[attribute_desc.buffer_index]
                        << " != rAttributeDesc.element_stride: " << attribute_desc.element_stride << "";
                    LOG_ERROR(ss.str());
                    return false;
                }
            }
        }
        return true;
    }


    uint32_t DX12InputLayout::get_attributes_num() const
    {
        return static_cast<uint32_t>(_vertex_attribute_descs.size());
    }

    const VertexAttributeDesc& DX12InputLayout::get_attribute_desc(uint32_t attribute_index) const
    {
        assert(attribute_index < static_cast<uint32_t>(m_VertexAttributeDescs.size()));
        return _vertex_attribute_descs[attribute_index];
    }


    D3D12_INPUT_LAYOUT_DESC DX12InputLayout::GetD3D12InputLayoutDesc() const
    {
        D3D12_INPUT_LAYOUT_DESC ret;
        ret.NumElements = static_cast<UINT>(d3d12_input_element_descs.size());
        ret.pInputElementDescs = d3d12_input_element_descs.data();
        return ret;
    }


    DX12BindingLayout::DX12BindingLayout(const DX12Context* context, const BindingLayoutDesc& desc) :
        _context(context), _desc(desc)
    {
    }

    bool DX12BindingLayout::initialize()
    {
        ResourceViewType current_resource_type = ResourceViewType(-1);
        uint32_t current_slot = ~0u;

        D3D12_ROOT_CONSTANTS d3d12_root_constants = {};    // 只允许一个 PushConstants

        for (const auto& binding : _desc.binding_layout_items)
        {
            if (binding.type == ResourceViewType::PushConstants)
            {
                push_constant_size = binding.size;
                d3d12_root_constants.Num32BitValues = binding.size / 4;
                d3d12_root_constants.RegisterSpace = _desc.register_space;
                d3d12_root_constants.ShaderRegister = binding.slot;
            }
            else if (binding.type == ResourceViewType::VolatileConstantBuffer)
            {
                D3D12_ROOT_DESCRIPTOR1 d3d12_root_descriptor;
                d3d12_root_descriptor.RegisterSpace = _desc.register_space;
                d3d12_root_descriptor.ShaderRegister = binding.slot;

                /**
                 * @brief       Volatile CBs are static descriptors, however strange that may seem.
                                A volatile CB can only be bound to a command list after it's been written into, and 
                                after that the data will not change until the command list has finished executing.
                                Subsequent writes will be made into a newly allocated portion of an upload buffer.
                 * 
                 */
                d3d12_root_descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
                descriptor_volatile_constant_buffers.push_back(std::make_pair(-1, d3d12_root_descriptor));
            }
            else if (!are_resource_types_compatible(binding.type, current_resource_type) || binding.slot != current_slot + 1)
            {
                // If resource type changes or resource binding slot changes, then start a new range. 

                if (binding.type == ResourceViewType::Sampler)
                {
                    auto& range = d3d12_descriptor_sampler_ranges.emplace_back();
                    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
                    range.NumDescriptors = 1;
                    range.BaseShaderRegister = binding.slot;
                    range.RegisterSpace = _desc.register_space;
                    range.OffsetInDescriptorsFromTableStart = descriptor_table_sampler_size++;
                }
                else 
                {
                    auto& range = d3d12_descriptor_srv_etc_ranges.emplace_back();

                    switch (binding.type) 
                    {
                    case ResourceViewType::Texture_SRV:
                    case ResourceViewType::TypedBuffer_SRV:
                    case ResourceViewType::StructuredBuffer_SRV:
                    case ResourceViewType::RawBuffer_SRV:
                        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                        break;

                    case ResourceViewType::Texture_UAV:
                    case ResourceViewType::TypedBuffer_UAV:
                    case ResourceViewType::StructuredBuffer_UAV:
                    case ResourceViewType::RawBuffer_UAV:
                        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                        break;

                    case ResourceViewType::ConstantBuffer:
                        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                        break;

                    case ResourceViewType::None:
                    case ResourceViewType::VolatileConstantBuffer:
                    case ResourceViewType::Sampler:
                    case ResourceViewType::PushConstants:
                    case ResourceViewType::Count:
                        assert(!"invalid Enumeration value");
                        continue;
                    }

                    range.BaseShaderRegister = binding.slot;
                    range.RegisterSpace = _desc.register_space;
                    range.NumDescriptors = 1;
                    range.OffsetInDescriptorsFromTableStart = descriptor_table_srv_etc_size++;

                    /**
                     * @brief       We don't know how apps will use resources referenced in a binding set. 
                                    They may bind a buffer to the command list and then copy data into it.  
                     * 
                     */
                    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

                    _srv_etc_binding_layouts.push_back(binding);
                }

                current_slot++;
                current_resource_type = binding.type;
            }
            else 
            {
                // The resource type doesn't change or resource binding slot doesn't change, 
                // then extend the current range. 
                if (binding.type == ResourceViewType::Sampler)
                {
                    if (d3d12_descriptor_sampler_ranges.empty())
                    {
                        LOG_ERROR("Create DX12BindingLayout failed because m_DescriptorSamplaerRanges is empty.");
                        return false;
                    }
                    auto& range = d3d12_descriptor_sampler_ranges.back();
                    range.NumDescriptors++;
                    descriptor_table_sampler_size++;
                }
                else 
                {
                    if (d3d12_descriptor_srv_etc_ranges.empty())
                    {
                        LOG_ERROR("Create DX12BindingLayout failed because m_DescriptorSamplaerRanges is empty.");
                        return false;
                    } 
                    auto& range = d3d12_descriptor_srv_etc_ranges.back();
                    range.NumDescriptors++;
                    descriptor_table_srv_etc_size++;
                    _srv_etc_binding_layouts.push_back(binding);
                }

                current_slot = binding.slot;
            }
        }

        // A PipelineBindingLayout occupies a contiguous segment of a root signature.
        // The root parameter indices stored here are relative to the beginning of that segment, not to the RS item 0.

        D3D12_SHADER_VISIBILITY shader_visibility = convert_shader_stage(_desc.shader_visibility);
        d3d12_root_parameters.clear();

        if (d3d12_root_constants.Num32BitValues > 0)
        {
            auto& rRootParameter = d3d12_root_parameters.emplace_back();
            rRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            rRootParameter.ShaderVisibility = shader_visibility;
            rRootParameter.Constants = d3d12_root_constants;

            root_param_push_constant_index = static_cast<uint32_t>(d3d12_root_parameters.size()) - 1;
        }

        for (auto& rVolatileCB : descriptor_volatile_constant_buffers)
        {
            auto& rRootParameter = d3d12_root_parameters.emplace_back();
            rRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            rRootParameter.ShaderVisibility = shader_visibility;
            rRootParameter.Descriptor = rVolatileCB.second;

            rVolatileCB.first = static_cast<uint32_t>(d3d12_root_parameters.size()) - 1;
        }

        if (descriptor_table_srv_etc_size > 0)
        {
            auto& rRootParameter = d3d12_root_parameters.emplace_back();
            rRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rRootParameter.ShaderVisibility = shader_visibility;
            rRootParameter.DescriptorTable.NumDescriptorRanges = static_cast<uint32_t>(d3d12_descriptor_srv_etc_ranges.size());
            rRootParameter.DescriptorTable.pDescriptorRanges = d3d12_descriptor_srv_etc_ranges.data();

            root_param_srv_etc_index = static_cast<uint32_t>(d3d12_root_parameters.size()) - 1;
        }

        if (descriptor_table_sampler_size > 0)
        {
            auto& rRootParameter = d3d12_root_parameters.emplace_back();
            rRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rRootParameter.ShaderVisibility = shader_visibility;
            rRootParameter.DescriptorTable.NumDescriptorRanges = static_cast<uint32_t>(d3d12_descriptor_sampler_ranges.size());
            rRootParameter.DescriptorTable.pDescriptorRanges = d3d12_descriptor_sampler_ranges.data();

            root_param_sampler_index = static_cast<uint32_t>(d3d12_root_parameters.size()) - 1;
        }

        return true;
    }

    DX12BindlessLayout::DX12BindlessLayout(const DX12Context* context, const BindlessLayoutDesc& desc) :
        _context(context), _desc(desc), root_parameter()
    {
    }
    
    bool DX12BindlessLayout::initialize()
    {
        for (const auto& binding : _desc.binding_layout_items)
        {
            auto& range = _descriptor_ranges.emplace_back();

            switch (binding.type)
            {
            case ResourceViewType::Texture_SRV: 
            case ResourceViewType::TypedBuffer_SRV:
            case ResourceViewType::StructuredBuffer_SRV:
            case ResourceViewType::RawBuffer_SRV:
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                break;

            case ResourceViewType::ConstantBuffer:
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                break;

            case ResourceViewType::Texture_UAV:
            case ResourceViewType::TypedBuffer_UAV:
            case ResourceViewType::StructuredBuffer_UAV:
            case ResourceViewType::RawBuffer_UAV:
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                break;

            case ResourceViewType::Sampler:
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                break;

            case ResourceViewType::None:
            case ResourceViewType::VolatileConstantBuffer:
            case ResourceViewType::PushConstants:
            case ResourceViewType::Count:
                assert(!"invalid Enumeration value");
                continue;
            }

            range.NumDescriptors = ~0u;    // Unbounded. 
            range.BaseShaderRegister = _desc.first_slot;
            range.RegisterSpace = binding.register_space;
            range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
            range.OffsetInDescriptorsFromTableStart = 0;
        }

        root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_parameter.ShaderVisibility = convert_shader_stage(_desc.shader_visibility);
        root_parameter.DescriptorTable.NumDescriptorRanges = static_cast<uint32_t>(_descriptor_ranges.size());
        root_parameter.DescriptorTable.pDescriptorRanges = _descriptor_ranges.data();
        
        return true;
    }

    DX12RootSignature::DX12RootSignature(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps) :
        _context(context), _descriptor_heaps(descriptor_heaps)
    {
    }

    DX12RootSignature::~DX12RootSignature() noexcept
    {
        auto iter = _descriptor_heaps->dx12_root_signatures.find(hash_index);
        if (iter != _descriptor_heaps->dx12_root_signatures.end())
        {
            _descriptor_heaps->dx12_root_signatures.erase(iter);
        }
    }

    bool DX12RootSignature::initialize(
        const BindingLayoutInterfaceArray& binding_layouts,
        bool allow_input_layout,
        const D3D12_ROOT_PARAMETER1* custom_parameters,
        uint32_t custom_parameter_count
    )
    {
        std::vector<D3D12_ROOT_PARAMETER1> d3d12_root_parameters(
            &custom_parameters[0], 
            &custom_parameters[custom_parameter_count]
        );

        for (uint32_t ix = 0; ix < binding_layouts.size(); ++ix)
        {
            uint32_t offset = static_cast<uint32_t>(d3d12_root_parameters.size());
            binding_layout_map.emplace_back(std::make_pair(binding_layouts[ix], offset));
            

            if (binding_layouts[ix]->is_binding_less())
            {
                DX12BindlessLayout* dx12_bindless_layout = check_cast<DX12BindlessLayout*>(binding_layouts[ix]);

                d3d12_root_parameters.push_back(dx12_bindless_layout->root_parameter);
            }
            else
            {
                DX12BindingLayout* dx12_binding_layout = check_cast<DX12BindingLayout*>(binding_layouts[ix]);

                d3d12_root_parameters.insert(
                    d3d12_root_parameters.end(),
                    dx12_binding_layout->d3d12_root_parameters.begin(),
                    dx12_binding_layout->d3d12_root_parameters.end()
                );

                if (dx12_binding_layout->push_constant_size != 0)
                {
                    push_constant_size = dx12_binding_layout->push_constant_size;
					root_param_push_constant_index = dx12_binding_layout->root_param_push_constant_index + offset;
				}
            }
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {};
        desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (allow_input_layout) desc.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        if (!d3d12_root_parameters.empty())
        {
            desc.Desc_1_1.pParameters = d3d12_root_parameters.data();
            desc.Desc_1_1.NumParameters = static_cast<uint32_t>(d3d12_root_parameters.size());
        }

        Microsoft::WRL::ComPtr<ID3DBlob> root_signature_blob;
        Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

        if (FAILED(D3D12SerializeVersionedRootSignature(&desc, root_signature_blob.GetAddressOf(), error_blob.GetAddressOf())))
        {
            std::stringstream ss("D3D12SerializeVersionedRootSignature call failed.\n");
            ss << static_cast<char*>(error_blob->GetBufferPointer());
            LOG_ERROR(ss.str());
            return false;
        }

        if (FAILED(_context->device->CreateRootSignature(
            0, 
            root_signature_blob->GetBufferPointer(), 
            root_signature_blob->GetBufferSize(), 
            IID_PPV_ARGS(d3d12_root_signature.GetAddressOf())
        )))
        {
            LOG_ERROR("ID3D12Device::CreateRootSignature call failed.");
            return false;
        }
        return true;
    }

    DX12GraphicsPipeline::DX12GraphicsPipeline(
        const DX12Context* context,
        const GraphicsPipelineDesc& desc,
        DX12RootSignature* dx12_root_signature,
        const FrameBufferInfo& crFrameBufferInfo
    ) :
        _context(context), 
        _desc(desc), 
        dx12_root_signature(dx12_root_signature), 
        _frame_buffer_info(crFrameBufferInfo),
        require_blend_factor(desc.render_state.blend_state.if_use_constant_color(crFrameBufferInfo.rtv_formats.size()))
    {
    }

    bool DX12GraphicsPipeline::initialize()
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{};
        pipeline_state_desc.SampleMask = ~0u;

        pipeline_state_desc.pRootSignature = dx12_root_signature->d3d12_root_signature.Get();

        if (_desc.vertex_shader != nullptr)
        {
            ShaderByteCode data = _desc.vertex_shader->get_byte_code();
            if (!data.is_valid())
            {
                ShaderDesc desc = _desc.vertex_shader->get_desc();
                std::stringstream ss;
                ss << "Load vertex shader: " << desc.name << " failed.";
                LOG_ERROR(ss.str().c_str());
                return false;
            }
            pipeline_state_desc.VS = { .pShaderBytecode = data.byte_code, .BytecodeLength = data.size };
        }
        if (_desc.hull_shader != nullptr)
        {
            ShaderByteCode data = _desc.hull_shader->get_byte_code();
            if (!data.is_valid())
            {
                ShaderDesc desc = _desc.hull_shader->get_desc();
                std::stringstream ss;
                ss << "Load hull shader: " << desc.name << " failed.";
                LOG_ERROR(ss.str().c_str());
                return false;
            }
            pipeline_state_desc.HS = { .pShaderBytecode = data.byte_code, .BytecodeLength = data.size };
        }
        if (_desc.domain_shader != nullptr)
        {
            ShaderByteCode data = _desc.domain_shader->get_byte_code();
            if (!data.is_valid())
            {
                ShaderDesc desc = _desc.domain_shader->get_desc();
                std::stringstream ss;
                ss << "Load domain shader: " << desc.name << " failed.";
                LOG_ERROR(ss.str().c_str());
                return false;
            }
            pipeline_state_desc.DS = { .pShaderBytecode = data.byte_code, .BytecodeLength = data.size };
        }
        if (_desc.geometry_shader != nullptr)
        {
            ShaderByteCode data = _desc.geometry_shader->get_byte_code();
            if (!data.is_valid())
            {
                ShaderDesc desc = _desc.geometry_shader->get_desc();
                std::stringstream ss;
                ss << "Load geometry shader: " << desc.name << " failed.";
                LOG_ERROR(ss.str().c_str());
                return false;
            }
            pipeline_state_desc.GS = { .pShaderBytecode = data.byte_code, .BytecodeLength = data.size };
        }
        if (_desc.pixel_shader != nullptr)
        {
            ShaderByteCode data = _desc.pixel_shader->get_byte_code();
            if (!data.is_valid())
            {
                ShaderDesc desc = _desc.pixel_shader->get_desc();
                std::stringstream ss;
                ss << "Load pixel shader: " << desc.name << " failed.";
                LOG_ERROR(ss.str().c_str());
                return false;
            }
            pipeline_state_desc.PS = { .pShaderBytecode = data.byte_code, .BytecodeLength = data.size };
        }

        pipeline_state_desc.BlendState = convert_blend_state(_desc.render_state.blend_state);

        const DepthStencilState& depth_stencil_state = _desc.render_state.depth_stencil_state;
        pipeline_state_desc.DepthStencilState = convert_depth_stencil_state(depth_stencil_state);

        if ((depth_stencil_state.enable_depth_test || depth_stencil_state.enable_stencil) && _frame_buffer_info.depth_format == Format::UNKNOWN)
        {
            pipeline_state_desc.DepthStencilState.DepthEnable = false;
            pipeline_state_desc.DepthStencilState.StencilEnable = false;

            LOG_WARN("DepthEnable or stencilEnable is true, but no depth target is bound.");
        }

        pipeline_state_desc.RasterizerState = convert_rasterizer_state(_desc.render_state.raster_state);

        switch (_desc.PrimitiveType)
        {
        case PrimitiveType::PointList: pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
        case PrimitiveType::LineList: pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;

        case PrimitiveType::TriangleList: 
        case PrimitiveType::TriangleStrip: 
        case PrimitiveType::TriangleListWithAdjacency:
        case PrimitiveType::TriangleStripWithAdjacency:
            pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
            
        case PrimitiveType::PatchList: pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH; break;
        }
        
        pipeline_state_desc.SampleDesc = { _frame_buffer_info.sample_count, _frame_buffer_info.sample_quality };

        pipeline_state_desc.DSVFormat = get_dxgi_format_mapping(_frame_buffer_info.depth_format).rtv_format;
        pipeline_state_desc.NumRenderTargets = _frame_buffer_info.rtv_formats.size();
        for (uint32_t ix = 0; ix < _frame_buffer_info.rtv_formats.size(); ++ix)
        {
            pipeline_state_desc.RTVFormats[ix] = get_dxgi_format_mapping(_frame_buffer_info.rtv_formats[ix]).rtv_format;
        }

        if (_desc.input_layout != nullptr)
        {
            DX12InputLayout* dx12_input_layout = check_cast<DX12InputLayout*>(_desc.input_layout);

            pipeline_state_desc.InputLayout = dx12_input_layout->GetD3D12InputLayoutDesc();

            if (pipeline_state_desc.InputLayout.NumElements == 0) pipeline_state_desc.InputLayout.pInputElementDescs = nullptr;
        }

        if (FAILED(_context->device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(_d3d12_pipeline_state.GetAddressOf()))))
        {
            LOG_ERROR("Failed to create D3D12 graphics pipeline state.");
            return false;
        }
        return true;
    }

    DX12ComputePipeline::DX12ComputePipeline(const DX12Context* context, const ComputePipelineDesc& desc, DX12RootSignature* dx12_root_signature) :
        _context(context), _desc(desc), dx12_root_signature(dx12_root_signature)
    {
    }

    bool DX12ComputePipeline::initialize()
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_state_desc{};

        pipeline_state_desc.pRootSignature = dx12_root_signature->d3d12_root_signature.Get();
        
        if (_desc.compute_shader == nullptr) 
        {
            LOG_ERROR("Compute shader is missing.");
            return false;
        }
        else
        {
            ShaderByteCode byte_code = _desc.compute_shader->get_byte_code();
            if (!byte_code.is_valid())
            {
                ShaderDesc desc = _desc.compute_shader->get_desc();
                std::stringstream ss;
                ss << "Load compute shader: " << desc.name << " failed.";
                LOG_ERROR(ss.str().c_str());
                return false;
            }
            pipeline_state_desc.CS = { .pShaderBytecode = byte_code.byte_code, .BytecodeLength = byte_code.size };
        }
        
        if (FAILED(_context->device->CreateComputePipelineState(&pipeline_state_desc, IID_PPV_ARGS(_d3d12_pipeline_state.GetAddressOf()))))
        {
            LOG_ERROR("Failed to create D3D12 compute pipeline state.");
            return false;
        }
        return true;
    }
    

    DX12BindingSet::DX12BindingSet(
        const DX12Context* context,
        DX12DescriptorHeaps* descriptor_heaps,
        const BindingSetDesc& desc,
        BindingLayoutInterface* binding_layout
    ) :
        _context(context), 
        _descriptor_heaps(descriptor_heaps), 
        _desc(desc), 
        _binding_layout(binding_layout)
    {
    }

    bool DX12BindingSet::initialize()
    {
        // 下面都是为各个 resource 创建 view

        DX12BindingLayout* dx12_binding_layout = check_cast<DX12BindingLayout*>(_binding_layout);

        for (const auto& [index, crD3D12RootDescriptor] : dx12_binding_layout->descriptor_volatile_constant_buffers)
        {
            BufferInterface* pFoundBuffer;
            for (const auto& binding : _desc.binding_items)
            {
                if (binding.type == ResourceViewType::VolatileConstantBuffer && binding.slot == crD3D12RootDescriptor.ShaderRegister)
                {
                    _resources.push_back(binding.resource);
                    pFoundBuffer = static_cast<BufferInterface*>(binding.resource.get());
                    break;
                }
            }
            root_param_index_volatile_constant_buffers.push_back(std::make_pair(index, pFoundBuffer));
        }

        if (dx12_binding_layout->descriptor_table_srv_etc_size > 0)
        {
            descriptor_table_srv_etc_base_index = _descriptor_heaps->shader_resource_heap.allocate_descriptors(dx12_binding_layout->descriptor_table_srv_etc_size);
            root_param_srv_etc_index = dx12_binding_layout->root_param_srv_etc_index;
            is_descriptor_table_srv_etc_valid = true;

            for (const auto& range : dx12_binding_layout->d3d12_descriptor_srv_etc_ranges)
            {
                for (uint32_t ix = 0; ix < range.NumDescriptors; ++ix)
                {
                    uint32_t slot = range.BaseShaderRegister + ix;
                    D3D12_CPU_DESCRIPTOR_HANDLE d3d12_descriptor_handle = _descriptor_heaps->shader_resource_heap.get_cpu_handle(
                        descriptor_table_srv_etc_base_index + range.OffsetInDescriptorsFromTableStart + ix
                    );

                    bool found = false;
                    std::shared_ptr<ResourceInterface> resource = nullptr;
                    for (uint32_t jx = 0; jx < _desc.binding_items.size(); ++jx)
                    {
                        const auto& binding = _desc.binding_items[jx];
                        if (binding.slot != slot) continue;

                        ResourceViewType type = get_normalized_resource_type(binding.type);
                        if (type == ResourceViewType::TypedBuffer_SRV && range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
                        {
                            if (binding.resource)
                            {
                                DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(binding.resource.get());
                                dx12_buffer->create_srv(d3d12_descriptor_handle.ptr, binding.format, binding.range, binding.type);

                                resource = binding.resource;
                                bindings_which_need_transition.push_back(static_cast<uint16_t>(jx));
                            }
                            else 
                            {
                                create_null_buffer_srv(d3d12_descriptor_handle.ptr, binding.format, _context);
                                LOG_WARN("There is no resource binding to set, it will create a null view.");
                            }
                            found = true;
                            break;
                        }
                        else if (type == ResourceViewType::TypedBuffer_UAV && range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
                        {
                            if (binding.resource)
                            {
                                DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(binding.resource.get());
                                dx12_buffer->create_uav(d3d12_descriptor_handle.ptr, binding.format, binding.range, binding.type);

                                resource = binding.resource;
                                bindings_which_need_transition.push_back(static_cast<uint16_t>(jx));
                            }
                            else 
                            {
                                CreateNullBufferUAV(d3d12_descriptor_handle.ptr, binding.format, _context);
                                LOG_WARN("There is no resource binding to set, it will create a null view.");
                            }
                            has_uav_bingings = true;
                            found = true;
                            break;
                        }
                        else if (type == ResourceViewType::Texture_SRV && range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
                        {
                            DX12Texture* dx12_texture = check_cast<DX12Texture*>(binding.resource.get());
                            dx12_texture->create_srv(d3d12_descriptor_handle.ptr, binding.format, binding.dimension, binding.subresource);

                            resource = binding.resource;
                            bindings_which_need_transition.push_back(static_cast<uint16_t>(jx));
                            found = true;
                            break;
                        }
                        else if (type == ResourceViewType::Texture_UAV && range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
                        {
                            DX12Texture* dx12_texture = check_cast<DX12Texture*>(binding.resource.get());
                            dx12_texture->create_uav(d3d12_descriptor_handle.ptr, binding.format, binding.dimension, binding.subresource);

                            resource = binding.resource;
                            bindings_which_need_transition.push_back(static_cast<uint16_t>(jx));
                            has_uav_bingings = true;
                            found = true;
                            break;
                        }
                        else if (type == ResourceViewType::ConstantBuffer && range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
                        {
                            DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(binding.resource.get());
                            dx12_buffer->create_cbv(d3d12_descriptor_handle.ptr, binding.range);

                            resource = binding.resource;

                            if (dx12_buffer->get_desc().is_volatile)
                            {
                                LOG_ERROR("Attempted to bind a volatile constant buffer to a non-volatile CB layout.");
                                return false;
                            }
                            else 
                            {
                                bindings_which_need_transition.push_back(static_cast<uint16_t>(jx));
                            }
                            found = true;
                            break;
                        }
                        else
                        {
                            continue;
                        }
                    }

                    if (resource) _resources.push_back(resource);

                    if (!found) return false;
                }
            }

            _descriptor_heaps->shader_resource_heap.copy_to_shader_visible_heap(descriptor_table_srv_etc_base_index, dx12_binding_layout->descriptor_table_srv_etc_size);
        }

        if (dx12_binding_layout->descriptor_table_sampler_size > 0)
        {
            descriptor_table_sampler_base_index = _descriptor_heaps->sampler_heap.allocate_descriptors(dx12_binding_layout->descriptor_table_sampler_size);
            root_param_sampler_index = dx12_binding_layout->root_param_sampler_index;
            is_descriptor_table_sampler_valid = true;

            for (const auto& range : dx12_binding_layout->d3d12_descriptor_sampler_ranges)
            {
                for (uint32_t ix = 0; ix < range.NumDescriptors; ++ix)
                {
                    uint32_t slot = range.BaseShaderRegister + ix;
                    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle = _descriptor_heaps->sampler_heap.get_cpu_handle(
                        descriptor_table_sampler_base_index + range.OffsetInDescriptorsFromTableStart + ix
                    );

                    bool found = false;

                    for (const auto& binding : _desc.binding_items)
                    {
                        if (binding.type == ResourceViewType::Sampler && binding.slot == slot)
                        {
                            std::shared_ptr<DX12Sampler> dx12_sampler = check_cast<DX12Sampler>(binding.resource);
                            dx12_sampler->create_descriptor(DescriptorHandle.ptr);

                            _resources.push_back(binding.resource);
                            found = true;
                            break;
                        }
                    }
                    if (!found) return false;
                }
            }

            _descriptor_heaps->sampler_heap.copy_to_shader_visible_heap(descriptor_table_sampler_base_index, dx12_binding_layout->descriptor_table_sampler_size);
        }

        return true;
    }


    DX12BindingSet::~DX12BindingSet() noexcept
    {
        DX12BindingLayout* dx12_binding_layout = check_cast<DX12BindingLayout*>(_binding_layout);
        _descriptor_heaps->shader_resource_heap.release_descriptors(descriptor_table_srv_etc_base_index, dx12_binding_layout->descriptor_table_srv_etc_size);
        _descriptor_heaps->sampler_heap.release_descriptors(descriptor_table_sampler_base_index, dx12_binding_layout->descriptor_table_sampler_size);
    }

    DX12BindlessSet::DX12BindlessSet(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps) :
        _context(context), _descriptor_heaps(descriptor_heaps)
    {
    }

    bool DX12BindlessSet::initialize()
    {
        return true;
    }

	bool DX12BindlessSet::set_slot(const BindingSetItem& item)
	{
		if (item.slot >= capacity) return false;

		D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = _descriptor_heaps->shader_resource_heap.get_cpu_handle(first_descriptor_index + item.slot);

		switch (item.type)
		{
		case ResourceViewType::Texture_SRV:
		{
			DX12Texture* dx12_texture = check_cast<DX12Texture*>(item.resource.get());
			dx12_texture->create_srv(descriptor_handle.ptr, item.format, item.dimension, item.subresource);
			break;
		}
		case ResourceViewType::Texture_UAV:
		{
			DX12Texture* dx12_texture = check_cast<DX12Texture*>(item.resource.get());
			dx12_texture->create_uav(descriptor_handle.ptr, item.format, item.dimension, item.subresource);
			break;
		}
		case ResourceViewType::TypedBuffer_SRV:
		case ResourceViewType::StructuredBuffer_SRV:
		case ResourceViewType::RawBuffer_SRV:
		{
			DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(item.resource.get());
			dx12_buffer->create_srv(descriptor_handle.ptr, item.format, item.range, item.type);
			break;
		}
		case ResourceViewType::TypedBuffer_UAV:
		case ResourceViewType::StructuredBuffer_UAV:
		case ResourceViewType::RawBuffer_UAV:
		{
			DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(item.resource.get());
			dx12_buffer->create_uav(descriptor_handle.ptr, item.format, item.range, item.type);
			break;
		}
		case ResourceViewType::ConstantBuffer:
		{
			DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(item.resource.get());
			dx12_buffer->create_cbv(descriptor_handle.ptr, item.range);
			break;
		}

		case ResourceViewType::VolatileConstantBuffer:
			assert(!"Attempted to bind a volatile constant buffer to a bindless set.");

		default:
			assert(!"invalid Enumeration value");
			return false;
		}

		_descriptor_heaps->shader_resource_heap.copy_to_shader_visible_heap(first_descriptor_index + item.slot);

		return true;
	}

	void DX12BindlessSet::resize(uint32_t new_size, bool keep_contents)
	{
		if (new_size == capacity) return;

		uint32_t old_capacity = capacity;

		uint32_t old_first_view_index = first_descriptor_index;

		if (new_size <= old_capacity)
		{
			_descriptor_heaps->shader_resource_heap.release_descriptors(old_first_view_index + new_size, old_capacity - new_size);
			capacity = new_size;
			return;
		}
        if (!keep_contents && old_capacity > 0)
        {
            _descriptor_heaps->shader_resource_heap.release_descriptors(old_first_view_index, old_capacity);
        }

        uint32_t new_first_view_index = _descriptor_heaps->shader_resource_heap.allocate_descriptors(new_size);
        first_descriptor_index = new_first_view_index;

        if (keep_contents && old_capacity > 0)
        {
            _context->device->CopyDescriptorsSimple(
                old_capacity,
                _descriptor_heaps->shader_resource_heap.get_cpu_handle(new_first_view_index),
                _descriptor_heaps->shader_resource_heap.get_cpu_handle(old_first_view_index),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            );
            _context->device->CopyDescriptorsSimple(
                old_capacity,
                _descriptor_heaps->shader_resource_heap.get_shader_visible_cpu_handle(new_first_view_index),
                _descriptor_heaps->shader_resource_heap.get_cpu_handle(old_first_view_index),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            );

            _descriptor_heaps->shader_resource_heap.release_descriptors(old_first_view_index, old_capacity);
        }

        capacity = new_size;
	}

	DX12BindlessSet::~DX12BindlessSet() noexcept
    {
        _descriptor_heaps->shader_resource_heap.release_descriptors(first_descriptor_index, capacity);
    }




}