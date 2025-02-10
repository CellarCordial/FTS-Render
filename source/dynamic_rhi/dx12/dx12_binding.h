#ifndef DYNAMIC_RHI_DX12_BINDING_H
#define DYNAMIC_RHI_DX12_BINDING_H

#include "../binding.h"
#include "dx12_descriptor.h"

namespace fantasy
{
    class DX12BindingLayout : public BindingLayoutInterface
    {
    public:
        DX12BindingLayout(const DX12Context* context, const BindingLayoutDesc& desc);

        bool initialize();

        const BindingLayoutDesc& get_binding_desc() const override { return _desc; }
        const BindlessLayoutDesc& get_bindless_desc() const override { assert(false); return _invalid_desc; }
        bool is_binding_less() const override { return false; }

    public:
        uint32_t push_constant_size = 0;
        uint32_t descriptor_table_srv_etc_size = 0;
        uint32_t descriptor_table_sampler_size = 0;

        uint32_t push_constant_root_param_index = INVALID_SIZE_32;
        uint32_t srv_root_param_start_index = INVALID_SIZE_32;
        uint32_t sampler_root_param_start_index = INVALID_SIZE_32;
        
        std::vector<D3D12_ROOT_PARAMETER1> d3d12_root_parameters;

        std::vector<std::pair<uint32_t, D3D12_ROOT_DESCRIPTOR1>> root_param_index_volatile_cb_descriptor_map;
        std::vector<D3D12_DESCRIPTOR_RANGE1> d3d12_descriptor_srv_etc_ranges;
        std::vector<D3D12_DESCRIPTOR_RANGE1> d3d12_descriptor_sampler_ranges;

    private:
        const DX12Context* _context;

        BindingLayoutDesc _desc;
        BindlessLayoutDesc _invalid_desc;
    };


    class DX12BindlessLayout : public BindingLayoutInterface
    {
    public:
        DX12BindlessLayout(const DX12Context* context, const BindlessLayoutDesc& desc);

        bool initialize();

        const BindingLayoutDesc& get_binding_desc() const override { assert(false); return invalid_desc; }
        const BindlessLayoutDesc& get_bindless_desc() const override { return desc; }
        bool is_binding_less() const override { return true; }

    public:
        BindlessLayoutDesc desc;
        BindingLayoutDesc invalid_desc;

        D3D12_ROOT_PARAMETER1 root_parameter;
        std::vector<D3D12_DESCRIPTOR_RANGE1> descriptor_ranges;

        uint32_t root_param_index = INVALID_SIZE_32;

    private:
        const DX12Context* _context;
    };

    
    class DX12BindingSet : public BindingSetInterface
    {
    public:
        DX12BindingSet(
            const DX12Context* context,
            DX12DescriptorManager* descriptor_heaps,
            const BindingSetDesc& desc,
            std::shared_ptr<BindingLayoutInterface> binding_layout
        );

        ~DX12BindingSet() noexcept;

        bool initialize();

        const BindingSetDesc& get_desc() const override { return _desc; }
        BindingLayoutInterface* get_layout() const override { return _binding_layout.get();}
        bool is_bindless() const override { return false; }

    public:
        uint32_t srv_start_index = 0;
        uint32_t sampler_view_start_index = 0;

        bool is_descriptor_table_srv_etc_valid = false;
        bool is_descriptor_table_sampler_valid = false;

        std::vector<std::pair<uint32_t, BufferInterface*>> root_param_index_volatile_cb_map;
        
    private:
        const DX12Context* _context;
        DX12DescriptorManager* _descriptor_manager;

        BindingSetDesc _desc;

        std::shared_ptr<BindingLayoutInterface> _binding_layout;
        std::vector<std::shared_ptr<ResourceInterface>> _ref_resources;
    };


    class DX12BindlessSet : public BindlessSetInterface
    {
    public:
        DX12BindlessSet(
            const DX12Context* context, 
            DX12DescriptorManager* descriptor_heaps, 
            std::shared_ptr<BindingLayoutInterface> binding_layout
        );
        ~DX12BindlessSet() noexcept;

        bool initialize();

        const BindingSetDesc& get_desc() const override { assert(false); return _invalid_desc; }
        BindingLayoutInterface* get_layout() const override { return _binding_layout.get();}
        bool is_bindless() const override { return true; }

        uint32_t get_capacity() const override { return capacity; }
		bool resize(uint32_t new_size, bool keep_contents) override;
		bool set_slot(const BindingSetItem& item) override;

    public:
        uint32_t first_descriptor_index = 0;
        uint32_t capacity = 0;

        std::vector<BindingSetItem> binding_items;

    private:
        const DX12Context* _context;
        DX12DescriptorManager* _descriptor_manager;

        std::shared_ptr<BindingLayoutInterface> _binding_layout;

        BindingSetDesc _invalid_desc;
    };
}








#endif