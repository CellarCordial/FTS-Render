/**
 * *****************************************************************************
 * @file        DX12Pipeline.h
 * @brief       
 * @author      CellarCordial (591885295@qq.com)
 * @date        2024-06-01
 * @copyright Copyright (c) 2024
 * *****************************************************************************
 */

 #ifndef RHI_DX12_PIPELINE_H
 #define RHI_DX12_PIPELINE_H


#include "../pipeline.h"
#include "dx12_descriptor.h"
#include "dx12_forward.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace fantasy
{
    class DX12InputLayout : public InputLayoutInterface
    {
    public:
        DX12InputLayout(const DX12Context* context);

        bool initialize(const VertexAttributeDescArray& vertex_attribute_descs);

        // InputLayoutInterface
        uint32_t get_attributes_num() const override;
        const VertexAttributeDesc& get_attribute_desc(uint32_t attribute_index) const override;
        
        D3D12_INPUT_LAYOUT_DESC GetD3D12InputLayoutDesc() const;

    public:
        std::unordered_map<uint32_t, uint32_t> slot_strides;     /**< Maps a binding slot to an element stride.  */
        
    private:
        const DX12Context* _context; 
        std::vector<VertexAttributeDesc> _vertex_attribute_descs;
        std::vector<D3D12_INPUT_ELEMENT_DESC> d3d12_input_element_descs;
    };


    class DX12BindingLayout : public BindingLayoutInterface
    {
    public:
        DX12BindingLayout(const DX12Context* context, const BindingLayoutDesc& desc);

        bool initialize();

        // BindingLayoutInterface
        const BindingLayoutDesc& get_binding_desc() const override { return _desc; }
        const BindlessLayoutDesc& get_bindless_desc() const override { assert(false); return _invalid_desc; }
        bool is_binding_less() const override { return false; }

    public:
        uint32_t push_constant_size = 0;

        // The index in the d3d12_root_parameters.  
        uint32_t root_param_push_constant_index = ~0u;
        uint32_t root_param_srv_etc_index = ~0u;
        uint32_t root_param_sampler_index = ~0u;

        uint32_t descriptor_table_srv_etc_size = 0;
        uint32_t descriptor_table_sampler_size = 0;
        
        std::vector<D3D12_ROOT_PARAMETER1> d3d12_root_parameters;    

        // uint32_t: 在 D3D12_ROOT_PARAMETERS 中的序号. 
        std::vector<std::pair<uint32_t, D3D12_ROOT_DESCRIPTOR1>> descriptor_volatile_constant_buffers;
        std::vector<D3D12_DESCRIPTOR_RANGE1> d3d12_descriptor_srv_etc_ranges;
        std::vector<D3D12_DESCRIPTOR_RANGE1> d3d12_descriptor_sampler_ranges;

    private:
        const DX12Context* _context;
        BindingLayoutDesc _desc;
        BindlessLayoutDesc _invalid_desc;

        std::vector<BindingLayoutItem> _srv_etc_binding_layouts;
    };


    class DX12BindlessLayout : public BindingLayoutInterface
    {
    public:
        DX12BindlessLayout(const DX12Context* context, const BindlessLayoutDesc& desc);

        bool initialize();

        // BindingLayoutInterface
        const BindingLayoutDesc& get_binding_desc() const override { assert(false); return _invalid_desc; }
        const BindlessLayoutDesc& get_bindless_desc() const override { return _desc; }
        bool is_binding_less() const override { return true; }

    public:
        D3D12_ROOT_PARAMETER1 root_parameter;

    private:
        const DX12Context* _context;
        BindlessLayoutDesc _desc;
        BindingLayoutDesc _invalid_desc;

        // There only one Root Parameter which is Descriptor Table. 
        std::vector<D3D12_DESCRIPTOR_RANGE1> _descriptor_ranges;
    };



    class DX12GraphicsPipeline : public GraphicsPipelineInterface
    {
    public:
        DX12GraphicsPipeline(
            const DX12Context* context,
            const GraphicsPipelineDesc& desc,
            DX12RootSignature* dx12_root_signature,
            const FrameBufferInfo& crFrameBufferInfo
        );

        bool initialize();

        // GraphicsPipelineInterface
        const GraphicsPipelineDesc& get_desc() const override { return _desc; }
        const FrameBufferInfo& get_frame_buffer_info() const override { return _frame_buffer_info; }
        void* get_native_object() override { return _d3d12_pipeline_state.Get(); }

    public:
        bool require_blend_factor = false;

        std::unique_ptr<DX12RootSignature> dx12_root_signature;

    private:
        const DX12Context* _context;

        GraphicsPipelineDesc _desc;
        FrameBufferInfo _frame_buffer_info;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> _d3d12_pipeline_state;
    };

    
    class DX12ComputePipeline : public ComputePipelineInterface
    {
    public:
        DX12ComputePipeline(const DX12Context* context, const ComputePipelineDesc& desc, DX12RootSignature* dx12_root_signature);

        bool initialize();

        // ComputePipelineInterface
        const ComputePipelineDesc& get_desc() const override { return _desc; }
        void* get_native_object() override { return _d3d12_pipeline_state.Get(); }

    public:
        std::unique_ptr<DX12RootSignature> dx12_root_signature;
        
    private:
        const DX12Context* _context;
        ComputePipelineDesc _desc;
        
        Microsoft::WRL::ComPtr<ID3D12PipelineState> _d3d12_pipeline_state;
    };


    class DX12BindingSet : public BindingSetInterface
    {
    public:
        DX12BindingSet(
            const DX12Context* context,
            DX12DescriptorHeaps* descriptor_heaps,
            const BindingSetDesc& desc,
            BindingLayoutInterface* binding_layout
        );

        ~DX12BindingSet() noexcept;

        bool initialize();


        // BindingSetInterface
        const BindingSetDesc& get_desc() const override { return _desc; }
        BindingLayoutInterface* get_layout() const override { assert(_binding_layout != nullptr); return _binding_layout;}
        bool is_bindless() const override { return false; }

    public:
        
        uint32_t descriptor_table_srv_etc_base_index = 0;
        uint32_t descriptor_table_sampler_base_index = 0;
        uint32_t root_param_srv_etc_index = 0;
        uint32_t root_param_sampler_index = 0;

        bool is_descriptor_table_srv_etc_valid = false;
        bool is_descriptor_table_sampler_valid = false;
        bool has_uav_bingings = false;

        // uint32_t: The index in the d3d12_root_parameters. 
        std::vector<std::pair<uint32_t, BufferInterface*>> root_param_index_volatile_constant_buffers;

        std::vector<uint16_t> bindings_which_need_transition;
        

    private:
        const DX12Context* _context;
        DX12DescriptorHeaps* _descriptor_heaps;

        BindingSetDesc _desc;

        BindingLayoutInterface* _binding_layout;
        std::vector<std::shared_ptr<ResourceInterface>> _resources;
    };


    class DX12BindlessSet : public BindlessSetInterface
    {
    public:
        DX12BindlessSet(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps);
        ~DX12BindlessSet() noexcept;

        bool initialize();

        // BindingSetInterface
        const BindingSetDesc& get_desc() const override { assert(false); return _invalid_desc; }
        BindingLayoutInterface* get_layout() const override { assert(false); return nullptr;}
        bool is_bindless() const override { return true; }

        // BindlessSetInterface
        uint32_t get_capacity() const override { return capacity; }
		void resize(uint32_t new_size, bool keep_contents) override;
		bool set_slot(const BindingSetItem& item) override;

    public:
        uint32_t first_descriptor_index = 0;
        uint32_t capacity = 0;

    private:
        const DX12Context* _context;
        DX12DescriptorHeaps* _descriptor_heaps;

        BindingSetDesc _invalid_desc;
    };

}




















 #endif