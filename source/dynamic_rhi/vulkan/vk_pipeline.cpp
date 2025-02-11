#include "vk_pipeline.h"
#include "vk_binding.h"
#include "vk_convert.h"
#include "../../core/tools/check_cast.h"
#include "vk_frame_buffer.h"

namespace fantasy 
{
    bool create_vk_pipeline_layout(
        const VKContext* context,
        const BindingLayoutInterfaceArray& binding_layouts,
        vk::ShaderStageFlags& out_vk_push_constant_visibility,
        vk::PipelineLayout& out_vk_pipeline_layout,
        uint32_t& out_push_constant_size
    )
    {
        StackArray<vk::DescriptorSetLayout, MAX_BINDING_LAYOUTS> descriptorSetLayouts;

        out_vk_push_constant_visibility = vk::ShaderStageFlagBits();
        for (uint32_t ix = 0; ix < binding_layouts.size(); ++ix)
        {
            auto binding_layout = check_cast<VKBindingLayout>(binding_layouts[ix]);

            descriptorSetLayouts.push_back(binding_layout->vk_descriptor_set_layout);

            if (!binding_layout->is_binding_less())
            {
                for (const auto& binding : binding_layout->binding_desc.binding_layout_items)
                {
                    if (binding.type == ResourceViewType::PushConstants)
                    {
                        out_push_constant_size = binding.size;
                        out_vk_push_constant_visibility = 
                            convert_shader_type_to_shader_stage_flag_bits(binding_layout->binding_desc.shader_visibility);
                        break;
                    }
                }
            }
        }

        vk::PushConstantRange vk_push_constant_range{};
        vk_push_constant_range.stageFlags = out_vk_push_constant_visibility;
        vk_push_constant_range.offset = 0;
        vk_push_constant_range.size = out_push_constant_size;

        vk::PipelineLayoutCreateInfo vk_pipeline_layout_info{};
        vk_pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        vk_pipeline_layout_info.pSetLayouts = descriptorSetLayouts.data();
        vk_pipeline_layout_info.pushConstantRangeCount = out_push_constant_size != 0 ? 1 : 0;
        vk_pipeline_layout_info.pPushConstantRanges = &vk_push_constant_range;;

        return vk::Result::eSuccess == context->device.createPipelineLayout(
            &vk_pipeline_layout_info,
            context->allocation_callbacks,
            &out_vk_pipeline_layout
        );
    }

    bool VKInputLayout::initialize(const VertexAttributeDescArray& vertex_attribute_descs)
    {
        int total_attribute_array_size = 0;

        std::unordered_map<uint32_t, vk::VertexInputBindingDescription> binding_map;

        for (const VertexAttributeDesc& desc : vertex_attribute_descs)
        {
            ReturnIfFalse(desc.array_size > 0);

            total_attribute_array_size += desc.array_size;

            if (binding_map.find(desc.buffer_slot) == binding_map.end())
            {
                vk::VertexInputBindingDescription description{};
                description.binding = desc.buffer_slot;
                description.stride = desc.element_stride;
                description.inputRate = desc.is_instanced ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex;
                binding_map[desc.buffer_slot] = description;
            }
            else 
            {
                ReturnIfFalse(binding_map[desc.buffer_slot].stride == desc.element_stride);
                ReturnIfFalse(
                    binding_map[desc.buffer_slot].inputRate == 
                        (desc.is_instanced ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex)
                );
            }
        }

        for (const auto& b : binding_map)
        {
            vk_input_binding_desc.push_back(b.second);
        }

        attribute_desc.resize(vertex_attribute_descs.size());
        vk_input_attribute_desc.resize(total_attribute_array_size);

        uint32_t attribute_location = 0;
        for (uint32_t ix = 0; ix < vertex_attribute_descs.size(); ix++)
        {
            const VertexAttributeDesc& desc = vertex_attribute_descs[ix];

            attribute_desc[ix] = desc;

            uint32_t element_size_bytes = get_format_info(desc.format).size;

            uint32_t buffer_offset = 0;
            for (uint32_t slot = 0; slot < desc.array_size; ++slot)
            {
                auto& attribute = vk_input_attribute_desc[attribute_location];

                attribute.binding = desc.buffer_slot;
                attribute.format = convert_format(desc.format);
                
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

    VKGraphicsPipeline::VKGraphicsPipeline(const VKContext* context, const GraphicsPipelineDesc& desc_)  : 
        _context(context), desc(desc_) 
    {
    }
    
    VKGraphicsPipeline::~VKGraphicsPipeline()
    {
        if (vk_pipeline)
        {
            _context->device.destroyPipeline(vk_pipeline, _context->allocation_callbacks);
        }

        if (vk_pipeline_layout)
        {
            _context->device.destroyPipelineLayout(vk_pipeline_layout, _context->allocation_callbacks);
        }
    }

    bool VKGraphicsPipeline::initialize(FrameBufferInterface* frame_buffer_)
    {
        std::vector<vk::PipelineShaderStageCreateInfo> vk_shader_stage_infos;

        vk::ShaderModule vs, hs, ds, gs, ps;

        if (desc.vertex_shader)
        {
            vk::ShaderModuleCreateInfo vk_shader_module_info{};
            vk_shader_module_info.pNext = nullptr;
            vk_shader_module_info.flags = vk::ShaderModuleCreateFlags();
            vk_shader_module_info.codeSize = desc.vertex_shader->get_byte_code().size;
            vk_shader_module_info.pCode = reinterpret_cast<uint32_t*>(desc.vertex_shader->get_byte_code().byte_code);
            ReturnIfFalse(_context->device.createShaderModule(&vk_shader_module_info, nullptr, &vs) == vk::Result::eSuccess);

            vk::PipelineShaderStageCreateInfo vk_pipeline_shader_stage_info{};
            vk_pipeline_shader_stage_info.pNext = nullptr;
            vk_pipeline_shader_stage_info.flags = vk::PipelineShaderStageCreateFlags();
            vk_pipeline_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
            vk_pipeline_shader_stage_info.module = vs;
            vk_pipeline_shader_stage_info.pName = desc.vertex_shader->get_desc().entry.c_str();
            vk_pipeline_shader_stage_info.pSpecializationInfo = nullptr;
            vk_shader_stage_infos.push_back(vk_pipeline_shader_stage_info);
        }
        if (desc.hull_shader)
        {
            vk::ShaderModuleCreateInfo vk_shader_module_info{};
            vk_shader_module_info.pNext = nullptr;
            vk_shader_module_info.flags = vk::ShaderModuleCreateFlags();
            vk_shader_module_info.codeSize = desc.hull_shader->get_byte_code().size;
            vk_shader_module_info.pCode = reinterpret_cast<uint32_t*>(desc.hull_shader->get_byte_code().byte_code);
            ReturnIfFalse(_context->device.createShaderModule(&vk_shader_module_info, nullptr, &hs) == vk::Result::eSuccess);

            vk::PipelineShaderStageCreateInfo vk_pipeline_shader_stage_info{};
            vk_pipeline_shader_stage_info.pNext = nullptr;
            vk_pipeline_shader_stage_info.flags = vk::PipelineShaderStageCreateFlags();
            vk_pipeline_shader_stage_info.stage = vk::ShaderStageFlagBits::eTessellationControl;
            vk_pipeline_shader_stage_info.module = hs;
            vk_pipeline_shader_stage_info.pName = desc.hull_shader->get_desc().entry.c_str();
            vk_pipeline_shader_stage_info.pSpecializationInfo = nullptr;
            vk_shader_stage_infos.push_back(vk_pipeline_shader_stage_info);
        }
        if (desc.domain_shader)
        {
            vk::ShaderModuleCreateInfo vk_shader_module_info{};
            vk_shader_module_info.pNext = nullptr;
            vk_shader_module_info.flags = vk::ShaderModuleCreateFlags();
            vk_shader_module_info.codeSize = desc.domain_shader->get_byte_code().size;
            vk_shader_module_info.pCode = reinterpret_cast<uint32_t*>(desc.domain_shader->get_byte_code().byte_code);
            ReturnIfFalse(_context->device.createShaderModule(&vk_shader_module_info, nullptr, &ds) == vk::Result::eSuccess);

            vk::PipelineShaderStageCreateInfo vk_pipeline_shader_stage_info{};
            vk_pipeline_shader_stage_info.pNext = nullptr;
            vk_pipeline_shader_stage_info.flags = vk::PipelineShaderStageCreateFlags();
            vk_pipeline_shader_stage_info.stage = vk::ShaderStageFlagBits::eTessellationEvaluation;
            vk_pipeline_shader_stage_info.module = ds;
            vk_pipeline_shader_stage_info.pName = desc.domain_shader->get_desc().entry.c_str();
            vk_pipeline_shader_stage_info.pSpecializationInfo = nullptr;
            vk_shader_stage_infos.push_back(vk_pipeline_shader_stage_info);
        }
        if (desc.geometry_shader)
        {
            vk::ShaderModuleCreateInfo vk_shader_module_info{};
            vk_shader_module_info.pNext = nullptr;
            vk_shader_module_info.flags = vk::ShaderModuleCreateFlags();
            vk_shader_module_info.codeSize = desc.geometry_shader->get_byte_code().size;
            vk_shader_module_info.pCode = reinterpret_cast<uint32_t*>(desc.geometry_shader->get_byte_code().byte_code);
            ReturnIfFalse(_context->device.createShaderModule(&vk_shader_module_info, nullptr, &gs) == vk::Result::eSuccess);

            vk::PipelineShaderStageCreateInfo vk_pipeline_shader_stage_info{};
            vk_pipeline_shader_stage_info.pNext = nullptr;
            vk_pipeline_shader_stage_info.flags = vk::PipelineShaderStageCreateFlags();
            vk_pipeline_shader_stage_info.stage = vk::ShaderStageFlagBits::eGeometry;
            vk_pipeline_shader_stage_info.module = gs;
            vk_pipeline_shader_stage_info.pName = desc.geometry_shader->get_desc().entry.c_str();
            vk_pipeline_shader_stage_info.pSpecializationInfo = nullptr;
            vk_shader_stage_infos.push_back(vk_pipeline_shader_stage_info);
        }
        if (desc.pixel_shader)
        {
            vk::ShaderModuleCreateInfo vk_shader_module_info{};
            vk_shader_module_info.pNext = nullptr;
            vk_shader_module_info.flags = vk::ShaderModuleCreateFlags();
            vk_shader_module_info.codeSize = desc.pixel_shader->get_byte_code().size;
            vk_shader_module_info.pCode = reinterpret_cast<uint32_t*>(desc.pixel_shader->get_byte_code().byte_code);
            ReturnIfFalse(_context->device.createShaderModule(&vk_shader_module_info, nullptr, &ps) == vk::Result::eSuccess);

            vk::PipelineShaderStageCreateInfo vk_pipeline_shader_stage_info{};
            vk_pipeline_shader_stage_info.pNext = nullptr;
            vk_pipeline_shader_stage_info.flags = vk::PipelineShaderStageCreateFlags();
            vk_pipeline_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
            vk_pipeline_shader_stage_info.module = ps;
            vk_pipeline_shader_stage_info.pName = desc.pixel_shader->get_desc().entry.c_str();
            vk_pipeline_shader_stage_info.pSpecializationInfo = nullptr;
            vk_shader_stage_infos.push_back(vk_pipeline_shader_stage_info);
        }

        
        vk::PipelineVertexInputStateCreateInfo vk_input_layout{};

        auto input_layout = check_cast<VKInputLayout>(desc.input_layout);
        if (input_layout)
        {
            vk_input_layout.pNext = nullptr;
            vk_input_layout.flags = vk::PipelineVertexInputStateCreateFlags();
            vk_input_layout.vertexBindingDescriptionCount = uint32_t(input_layout->vk_input_binding_desc.size());      
            vk_input_layout.pVertexBindingDescriptions = input_layout->vk_input_binding_desc.data();
            vk_input_layout.vertexAttributeDescriptionCount = uint32_t(input_layout->vk_input_attribute_desc.size());
            vk_input_layout.pVertexAttributeDescriptions = input_layout->vk_input_attribute_desc.data(); 
        }

        vk::PipelineInputAssemblyStateCreateInfo vk_input_assembly_info{};
        vk_input_assembly_info.pNext = nullptr;
        vk_input_assembly_info.flags = vk::PipelineInputAssemblyStateCreateFlags();
        vk_input_assembly_info.topology = convert_primitive_topology(desc.primitive_type);
        vk_input_assembly_info.primitiveRestartEnable = false;


        vk::PipelineViewportStateCreateInfo vk_viewport_info{};
        vk_viewport_info.pNext = nullptr;
        vk_viewport_info.flags = vk::PipelineViewportStateCreateFlags();
        vk_viewport_info.viewportCount = 1;
		vk_viewport_info.pViewports = nullptr;
		vk_viewport_info.scissorCount = 1;
		vk_viewport_info.pScissors = nullptr;


        const auto& raster_state = desc.render_state.raster_state;    

        vk::PipelineRasterizationStateCreateInfo vk_raster_state_info{};
        vk_raster_state_info.pNext = nullptr;
        vk_raster_state_info.flags = vk::PipelineRasterizationStateCreateFlags();
        vk_raster_state_info.depthClampEnable = false;
        vk_raster_state_info.rasterizerDiscardEnable = false;
        vk_raster_state_info.polygonMode = convert_fill_mode(raster_state.fill_mode);
        vk_raster_state_info.cullMode = convert_cull_mode(raster_state.cull_mode);
        vk_raster_state_info.frontFace = raster_state.front_counter_clock_wise ? vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise;
        vk_raster_state_info.depthBiasEnable = raster_state.depth_bias != 0 ? true : false;
        vk_raster_state_info.depthBiasConstantFactor = static_cast<float>(raster_state.depth_bias);
        vk_raster_state_info.depthBiasClamp = raster_state.depth_bias_clamp;
        vk_raster_state_info.depthBiasSlopeFactor = raster_state.slope_scale_depth_bias;
        vk_raster_state_info.lineWidth = 1.0f;

        vk::PipelineRasterizationConservativeStateCreateInfoEXT vk_raster_conservative_info{};
        vk_raster_conservative_info.conservativeRasterizationMode = vk::ConservativeRasterizationModeEXT::eOverestimate;
		if (raster_state.enable_conservative_raster) vk_raster_state_info.setPNext(&vk_raster_conservative_info);


        const auto& depth_stencil_state = desc.render_state.depth_stencil_state;

        vk::PipelineDepthStencilStateCreateInfo vk_depth_stencil_info{};
        vk_depth_stencil_info.pNext = nullptr;
        vk_depth_stencil_info.flags = vk::PipelineDepthStencilStateCreateFlags();
        vk_depth_stencil_info.depthTestEnable = depth_stencil_state.enable_depth_test;
        vk_depth_stencil_info.depthWriteEnable = depth_stencil_state.enable_depth_write;
        vk_depth_stencil_info.depthCompareOp = convert_compare_op(depth_stencil_state.depth_func);
        vk_depth_stencil_info.depthBoundsTestEnable = false;
        vk_depth_stencil_info.stencilTestEnable = depth_stencil_state.enable_stencil;
        vk_depth_stencil_info.front = convert_stencil_state(depth_stencil_state, depth_stencil_state.front_face_stencil);
        vk_depth_stencil_info.back = convert_stencil_state(depth_stencil_state, depth_stencil_state.back_face_stencil);
        vk_depth_stencil_info.maxDepthBounds = 0.0f;
        vk_depth_stencil_info.minDepthBounds = 0.0f;


        VKFrameBuffer* frame_buffer = check_cast<VKFrameBuffer*>(frame_buffer_);
        uint32_t color_attachment_count = frame_buffer->desc.color_attachments.size();

        const auto& blend_state = desc.render_state.blend_state;

        StackArray<vk::PipelineColorBlendAttachmentState, MAX_RENDER_TARGETS + 1> vk_color_blend_attachments(color_attachment_count);
        for(uint32_t i = 0; i < color_attachment_count; i++)
        {
            vk_color_blend_attachments[i] = convert_blend_state(blend_state.target_blends[i]);
        }

        vk::PipelineColorBlendStateCreateInfo vk_color_blend_info{};
        vk_color_blend_info.pNext = nullptr;
        vk_color_blend_info.flags = vk::PipelineColorBlendStateCreateFlags();
        vk_color_blend_info.logicOpEnable = false;
        vk_color_blend_info.logicOp = vk::LogicOp::eClear;
        vk_color_blend_info.attachmentCount = static_cast<uint32_t>(vk_color_blend_attachments.size());
        vk_color_blend_info.pAttachments = vk_color_blend_attachments.data();

        use_blend_constant = blend_state.if_use_constant_color(color_attachment_count);

        StackArray<vk::DynamicState, 5> vk_dynamic_states;
        vk_dynamic_states.push_back(vk::DynamicState::eViewport);
        vk_dynamic_states.push_back(vk::DynamicState::eScissor);
        if (use_blend_constant) vk_dynamic_states.push_back(vk::DynamicState::eBlendConstants);
        if (desc.render_state.depth_stencil_state.dynamic_stencil_ref) vk_dynamic_states.push_back(vk::DynamicState::eStencilReference);

        vk::PipelineDynamicStateCreateInfo vk_dynamic_state_info{};
        vk_dynamic_state_info.pNext = nullptr;
        vk_dynamic_state_info.flags = vk::PipelineDynamicStateCreateFlags();
        vk_dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(vk_dynamic_states.size());
        vk_dynamic_state_info.pDynamicStates = vk_dynamic_states.data();


        ReturnIfFalse(create_vk_pipeline_layout(
            _context, 
            desc.binding_layouts, 
            vk_push_constant_visibility, 
            vk_pipeline_layout,
            push_constant_size
        ));


        vk::GraphicsPipelineCreateInfo vk_pipeline_info{};
        vk_pipeline_info.pNext = nullptr;
        vk_pipeline_info.flags = vk::PipelineCreateFlags();
        vk_pipeline_info.stageCount = static_cast<uint32_t>(vk_shader_stage_infos.size());
        vk_pipeline_info.pStages = vk_shader_stage_infos.data();
        vk_pipeline_info.pVertexInputState = &vk_input_layout;
        vk_pipeline_info.pInputAssemblyState = &vk_input_assembly_info;
        vk_pipeline_info.pTessellationState = nullptr;
        vk_pipeline_info.pViewportState = &vk_viewport_info;
        vk_pipeline_info.pRasterizationState = &vk_raster_state_info;
        vk_pipeline_info.pMultisampleState = nullptr;
        vk_pipeline_info.pDepthStencilState = &vk_depth_stencil_info;
        vk_pipeline_info.pColorBlendState = &vk_color_blend_info;
        vk_pipeline_info.pDynamicState = &vk_dynamic_state_info;
        vk_pipeline_info.layout = vk_pipeline_layout;
        vk_pipeline_info.renderPass = frame_buffer->vk_render_pass;
        vk_pipeline_info.subpass = 0;
        vk_pipeline_info.basePipelineHandle = nullptr;
        vk_pipeline_info.basePipelineIndex = -1;

        vk::PipelineTessellationStateCreateInfo vk_tessellation_info{};
        if (desc.primitive_type == PrimitiveType::PatchList)
        {
            vk_tessellation_info.patchControlPoints = desc.patch_control_points;
            vk_pipeline_info.pTessellationState = &vk_tessellation_info;
        }

        return vk::Result::eSuccess == _context->device.createGraphicsPipelines(
            _context->vk_pipeline_cache,
            1, 
            &vk_pipeline_info,
            _context->allocation_callbacks,
            &vk_pipeline
        );
    }

    const GraphicsPipelineDesc& VKGraphicsPipeline::get_desc() const 
    { 
        return desc; 
    }

    void* VKGraphicsPipeline::get_native_object() 
    { 
        return vk_pipeline; 
    }

    VKComputePipeline::VKComputePipeline(const VKContext* context, const ComputePipelineDesc& desc_)  : 
        _context(context), desc(desc_) 
    {
    }

    VKComputePipeline::~VKComputePipeline()
    {
        if (vk_pipeline)
        {
            _context->device.destroyPipeline(vk_pipeline, _context->allocation_callbacks);
        }

        if (vk_pipeline_layout)
        {
            _context->device.destroyPipelineLayout(vk_pipeline_layout, _context->allocation_callbacks);
        }
    }

    bool VKComputePipeline::initialize()
    {
        ReturnIfFalse(create_vk_pipeline_layout(
            _context, 
            desc.binding_layouts, 
            vk_push_constant_visibility, 
            vk_pipeline_layout,
            push_constant_size
        ));


        vk::ShaderModule cs;

        vk::ShaderModuleCreateInfo vk_shader_module_info{};
        vk_shader_module_info.pNext = nullptr;
        vk_shader_module_info.flags = vk::ShaderModuleCreateFlags();
        vk_shader_module_info.codeSize = desc.compute_shader->get_byte_code().size;
        vk_shader_module_info.pCode = reinterpret_cast<uint32_t*>(desc.compute_shader->get_byte_code().byte_code);
        ReturnIfFalse(_context->device.createShaderModule(&vk_shader_module_info, nullptr, &cs) == vk::Result::eSuccess);

        vk::PipelineShaderStageCreateInfo vk_shader_stage_info{};
        vk_shader_stage_info.pNext = nullptr;
        vk_shader_stage_info.flags = vk::PipelineShaderStageCreateFlags();
        vk_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
        vk_shader_stage_info.module = cs;
        vk_shader_stage_info.pName = desc.compute_shader->get_desc().entry.c_str();
        vk_shader_stage_info.pSpecializationInfo = nullptr;

        
        vk::ComputePipelineCreateInfo vk_pipeline_info{};
        vk_pipeline_info.pNext = nullptr;
        vk_pipeline_info.flags = vk::PipelineCreateFlags();
        vk_pipeline_info.stage = vk_shader_stage_info;
        vk_pipeline_info.layout = vk_pipeline_layout;
        vk_pipeline_info.basePipelineHandle = nullptr;
        vk_pipeline_info.basePipelineIndex = -1;

        return vk::Result::eSuccess == _context->device.createComputePipelines(
            _context->vk_pipeline_cache,
            1, 
            &vk_pipeline_info,
            _context->allocation_callbacks,
            &vk_pipeline
        );
    }

    const ComputePipelineDesc& VKComputePipeline::get_desc() const
    { 
        return desc; 
    }

    void* VKComputePipeline::get_native_object()
    { 
        return vk_pipeline; 
    }
}