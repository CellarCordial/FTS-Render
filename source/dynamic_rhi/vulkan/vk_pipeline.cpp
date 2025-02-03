#include "vk_pipeline.h"
#include "vk_convert.h"


namespace fantasy 
{

    bool VKInputLayout::initialize(const VertexAttributeDescArray& vertex_attribute_descs)
    {
        int total_attribute_array_size = 0;

        // map<VertexAttributeDesc::buffer_index, vk::VertexInputBindingDescription>
        std::unordered_map<uint32_t, vk::VertexInputBindingDescription> binding_map;

        // 遍历 VertexAttributeDescArray 将其按照 buffer_index 存储到 binding_map 中, 即去除相同 Buffer_index 重复值.
        for (const VertexAttributeDesc& desc : vertex_attribute_descs)
        {
            ReturnIfFalse(desc.array_size > 0);

            total_attribute_array_size += desc.array_size;

            if (binding_map.find(desc.buffer_index) == binding_map.end())
            {
                vk::VertexInputBindingDescription description{};
                description.binding = desc.buffer_index;
                description.stride = desc.element_stride;
                description.inputRate = desc.is_instanced ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex;
                binding_map[desc.buffer_index] = description;
            }
            else 
            {
                // 不同的 VertexAttributeDesc, 若其 buffer_index 相同, 则其 element_stride 和 is_instanced 也必须相同.
                // 即确保同一 vertex buffer 中的 Vertex 大小相同.
                ReturnIfFalse(binding_map[desc.buffer_index].stride == desc.element_stride);
                ReturnIfFalse(
                    binding_map[desc.buffer_index].inputRate == 
                        (desc.is_instanced ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex)
                );
            }
        }

        // 将去重后的 vk::VertexInputBindingDescription 转移至 vk_input_binding_desc.
        for (const auto& b : binding_map)
        {
            vk_input_binding_desc.push_back(b.second);
        }

        attribute_desc.resize(vertex_attribute_descs.size());
        vk_input_attribute_desc.resize(total_attribute_array_size);

        // 按照 VertexAttributeDesc::array_size 中的数量铺平.
        uint32_t attribute_location = 0;
        for (uint32_t ix = 0; ix < vertex_attribute_descs.size(); ix++)
        {
            const VertexAttributeDesc& desc = vertex_attribute_descs[ix];

            // 复制一份 VertexAttributeDesc.
            attribute_desc[ix] = desc;

            uint32_t element_size_bytes = get_format_info(desc.format).byte_size_per_pixel;

            uint32_t buffer_offset = 0;
            for (uint32_t slot = 0; slot < desc.array_size; ++slot)
            {
                auto& attribute = vk_input_attribute_desc[attribute_location];

                attribute.binding = desc.buffer_index;
                attribute.format = vk::Format(convert_format(desc.format));
                
                // 该遍历中变化的两个值.
                attribute.location = attribute_location;
                attribute.offset = buffer_offset + desc.offset;
                
                buffer_offset += element_size_bytes;
                attribute_location++;
            }
        }

        return true;
    }

    const VertexAttributeDesc& VKInputLayout::get_attribute_desc(uint32_t attribute_index) const 
    {
        assert(attribute_index < input_desc.size());
        return attribute_desc[attribute_index];
    }

    uint32_t VKInputLayout::get_attributes_num() const
    { 
        return static_cast<uint32_t>(attribute_desc.size()); 
    }


}