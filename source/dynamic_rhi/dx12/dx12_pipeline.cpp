#include "dx12_pipeline.h"
#include "dx12_convert.h"
#include "dx12_binding.h"
#include <cassert>
#include <combaseapi.h>
#include <cstdint>
#include <d3d12.h>
#include <d3dcommon.h>
#include <intsafe.h>
#include <memory>
#include <sstream>
#include "../../core/tools/check_cast.h"

namespace fantasy 
{
    bool create_d3d12_root_signature(
        const DX12Context* context,
        const BindingLayoutInterfaceArray& binding_layouts,
        bool allow_input_layout,
        uint32_t& out_push_constant_size,
        uint32_t& out_root_param_push_constant_index,
        Microsoft::WRL::ComPtr<ID3D12RootSignature>& out_signature
    )
    {
        std::vector<D3D12_ROOT_PARAMETER1> d3d12_root_parameters;

        for (uint32_t ix = 0; ix < binding_layouts.size(); ++ix)
        {
            uint32_t offset = static_cast<uint32_t>(d3d12_root_parameters.size());

            if (binding_layouts[ix]->is_binding_less())
            {
                auto bindless_layout = check_cast<DX12BindlessLayout>(binding_layouts[ix]);

                bindless_layout->root_param_index = ix;

                d3d12_root_parameters.push_back(bindless_layout->root_parameter);
            }
            else
            {
                auto binding_layout = check_cast<DX12BindingLayout>(binding_layouts[ix]);

                uint32_t offset = static_cast<uint32_t>(d3d12_root_parameters.size());

                if (binding_layout->srv_root_param_start_index != INVALID_SIZE_32) 
                    binding_layout->srv_root_param_start_index += offset;
                if (binding_layout->sampler_root_param_start_index != INVALID_SIZE_32) 
                    binding_layout->sampler_root_param_start_index += offset;

                d3d12_root_parameters.insert(
                    d3d12_root_parameters.end(),
                    binding_layout->d3d12_root_parameters.begin(),
                    binding_layout->d3d12_root_parameters.end()
                );

                if (binding_layout->push_constant_size != 0)
                {
                    out_push_constant_size = binding_layout->push_constant_size;
					out_root_param_push_constant_index = binding_layout->push_constant_root_param_index + offset;
				}
            }
        }

        ReturnIfFalse(!d3d12_root_parameters.empty());

        D3D12_ROOT_SIGNATURE_DESC1 d3d12_root_signature_desc{};
        d3d12_root_signature_desc.NumParameters = static_cast<uint32_t>(d3d12_root_parameters.size());
        d3d12_root_signature_desc.pParameters = d3d12_root_parameters.data();
        d3d12_root_signature_desc.NumStaticSamplers = 0;
        d3d12_root_signature_desc.pStaticSamplers = nullptr;
        if (allow_input_layout) d3d12_root_signature_desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        Microsoft::WRL::ComPtr<ID3DBlob> root_signature_blob;
        Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
        
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3d12_vertsioned_root_signature_desc{};
        d3d12_vertsioned_root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        d3d12_vertsioned_root_signature_desc.Desc_1_1 = d3d12_root_signature_desc;

        if (
            FAILED(D3D12SerializeVersionedRootSignature(
                &d3d12_vertsioned_root_signature_desc, 
                root_signature_blob.GetAddressOf(), 
                error_blob.GetAddressOf()
            ))
        )
        {
            std::stringstream ss("D3D12 root signature initialize failed.\n");
            ss << static_cast<char*>(error_blob->GetBufferPointer());
            LOG_ERROR(ss.str());
            return false;
        }

        return SUCCEEDED(context->device->CreateRootSignature(
            0, 
            root_signature_blob->GetBufferPointer(), 
            root_signature_blob->GetBufferSize(), 
            IID_PPV_ARGS(out_signature.GetAddressOf())
        ));
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
            
            ReturnIfFalse(attribute_desc.array_size != 0);

            const auto& format_mapping = get_dxgi_format_mapping(attribute_desc.format);
            const auto& format_info = get_format_info(attribute_desc.format);

            for (uint32_t semantic_index = 0; semantic_index < attribute_desc.array_size; ++semantic_index)
            {
                D3D12_INPUT_ELEMENT_DESC& d3d12_input_element_desc = _d3d12_input_element_descs.emplace_back();
                d3d12_input_element_desc.SemanticName = attribute_desc.name.c_str();
                d3d12_input_element_desc.InputSlot = attribute_desc.buffer_slot;
                d3d12_input_element_desc.Format = format_mapping.srv_format;

                d3d12_input_element_desc.SemanticIndex = semantic_index;
                d3d12_input_element_desc.AlignedByteOffset = attribute_desc.offset + semantic_index * format_info.size;

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

            if (!slot_strides.contains(attribute_desc.buffer_slot))
            {
                slot_strides[attribute_desc.buffer_slot] = attribute_desc.element_stride;
            }
            else 
            {
                ReturnIfFalse(slot_strides[attribute_desc.buffer_slot] == attribute_desc.element_stride)
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
        assert(attribute_index < _vertex_attribute_descs.size());
        return _vertex_attribute_descs[attribute_index];
    }


    D3D12_INPUT_LAYOUT_DESC DX12InputLayout::GetD3D12InputLayoutDesc() const
    {
        D3D12_INPUT_LAYOUT_DESC ret;
        ret.NumElements = static_cast<UINT>(_d3d12_input_element_descs.size());
        ret.pInputElementDescs = _d3d12_input_element_descs.data();
        return ret;
    }

    DX12GraphicsPipeline::DX12GraphicsPipeline(const DX12Context* context, const GraphicsPipelineDesc& desc_) :
        _context(context), desc(desc_)
    {
    }

    bool DX12GraphicsPipeline::initialize(const FrameBufferInfo& frame_buffer_info)
    {
        ReturnIfFalse(create_d3d12_root_signature(
            _context,
            desc.binding_layouts,
            desc.input_layout != nullptr,
            push_constant_size,
            push_constant_root_param_index,
            d3d12_root_signature
        ));

        D3D12_SHADER_BYTECODE vs{}, hs{}, ds{}, gs{}, ps{};

        if (desc.vertex_shader)
        {
            const ShaderByteCode& data = desc.vertex_shader->get_byte_code();
            vs = { .pShaderBytecode = static_cast<void*>(data.byte_code), .BytecodeLength = data.size };
        }
        if (desc.hull_shader)
        {
            const ShaderByteCode& data = desc.hull_shader->get_byte_code();
            hs = { .pShaderBytecode = static_cast<void*>(data.byte_code), .BytecodeLength = data.size };
        }
        if (desc.domain_shader)
        {
            const ShaderByteCode& data = desc.domain_shader->get_byte_code();
            ds = { .pShaderBytecode = static_cast<void*>(data.byte_code), .BytecodeLength = data.size };
        }
        if (desc.geometry_shader)
        {
            const ShaderByteCode& data = desc.geometry_shader->get_byte_code();
            gs = { .pShaderBytecode = static_cast<void*>(data.byte_code), .BytecodeLength = data.size };
        }
        if (desc.pixel_shader)
        {
            const ShaderByteCode& data = desc.pixel_shader->get_byte_code();
            ps = { .pShaderBytecode = static_cast<void*>(data.byte_code), .BytecodeLength = data.size };
        }

        use_blend_constant = desc.render_state.blend_state.if_use_constant_color(frame_buffer_info.rtv_formats.size());

        
        D3D12_INPUT_LAYOUT_DESC d3d12_input_layout_desc{};
        if (desc.input_layout)
        {
            auto input_layout = check_cast<DX12InputLayout>(desc.input_layout);
            d3d12_input_layout_desc = input_layout->GetD3D12InputLayoutDesc();
        }

        // NodeMask: 对于单个 GPU 操作, 请将此设置为零.
        
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12_pipeline_state_desc{};
        d3d12_pipeline_state_desc.pRootSignature = d3d12_root_signature.Get();
        d3d12_pipeline_state_desc.VS = vs;
        d3d12_pipeline_state_desc.PS = ps;
        d3d12_pipeline_state_desc.DS = ds;
        d3d12_pipeline_state_desc.HS = hs;
        d3d12_pipeline_state_desc.GS = gs;
        d3d12_pipeline_state_desc.StreamOutput = D3D12_STREAM_OUTPUT_DESC{};
        d3d12_pipeline_state_desc.BlendState = convert_blend_state(desc.render_state.blend_state); 
        d3d12_pipeline_state_desc.SampleMask = ~0u;
        d3d12_pipeline_state_desc.RasterizerState = convert_rasterizer_state(desc.render_state.raster_state);
        d3d12_pipeline_state_desc.DepthStencilState = convert_depth_stencil_state(desc.render_state.depth_stencil_state);
        d3d12_pipeline_state_desc.InputLayout = d3d12_input_layout_desc;
        d3d12_pipeline_state_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE{};
        d3d12_pipeline_state_desc.PrimitiveTopologyType = convert_primitive_topology(desc.primitive_type);
        d3d12_pipeline_state_desc.NumRenderTargets = static_cast<uint32_t>(frame_buffer_info.rtv_formats.size());
        for (uint32_t ix = 0; ix < frame_buffer_info.rtv_formats.size(); ++ix)
        {
            d3d12_pipeline_state_desc.RTVFormats[ix] = get_dxgi_format_mapping(frame_buffer_info.rtv_formats[ix]).rtv_dsv_format;
        }
        d3d12_pipeline_state_desc.DSVFormat = get_dxgi_format_mapping(frame_buffer_info.depth_format).rtv_dsv_format;
        d3d12_pipeline_state_desc.SampleDesc = { 1, 0 };
        d3d12_pipeline_state_desc.NodeMask = 0;
        d3d12_pipeline_state_desc.CachedPSO = D3D12_CACHED_PIPELINE_STATE{};
        d3d12_pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        return SUCCEEDED(_context->device->CreateGraphicsPipelineState(
            &d3d12_pipeline_state_desc, 
            IID_PPV_ARGS(d3d12_pipeline_state.GetAddressOf())
        ));
    }

    const GraphicsPipelineDesc& DX12GraphicsPipeline::get_desc() const
    { 
        return desc; 
    }

    void* DX12GraphicsPipeline::get_native_object() 
    { 
        return d3d12_pipeline_state.Get(); 
    }

    DX12ComputePipeline::DX12ComputePipeline(const DX12Context* context, const ComputePipelineDesc& desc_) :
        _context(context), desc(desc_)
    {
    }

    bool DX12ComputePipeline::initialize()
    {
        ReturnIfFalse(create_d3d12_root_signature(
            _context, 
            desc.binding_layouts, 
            false, 
            push_constant_size, 
            push_constant_root_param_index, 
            d3d12_root_signature
        ));

        ReturnIfFalse(desc.compute_shader != nullptr);
        const ShaderByteCode& byte_code = desc.compute_shader->get_byte_code();

        D3D12_COMPUTE_PIPELINE_STATE_DESC d3d12_pipeline_state_desc{};
        d3d12_pipeline_state_desc.pRootSignature = d3d12_root_signature.Get();
        d3d12_pipeline_state_desc.CS = { .pShaderBytecode = byte_code.byte_code, .BytecodeLength = byte_code.size };
        d3d12_pipeline_state_desc.NodeMask = 0;
        d3d12_pipeline_state_desc.CachedPSO = D3D12_CACHED_PIPELINE_STATE{};
        d3d12_pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        
        return SUCCEEDED(_context->device->CreateComputePipelineState(
            &d3d12_pipeline_state_desc, 
            IID_PPV_ARGS(d3d12_pipeline_state.GetAddressOf())
        ));
    }

    const ComputePipelineDesc& DX12ComputePipeline::get_desc() const
    { 
        return desc; 
    }

    void* DX12ComputePipeline::get_native_object() 
    { 
        return d3d12_pipeline_state.Get(); 
    }
}