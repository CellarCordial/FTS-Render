#ifndef RHI_DX12_PIPELINE_H
#define RHI_DX12_PIPELINE_H

#include "../pipeline.h"
#include "../frame_buffer.h"
#include "dx12_forward.h"
#include <unordered_map>
#include <vector>

namespace fantasy
{
    class DX12InputLayout : public InputLayoutInterface
    {
    public:
        DX12InputLayout(const DX12Context* context);

        bool initialize(const VertexAttributeDescArray& vertex_attribute_descs);

        uint32_t get_attributes_num() const override;
        const VertexAttributeDesc& get_attribute_desc(uint32_t attribute_index) const override;
        
        D3D12_INPUT_LAYOUT_DESC GetD3D12InputLayoutDesc() const;

    public:
        // map<VertexAttributeDesc::buffer_slot, VertexAttributeDesc::element_stride>.
        std::unordered_map<uint32_t, uint32_t> slot_strides;
        
    private:
        const DX12Context* _context; 
        std::vector<VertexAttributeDesc> _vertex_attribute_descs;
        std::vector<D3D12_INPUT_ELEMENT_DESC> _d3d12_input_element_descs;
    };


    class DX12GraphicsPipeline : public GraphicsPipelineInterface
    {
    public:
        DX12GraphicsPipeline(const DX12Context* context, const GraphicsPipelineDesc& desc);

        bool initialize(const FrameBufferInfo& frame_buffer_info);

        const GraphicsPipelineDesc& get_desc() const override;
        void* get_native_object() override;

    public:
        GraphicsPipelineDesc desc;

        Microsoft::WRL::ComPtr<ID3D12PipelineState> d3d12_pipeline_state;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> d3d12_root_signature;

        uint32_t push_constant_size = 0;
        uint32_t push_constant_root_param_index = INVALID_SIZE_32;
        bool use_blend_constant = false;

    private:
        const DX12Context* _context;
    };

    
    class DX12ComputePipeline : public ComputePipelineInterface
    {
    public:
        DX12ComputePipeline(const DX12Context* context, const ComputePipelineDesc& desc);

        bool initialize();

        const ComputePipelineDesc& get_desc() const override;
        void* get_native_object() override;
    
    public:
        ComputePipelineDesc desc;

        Microsoft::WRL::ComPtr<ID3D12PipelineState> d3d12_pipeline_state;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> d3d12_root_signature;

        uint32_t push_constant_size = 0;
        uint32_t push_constant_root_param_index = INVALID_SIZE_32;
        
    private:
        const DX12Context* _context;
    };

}




















 #endif