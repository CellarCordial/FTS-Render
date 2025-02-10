#ifndef DYNAMIC_RHI_VULKAN_PIPELINE_H
#define DYNAMIC_RHI_VULKAN_PIPELINE_H

#include "../frame_buffer.h"
#include "../pipeline.h"
#include "vk_forward.h"


namespace fantasy 
{
    class VKInputLayout : public InputLayoutInterface
    {
    public:
        VKInputLayout() = default;
		~VKInputLayout() override = default;

        bool initialize(const VertexAttributeDescArray& vertex_attribute_descs);

        const VertexAttributeDesc& get_attribute_desc(uint32_t attribute_index) const override;
        uint32_t get_attributes_num() const override;

    public:
        std::vector<VertexAttributeDesc> attribute_desc;
        std::vector<vk::VertexInputBindingDescription> vk_input_binding_desc;
        std::vector<vk::VertexInputAttributeDescription> vk_input_attribute_desc;
    };


    class VKGraphicsPipeline : public GraphicsPipelineInterface
    {
    public:
        explicit VKGraphicsPipeline(const VKContext* context, const GraphicsPipelineDesc& desc);
        ~VKGraphicsPipeline() override;

        bool initialize(FrameBufferInterface* frame_buffer);

        const GraphicsPipelineDesc& get_desc() const override;
        void* get_native_object() override;

    public:
        GraphicsPipelineDesc desc;

        vk::Pipeline vk_pipeline;
        vk::PipelineLayout vk_pipeline_layout;
        vk::ShaderStageFlags vk_push_constant_visibility;
        
        bool use_blend_constant = false;
        uint32_t push_constant_size = INVALID_SIZE_32;
    
    private:
        const VKContext* _context;
    };

    class VKComputePipeline : public ComputePipelineInterface
    {
    public:
        explicit VKComputePipeline(const VKContext* context, const ComputePipelineDesc& desc);
        ~VKComputePipeline() override;

        bool initialize();

        const ComputePipelineDesc& get_desc() const override;
        void* get_native_object() override;
    
    public:
        ComputePipelineDesc desc;

        vk::Pipeline vk_pipeline;
        vk::PipelineLayout vk_pipeline_layout;
        vk::ShaderStageFlags vk_push_constant_visibility;
        
        uint32_t push_constant_size = INVALID_SIZE_32;

    private:
        const VKContext* _context;
    };

}

#endif