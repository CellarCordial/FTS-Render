#ifndef DYNAMIC_RHI_VULKAN_CONVERT_H
#define DYNAMIC_RHI_VULKAN_CONVERT_H

#include <vulkan/vulkan.hpp>
#include "../resource.h"
#include "../ray_tracing.h"


namespace fantasy 
{
    struct ResourceStateMapping
    {
        ResourceStates state;         
        vk::PipelineStageFlags vk_stage_flags;
        vk::AccessFlags vk_access_flags;
        vk::ImageLayout vk_image_layout;
    };

    ResourceStateMapping convert_resource_state(ResourceStates state);
    vk::SamplerAddressMode convert_sampler_address_mode(SamplerAddressMode mode);

    vk::PipelineStageFlagBits convert_shader_type_to_pipeline_stage_flag_bits(ShaderType shader_type);
    vk::ShaderStageFlagBits convert_shader_type_to_shader_stage_flag_bits(ShaderType shader_type);
    vk::PrimitiveTopology convert_primitive_topology(PrimitiveType topology);
    vk::PolygonMode convert_fill_mode(RasterFillMode mode);
    vk::CullModeFlagBits convert_cull_mode(RasterCullMode mode);
    vk::CompareOp convert_compare_op(ComparisonFunc op);
    vk::StencilOp convert_stencil_op(StencilOP op);
    vk::StencilOpState convert_stencil_state(const DepthStencilState& depthStencilState, const StencilOPDesc& desc);
    vk::BlendFactor convert_blend_value(BlendFactor value);
    vk::BlendOp convert_blend_op(BlendOP op);
    vk::ColorComponentFlags convert_color_mask(ColorMask mask);
    vk::PipelineColorBlendAttachmentState convert_blend_state(const BlendState::RenderTarget& state);

    vk::Format convert_format(Format format);

    vk::ImageType convert_texture_dimension_to_image_type(TextureDimension dimension);
    vk::ImageViewType convert_texture_dimension_to_image_view_type(TextureDimension dimension);
    vk::ImageUsageFlags get_image_usage_flag(const TextureDesc& desc);
    vk::SampleCountFlagBits get_sample_count_flag(uint32_t sample_count);
    vk::ImageCreateFlags get_image_create_flag(TextureDimension dimension, bool is_tiled);
    vk::ImageCreateInfo convert_image_info(const TextureDesc& desc);
    vk::BufferCreateInfo convert_buffer_info(const BufferDesc& desc);
    vk::BufferUsageFlags get_buffer_usage(const BufferDesc& desc);
    vk::SamplerCreateInfo convert_sampler_info(const SamplerDesc& desc);

    vk::BuildAccelerationStructureFlagsKHR convert_accel_struct_build_flags(ray_tracing::AccelStructBuildFlags buildFlags);
    vk::GeometryInstanceFlagsKHR convert_instance_flags(ray_tracing::InstanceFlags instanceFlags);
}






#endif