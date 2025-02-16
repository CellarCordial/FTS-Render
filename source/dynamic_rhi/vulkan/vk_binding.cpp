#include "vk_binding.h"
#include "vk_resource.h"
#include "vk_convert.h"
#include "../../core/tools/check_cast.h"

namespace fantasy 
{
    VKBindingLayout::VKBindingLayout(const VKContext* context, const BindingLayoutDesc& desc) :
        _context(context), binding_desc(desc), is_bindless(false)
    {
        uint32_t binding_location = 0;

        for (const BindingLayoutItem& binding : binding_desc.binding_layout_items)
        {
            vk::DescriptorType descriptor_type;

            switch (binding.type)
            {
            case ResourceViewType::PushConstants: descriptor_type = vk::DescriptorType::eUniformBuffer; break;
            
            case ResourceViewType::Texture_SRV: descriptor_type = vk::DescriptorType::eSampledImage; break;
            case ResourceViewType::Texture_UAV: descriptor_type = vk::DescriptorType::eStorageImage; break;
            
            case ResourceViewType::ConstantBuffer: descriptor_type = vk::DescriptorType::eUniformBuffer; break;
            
            case ResourceViewType::TypedBuffer_SRV: descriptor_type = vk::DescriptorType::eUniformTexelBuffer; break;
            case ResourceViewType::TypedBuffer_UAV: descriptor_type = vk::DescriptorType::eStorageTexelBuffer; break;

            case ResourceViewType::StructuredBuffer_SRV:
            case ResourceViewType::StructuredBuffer_UAV:
            case ResourceViewType::RawBuffer_SRV:
            case ResourceViewType::RawBuffer_UAV:
                descriptor_type = vk::DescriptorType::eStorageBuffer;
                break;

            case ResourceViewType::Sampler: descriptor_type = vk::DescriptorType::eSampler; break;

            case ResourceViewType::AccelStruct: descriptor_type = vk::DescriptorType::eAccelerationStructureKHR; break;

            default:
                assert(!"Invalid enum");
                continue;
            }

            vk::DescriptorSetLayoutBinding vk_layout_binding{};
            vk_layout_binding.binding = binding_location;
            vk_layout_binding.descriptorType = descriptor_type;
            vk_layout_binding.descriptorCount = binding.type == ResourceViewType::PushConstants ? 0 : 1;
            vk_layout_binding.stageFlags = convert_shader_type_to_shader_stage_flag_bits(binding_desc.shader_visibility);
            vk_layout_binding.pImmutableSamplers = nullptr;

            vk_descriptor_set_layout_bindings.push_back(vk_layout_binding);

            binding_location++;
        }
    }

    VKBindingLayout::VKBindingLayout(const VKContext* context, const BindlessLayoutDesc& desc) :
        _context(context), bindless_desc(desc), is_bindless(true)
    {
        uint32_t binding_location = 0;

        for (const BindingLayoutItem& binding : bindless_desc.binding_layout_items)
        {
            vk::DescriptorType descriptor_type;

            uint32_t mask = 0;

            switch (binding.type)
            {
            case ResourceViewType::Texture_SRV: descriptor_type = vk::DescriptorType::eSampledImage; break;
            case ResourceViewType::Texture_UAV: descriptor_type = vk::DescriptorType::eStorageImage; break;
            
            case ResourceViewType::TypedBuffer_SRV: descriptor_type = vk::DescriptorType::eUniformTexelBuffer; break;
            case ResourceViewType::TypedBuffer_UAV: descriptor_type = vk::DescriptorType::eStorageTexelBuffer; break;

            case ResourceViewType::ConstantBuffer: descriptor_type = vk::DescriptorType::eUniformBuffer; break;

            case ResourceViewType::StructuredBuffer_SRV:
            case ResourceViewType::StructuredBuffer_UAV:
            case ResourceViewType::RawBuffer_SRV:
            case ResourceViewType::RawBuffer_UAV:
                descriptor_type = vk::DescriptorType::eStorageBuffer;
                break;

            case ResourceViewType::Sampler: descriptor_type = vk::DescriptorType::eSampler; break;

            case ResourceViewType::AccelStruct: descriptor_type = vk::DescriptorType::eAccelerationStructureKHR; break;

            default:
                assert(!"Invalid enum");
                continue;
            }

            vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding{};
            descriptorSetLayoutBinding.binding = binding_location;
            descriptorSetLayoutBinding.descriptorCount = 1024;
            descriptorSetLayoutBinding.descriptorType = descriptor_type;
            descriptorSetLayoutBinding.stageFlags = convert_shader_type_to_shader_stage_flag_bits(bindless_desc.shader_visibility);

            vk_descriptor_set_layout_bindings.push_back(descriptorSetLayoutBinding);

            binding_location++;
        } 
    }

    VKBindingLayout::~VKBindingLayout()
    {
        if (vk_descriptor_set_layout)
        {
            _context->device.destroyDescriptorSetLayout(vk_descriptor_set_layout, _context->allocation_callbacks);
        }
    }

    bool VKBindingLayout::initialize()
    {
        vk::DescriptorSetLayoutCreateInfo vk_descriptor_set_layout_info{};
        vk_descriptor_set_layout_info.pNext = nullptr;
        vk_descriptor_set_layout_info.flags = vk::DescriptorSetLayoutCreateFlags();
        vk_descriptor_set_layout_info.bindingCount = static_cast<uint32_t>(vk_descriptor_set_layout_bindings.size());
        vk_descriptor_set_layout_info.pBindings = vk_descriptor_set_layout_bindings.data();

        // ePartiallyBound: 允许描述符集中的某些描述符未被绑定资源，而不会导致验证层报错。
        std::vector<vk::DescriptorBindingFlags> bind_flag(
            vk_descriptor_set_layout_bindings.size(), 
            vk::DescriptorBindingFlagBits::ePartiallyBound
        );

        vk::DescriptorSetLayoutBindingFlagsCreateInfo vk_bindless_info{};
        vk_bindless_info.bindingCount = static_cast<uint32_t>(vk_descriptor_set_layout_bindings.size());
        vk_bindless_info.pBindingFlags = bind_flag.data();

        if (is_bindless) vk_descriptor_set_layout_info.setPNext(&vk_bindless_info);

        ReturnIfFalse(vk::Result::eSuccess == _context->device.createDescriptorSetLayout(
            &vk_descriptor_set_layout_info,
            _context->allocation_callbacks,
            &vk_descriptor_set_layout
        ));

        std::unordered_map<vk::DescriptorType, uint32_t> pool_size_map;
        for (auto binding : vk_descriptor_set_layout_bindings)
        {
            if (pool_size_map.find(binding.descriptorType) == pool_size_map.end())
            {
                pool_size_map[binding.descriptorType] = 0;
            }

            pool_size_map[binding.descriptorType] += binding.descriptorCount;
        }

        for (auto iter : pool_size_map)
        {
            if (iter.second > 0)
            {
                vk_descriptor_pool_sizes.push_back(vk::DescriptorPoolSize(iter.first, iter.second));
            }
        }
        return true;
    }

    const BindlessLayoutDesc& VKBindingLayout::get_bindless_desc() const
    {
        if (!is_bindless) assert(!"Binding layout has no bindless desc.");
        return bindless_desc;
    }

    const BindingLayoutDesc& VKBindingLayout::get_binding_desc() const
    {
        if (is_bindless) assert(!"Bindless layout has no binding desc.");
        return binding_desc;
    }


    VKBindingSet::VKBindingSet(
        const VKContext* context,
        const BindingSetDesc& desc_, 
        std::shared_ptr<BindingLayoutInterface> binding_layout
     ) : 
        _context(context), desc(desc_), _layout(binding_layout)
    {
    }

    VKBindingSet::~VKBindingSet()
    {
        // VkDescriptorSet 会在 VkDescriptorPool 清除时自动被清除.
        if (vk_descriptor_pool)
        {
            _context->device.destroyDescriptorPool(vk_descriptor_pool, _context->allocation_callbacks);
        }
    }

    bool VKBindingSet::initialize()
    {
        auto binding_layout = check_cast<VKBindingLayout>(_layout);

        const auto& vk_descriptor_pool_sizes = binding_layout->vk_descriptor_pool_sizes;

        vk::DescriptorPoolCreateInfo vk_descriptor_pool_info{};
        vk_descriptor_pool_info.pNext = nullptr;
        vk_descriptor_pool_info.flags = vk::DescriptorPoolCreateFlags();
        vk_descriptor_pool_info.maxSets = 1;
        vk_descriptor_pool_info.poolSizeCount = uint32_t(vk_descriptor_pool_sizes.size());
        vk_descriptor_pool_info.pPoolSizes = vk_descriptor_pool_sizes.data();

        ReturnIfFalse(vk::Result::eSuccess == _context->device.createDescriptorPool(
            &vk_descriptor_pool_info, 
            _context->allocation_callbacks, 
            &vk_descriptor_pool
        ));
        
        const auto& vk_descriptor_set_layout = binding_layout->vk_descriptor_set_layout;

        vk::DescriptorSetAllocateInfo vk_descriptor_set_alloc_info{};
        vk_descriptor_set_alloc_info.pNext = nullptr;
        vk_descriptor_set_alloc_info.descriptorPool = vk_descriptor_pool;
        vk_descriptor_set_alloc_info.descriptorSetCount = 1;
        vk_descriptor_set_alloc_info.pSetLayouts = &vk_descriptor_set_layout;

        ReturnIfFalse(vk::Result::eSuccess == _context->device.allocateDescriptorSets(
            &vk_descriptor_set_alloc_info, 
            &vk_descriptor_set
        ));

        
        StackArray<vk::DescriptorImageInfo, MAX_BINDINGS_PER_LAYOUT> descriptor_image_info;
        StackArray<vk::DescriptorBufferInfo, MAX_BINDINGS_PER_LAYOUT> descriptor_buffer_info;

        StackArray<vk::WriteDescriptorSet, MAX_BINDINGS_PER_LAYOUT> descriptor_write_info;

        for (size_t ix = 0; ix < desc.binding_items.size(); ix++)
        {
            const BindingSetItem& binding = desc.binding_items[ix];
            const vk::DescriptorSetLayoutBinding& layout_binding = binding_layout->vk_descriptor_set_layout_bindings[ix];

            ReturnIfFalse(binding.resource == nullptr && binding.type != ResourceViewType::PushConstants);

            ref_resources.push_back(binding.resource);

            switch (binding.type)
            {
            case ResourceViewType::Texture_SRV:
            {
                const auto texture = check_cast<VKTexture>(binding.resource);

                vk::DescriptorImageInfo vk_descriptor_image_info{};
                vk_descriptor_image_info.sampler = VK_NULL_HANDLE;
                vk_descriptor_image_info.imageView = texture->get_view(ResourceViewType::Texture_SRV, binding.subresource, binding.format);
                vk_descriptor_image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

                descriptor_image_info.push_back(vk_descriptor_image_info);

                vk::WriteDescriptorSet vk_write_descriptor_set{};
                vk_write_descriptor_set.pNext = nullptr;
                vk_write_descriptor_set.dstSet = vk_descriptor_set;
                vk_write_descriptor_set.dstBinding = layout_binding.binding;
                vk_write_descriptor_set.dstArrayElement = 0;
                vk_write_descriptor_set.descriptorCount = 1;
                vk_write_descriptor_set.descriptorType = layout_binding.descriptorType;
                vk_write_descriptor_set.pImageInfo = &descriptor_image_info.back();
                vk_write_descriptor_set.pBufferInfo = nullptr;
                vk_write_descriptor_set.pTexelBufferView = nullptr;

                descriptor_write_info.push_back(vk_write_descriptor_set);
            }
            break;

            case ResourceViewType::Texture_UAV:
            {
                const auto texture = check_cast<VKTexture>(binding.resource);

                vk::DescriptorImageInfo vk_descriptor_image_info{};
                vk_descriptor_image_info.sampler = VK_NULL_HANDLE;
                vk_descriptor_image_info.imageView = texture->get_view(ResourceViewType::Texture_UAV, binding.subresource, binding.format);
                vk_descriptor_image_info.imageLayout = vk::ImageLayout::eGeneral;

                descriptor_image_info.push_back(vk_descriptor_image_info);

                vk::WriteDescriptorSet vk_write_descriptor_set{};
                vk_write_descriptor_set.pNext = nullptr;
                vk_write_descriptor_set.dstSet = vk_descriptor_set;
                vk_write_descriptor_set.dstBinding = layout_binding.binding;
                vk_write_descriptor_set.dstArrayElement = 0;
                vk_write_descriptor_set.descriptorCount = 1;
                vk_write_descriptor_set.descriptorType = layout_binding.descriptorType;
                vk_write_descriptor_set.pImageInfo = &descriptor_image_info.back();
                vk_write_descriptor_set.pBufferInfo = nullptr;
                vk_write_descriptor_set.pTexelBufferView = nullptr;

                descriptor_write_info.push_back(vk_write_descriptor_set);
            }
            break;

            case ResourceViewType::TypedBuffer_SRV:
            case ResourceViewType::TypedBuffer_UAV:
            {
                const auto buffer = check_cast<VKBuffer>(binding.resource);

                vk::BufferView buffer_view = buffer->get_typed_buffer_view(binding.range, binding.type, binding.format);

                vk::WriteDescriptorSet vk_write_descriptor_set{};
                vk_write_descriptor_set.pNext = nullptr;
                vk_write_descriptor_set.dstSet = vk_descriptor_set;
                vk_write_descriptor_set.dstBinding = layout_binding.binding;
                vk_write_descriptor_set.dstArrayElement = 0;
                vk_write_descriptor_set.descriptorCount = 1;
                vk_write_descriptor_set.descriptorType = layout_binding.descriptorType;
                vk_write_descriptor_set.pImageInfo = nullptr;
                vk_write_descriptor_set.pBufferInfo = nullptr;
                vk_write_descriptor_set.pTexelBufferView = &buffer_view;

                descriptor_write_info.push_back(vk_write_descriptor_set);
            }
            break;

            case ResourceViewType::StructuredBuffer_SRV:
            case ResourceViewType::StructuredBuffer_UAV:
            case ResourceViewType::RawBuffer_SRV:
            case ResourceViewType::RawBuffer_UAV:
            case ResourceViewType::ConstantBuffer:
            {
                const auto buffer = check_cast<VKBuffer>(binding.resource);

                if (binding.type == ResourceViewType::StructuredBuffer_UAV || binding.type == ResourceViewType::RawBuffer_UAV)
                    ReturnIfFalse(buffer->desc.allow_unordered_access);
                if (binding.type == ResourceViewType::StructuredBuffer_UAV || binding.type == ResourceViewType::StructuredBuffer_SRV)
                    ReturnIfFalse(buffer->desc.struct_stride != 0);

                vk::DescriptorBufferInfo vk_descriptor_buffer_info{};
                vk_descriptor_buffer_info.buffer = buffer->vk_buffer;
                vk_descriptor_buffer_info.offset = binding.range.byte_offset;
                vk_descriptor_buffer_info.range = binding.range.byte_size;

                descriptor_buffer_info.push_back(vk_descriptor_buffer_info);

                vk::WriteDescriptorSet vk_write_descriptor_set{};
                vk_write_descriptor_set.pNext = nullptr;
                vk_write_descriptor_set.dstSet = vk_descriptor_set;
                vk_write_descriptor_set.dstBinding = layout_binding.binding;
                vk_write_descriptor_set.dstArrayElement = 0;
                vk_write_descriptor_set.descriptorCount = 1;
                vk_write_descriptor_set.descriptorType = layout_binding.descriptorType;
                vk_write_descriptor_set.pImageInfo = nullptr;
                vk_write_descriptor_set.pBufferInfo = &descriptor_buffer_info.back();
                vk_write_descriptor_set.pTexelBufferView = nullptr;

                descriptor_write_info.push_back(vk_write_descriptor_set);
            }
            break;

            case ResourceViewType::Sampler:
            {
                const auto sampler = check_cast<VKSampler>(binding.resource);

                vk::DescriptorImageInfo vk_descriptor_sampler_info{};
                vk_descriptor_sampler_info.sampler = sampler->vk_sampler;
                vk_descriptor_sampler_info.imageView = VK_NULL_HANDLE;
                vk_descriptor_sampler_info.imageLayout = vk::ImageLayout(0);

                descriptor_image_info.push_back(vk_descriptor_sampler_info);

                vk::WriteDescriptorSet vk_write_descriptor_set{};
                vk_write_descriptor_set.pNext = nullptr;
                vk_write_descriptor_set.dstSet = vk_descriptor_set;
                vk_write_descriptor_set.dstBinding = layout_binding.binding;
                vk_write_descriptor_set.dstArrayElement = 0;
                vk_write_descriptor_set.descriptorCount = 1;
                vk_write_descriptor_set.descriptorType = layout_binding.descriptorType;
                vk_write_descriptor_set.pImageInfo = &descriptor_image_info.back();
                vk_write_descriptor_set.pBufferInfo = nullptr;
                vk_write_descriptor_set.pTexelBufferView = nullptr;

                descriptor_write_info.push_back(vk_write_descriptor_set);
            }
            break;

            default:
                assert(!"Invalid enum");
                break;
            }
        }

        _context->device.updateDescriptorSets(
            static_cast<uint32_t>(descriptor_write_info.size()), 
            descriptor_write_info.data(), 
            0, 
            nullptr
        );

        return true;
    }


    VKBindlessSet::VKBindlessSet(const VKContext* context, std::shared_ptr<BindingLayoutInterface> binding_layout) : 
        _context(context), _layout(binding_layout)
    {
    }

    VKBindlessSet::~VKBindlessSet()
    {
        if (vk_descriptor_pool)
        {
            _context->device.destroyDescriptorPool(vk_descriptor_pool, _context->allocation_callbacks);
        }
    }

    bool VKBindlessSet::initialize()
    {
        auto binding_layout = check_cast<VKBindingLayout>(_layout);

        capacity = binding_layout->vk_descriptor_set_layout_bindings[0].descriptorCount;

        const auto& vk_descriptor_pool_sizes = binding_layout->vk_descriptor_pool_sizes;

        vk::DescriptorPoolCreateInfo vk_descriptor_pool_info{};
        vk_descriptor_pool_info.pNext = nullptr;
        vk_descriptor_pool_info.flags = vk::DescriptorPoolCreateFlags();
        vk_descriptor_pool_info.maxSets = 1;
        vk_descriptor_pool_info.poolSizeCount = static_cast<uint32_t>(vk_descriptor_pool_sizes.size());
        vk_descriptor_pool_info.pPoolSizes = vk_descriptor_pool_sizes.data();

        ReturnIfFalse(vk::Result::eSuccess == _context->device.createDescriptorPool(
            &vk_descriptor_pool_info,
            _context->allocation_callbacks,
            &vk_descriptor_pool
        ));

        const auto& vk_descriptor_set_layout = binding_layout->vk_descriptor_set_layout;

        vk::DescriptorSetAllocateInfo vk_descriptor_set_alloc_info{};
        vk_descriptor_set_alloc_info.pNext = nullptr;
        vk_descriptor_set_alloc_info.descriptorPool = vk_descriptor_pool;
        vk_descriptor_set_alloc_info.descriptorSetCount = 1;
        vk_descriptor_set_alloc_info.pSetLayouts = &vk_descriptor_set_layout;

        return _context->device.allocateDescriptorSets(&vk_descriptor_set_alloc_info, &vk_descriptor_set) == vk::Result::eSuccess;
    } 

    bool VKBindlessSet::resize(uint32_t new_size, bool keep_contents)
    {
        return new_size <= 1024;
    }

    bool VKBindlessSet::set_slot(const BindingSetItem& binding)
    {
        ReturnIfFalse(binding.slot < capacity);
        binding_items.push_back(binding);

        StackArray<vk::DescriptorImageInfo, MAX_BINDINGS_PER_LAYOUT> descriptor_image_info;
        StackArray<vk::DescriptorBufferInfo, MAX_BINDINGS_PER_LAYOUT> descriptor_buffer_info;

        StackArray<vk::WriteDescriptorSet, MAX_BINDINGS_PER_LAYOUT> descriptor_write_info;

        auto binding_layout = check_cast<VKBindingLayout>(_layout);

        for (uint32_t ix = 0; ix < binding_layout->bindless_desc.binding_layout_items.size(); ix++)
        {
            if (binding_layout->bindless_desc.binding_layout_items[ix].type == binding.type)
            {
                const vk::DescriptorSetLayoutBinding& layout_binding = 
                    binding_layout->vk_descriptor_set_layout_bindings[ix];

                switch (binding.type)
                {
                case ResourceViewType::Texture_SRV:
                {
                    const auto texture = check_cast<VKTexture>(binding.resource);

                    vk::DescriptorImageInfo vk_descriptor_image_info{};
                    vk_descriptor_image_info.sampler = VK_NULL_HANDLE;
                    vk_descriptor_image_info.imageView = texture->get_view(ResourceViewType::Texture_SRV, binding.subresource, binding.format);
                    vk_descriptor_image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

                    descriptor_image_info.push_back(vk_descriptor_image_info);

                    vk::WriteDescriptorSet vk_write_descriptor_set{};
                    vk_write_descriptor_set.pNext = nullptr;
                    vk_write_descriptor_set.dstSet = vk_descriptor_set;
                    vk_write_descriptor_set.dstBinding = layout_binding.binding;
                    vk_write_descriptor_set.dstArrayElement = 0;
                    vk_write_descriptor_set.descriptorCount = 1;
                    vk_write_descriptor_set.descriptorType = layout_binding.descriptorType;
                    vk_write_descriptor_set.pImageInfo = &descriptor_image_info.back();
                    vk_write_descriptor_set.pBufferInfo = nullptr;
                    vk_write_descriptor_set.pTexelBufferView = nullptr;

                    descriptor_write_info.push_back(vk_write_descriptor_set);
                }
                break;

                case ResourceViewType::Texture_UAV:
                {
                    const auto texture = check_cast<VKTexture>(binding.resource);

                    vk::DescriptorImageInfo vk_descriptor_image_info{};
                    vk_descriptor_image_info.sampler = VK_NULL_HANDLE;
                    vk_descriptor_image_info.imageView = texture->get_view(ResourceViewType::Texture_UAV, binding.subresource, binding.format);
                    vk_descriptor_image_info.imageLayout = vk::ImageLayout::eGeneral;

                    descriptor_image_info.push_back(vk_descriptor_image_info);

                    vk::WriteDescriptorSet vk_write_descriptor_set{};
                    vk_write_descriptor_set.pNext = nullptr;
                    vk_write_descriptor_set.dstSet = vk_descriptor_set;
                    vk_write_descriptor_set.dstBinding = layout_binding.binding;
                    vk_write_descriptor_set.dstArrayElement = 0;
                    vk_write_descriptor_set.descriptorCount = 1;
                    vk_write_descriptor_set.descriptorType = layout_binding.descriptorType;
                    vk_write_descriptor_set.pImageInfo = &descriptor_image_info.back();
                    vk_write_descriptor_set.pBufferInfo = nullptr;
                    vk_write_descriptor_set.pTexelBufferView = nullptr;

                    descriptor_write_info.push_back(vk_write_descriptor_set);
                }
                break;

                case ResourceViewType::TypedBuffer_SRV:
                case ResourceViewType::TypedBuffer_UAV:
                {
                    const auto buffer = check_cast<VKBuffer>(binding.resource);

                    vk::BufferView buffer_view = buffer->get_typed_buffer_view(binding.range, binding.type, binding.format);

                    vk::WriteDescriptorSet vk_write_descriptor_set{};
                    vk_write_descriptor_set.pNext = nullptr;
                    vk_write_descriptor_set.dstSet = vk_descriptor_set;
                    vk_write_descriptor_set.dstBinding = layout_binding.binding;
                    vk_write_descriptor_set.dstArrayElement = 0;
                    vk_write_descriptor_set.descriptorCount = 1;
                    vk_write_descriptor_set.descriptorType = layout_binding.descriptorType;
                    vk_write_descriptor_set.pImageInfo = nullptr;
                    vk_write_descriptor_set.pBufferInfo = nullptr;
                    vk_write_descriptor_set.pTexelBufferView = &buffer_view;

                    descriptor_write_info.push_back(vk_write_descriptor_set);
                }
                break;

                case ResourceViewType::StructuredBuffer_SRV:
                case ResourceViewType::StructuredBuffer_UAV:
                case ResourceViewType::RawBuffer_SRV:
                case ResourceViewType::RawBuffer_UAV:
                case ResourceViewType::ConstantBuffer:
                {
                    const auto buffer = check_cast<VKBuffer>(binding.resource);

                    if (binding.type == ResourceViewType::StructuredBuffer_UAV || binding.type == ResourceViewType::RawBuffer_UAV)
                        ReturnIfFalse(buffer->desc.allow_unordered_access);
                    if (binding.type == ResourceViewType::StructuredBuffer_UAV || binding.type == ResourceViewType::StructuredBuffer_SRV)
                        ReturnIfFalse(buffer->desc.struct_stride != 0);

                    vk::DescriptorBufferInfo vk_descriptor_buffer_info{};
                    vk_descriptor_buffer_info.buffer = buffer->vk_buffer;
                    vk_descriptor_buffer_info.offset = binding.range.byte_offset;
                    vk_descriptor_buffer_info.range = binding.range.byte_size;

                    descriptor_buffer_info.push_back(vk_descriptor_buffer_info);

                    vk::WriteDescriptorSet vk_write_descriptor_set{};
                    vk_write_descriptor_set.pNext = nullptr;
                    vk_write_descriptor_set.dstSet = vk_descriptor_set;
                    vk_write_descriptor_set.dstBinding = layout_binding.binding;
                    vk_write_descriptor_set.dstArrayElement = 0;
                    vk_write_descriptor_set.descriptorCount = 1;
                    vk_write_descriptor_set.descriptorType = layout_binding.descriptorType;
                    vk_write_descriptor_set.pImageInfo = nullptr;
                    vk_write_descriptor_set.pBufferInfo = &descriptor_buffer_info.back();
                    vk_write_descriptor_set.pTexelBufferView = nullptr;

                    descriptor_write_info.push_back(vk_write_descriptor_set);
                }
                break;

                case ResourceViewType::Sampler:
                {
                    const auto sampler = check_cast<VKSampler>(binding.resource);

                    vk::DescriptorImageInfo vk_descriptor_sampler_info{};
                    vk_descriptor_sampler_info.sampler = sampler->vk_sampler;
                    vk_descriptor_sampler_info.imageView = VK_NULL_HANDLE;
                    vk_descriptor_sampler_info.imageLayout = vk::ImageLayout(0);

                    descriptor_image_info.push_back(vk_descriptor_sampler_info);

                    vk::WriteDescriptorSet vk_write_descriptor_set{};
                    vk_write_descriptor_set.pNext = nullptr;
                    vk_write_descriptor_set.dstSet = vk_descriptor_set;
                    vk_write_descriptor_set.dstBinding = layout_binding.binding;
                    vk_write_descriptor_set.dstArrayElement = 0;
                    vk_write_descriptor_set.descriptorCount = 1;
                    vk_write_descriptor_set.descriptorType = layout_binding.descriptorType;
                    vk_write_descriptor_set.pImageInfo = &descriptor_image_info.back();
                    vk_write_descriptor_set.pBufferInfo = nullptr;
                    vk_write_descriptor_set.pTexelBufferView = nullptr;

                    descriptor_write_info.push_back(vk_write_descriptor_set);
                }
                break;

                default:
                    assert(!"Invalid enum");
                    return false;
                }
            }
        }

        _context->device.updateDescriptorSets(
            static_cast<uint32_t>(descriptor_write_info.size()), 
            descriptor_write_info.data(), 
            0, 
            nullptr
        );

        return true;
    }
}