#ifndef DYNAMIC_RHI_VULKAN_BINDING_H
#define DYNAMIC_RHI_VULKAN_BINDING_H

#include "../binding.h"
#include "vk_forward.h"

namespace fantasy
{
    class VKBindingLayout : public BindingLayoutInterface
    {
    public:
        VKBindingLayout(const VKContext* context, const BindingLayoutDesc& desc);
        VKBindingLayout(const VKContext* context, const BindlessLayoutDesc& desc);
        ~VKBindingLayout() override;

        bool initialize();

        const BindlessLayoutDesc& get_bindless_desc() const override;
        const BindingLayoutDesc& get_binding_desc() const override;
        bool is_binding_less() const override { return is_bindless; }

    public:
        bool is_bindless;

        BindingLayoutDesc binding_desc;
        BindlessLayoutDesc bindless_desc;

        vk::DescriptorSetLayout vk_descriptor_set_layout;
        std::vector<vk::DescriptorPoolSize> vk_descriptor_pool_sizes;

        std::vector<vk::DescriptorSetLayoutBinding> vk_descriptor_set_layout_bindings;

    private:
        const VKContext* _context;
    };


    class VKBindingSet : public BindingSetInterface
    {
    public:
        explicit VKBindingSet(
            const VKContext* context, 
            const BindingSetDesc& desc, 
            std::shared_ptr<BindingLayoutInterface> binding_layout
        );
		~VKBindingSet() override;

        bool initialize();

        BindingLayoutInterface* get_layout() const override { return _layout.get(); }
        const BindingSetDesc& get_desc() const override { return desc; }
        bool is_bindless() const override { return true; }

    public:
        BindingSetDesc desc;

        vk::DescriptorPool vk_descriptor_pool;
        vk::DescriptorSet vk_descriptor_set;

        std::vector<std::shared_ptr<ResourceInterface>> ref_resources;

    private:
        const VKContext* _context;

        std::shared_ptr<BindingLayoutInterface> _layout;
    };

    class VKBindlessSet : public BindlessSetInterface
    {
    public:
        explicit VKBindlessSet(const VKContext* context, std::shared_ptr<BindingLayoutInterface> binding_layout);
        ~VKBindlessSet() override;

        bool initialize();

        // BindingSetInterface
        const BindingSetDesc& get_desc() const override { assert(false); return _invalid_desc; }
        BindingLayoutInterface* get_layout() const override { assert(false); return _layout.get();}
        bool is_bindless() const override { return true; }

		bool resize(uint32_t new_size, bool keep_contents) override;
		bool set_slot(const BindingSetItem& item) override;
        uint32_t get_capacity() const override { return capacity; }

    public:
        vk::DescriptorPool vk_descriptor_pool;
        vk::DescriptorSet vk_descriptor_set;
        
        uint32_t capacity = 0;
        std::vector<BindingSetItem> binding_items;

    private:
        const VKContext* _context;

        std::shared_ptr<BindingLayoutInterface> _layout;
        BindingSetDesc _invalid_desc;
    };

}







#endif