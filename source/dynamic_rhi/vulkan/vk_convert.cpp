#include "vk_convert.h"

namespace fantasy
{
    vk::SamplerAddressMode convert_sampler_address_mode(SamplerAddressMode mode)
    {
        switch(mode)
        {
            case SamplerAddressMode::Clamp: return vk::SamplerAddressMode::eClampToEdge;
            case SamplerAddressMode::Wrap: return vk::SamplerAddressMode::eRepeat;
            case SamplerAddressMode::Border: return vk::SamplerAddressMode::eClampToBorder;
            case SamplerAddressMode::Mirror: return vk::SamplerAddressMode::eMirroredRepeat;
            case SamplerAddressMode::MirrorOnce: return vk::SamplerAddressMode::eMirrorClampToEdge;
            default:
                assert(false && "Invalid enum.");
                return vk::SamplerAddressMode(0);
        }
    }

    vk::PipelineStageFlagBits convert_shader_type_to_pipeline_stage_flag_bits(ShaderType shader_type)
    {
        if (shader_type == ShaderType::All) return vk::PipelineStageFlagBits::eAllCommands;

        uint32_t ret = 0;

        if ((shader_type & ShaderType::Compute) != 0)        ret |= uint32_t(vk::PipelineStageFlagBits::eComputeShader);
        if ((shader_type & ShaderType::Vertex) != 0)         ret |= uint32_t(vk::PipelineStageFlagBits::eVertexShader);
        if ((shader_type & ShaderType::Hull) != 0)           ret |= uint32_t(vk::PipelineStageFlagBits::eTessellationControlShader);
        if ((shader_type & ShaderType::Domain) != 0)         ret |= uint32_t(vk::PipelineStageFlagBits::eTessellationEvaluationShader);
        if ((shader_type & ShaderType::Geometry) != 0)       ret |= uint32_t(vk::PipelineStageFlagBits::eGeometryShader);
        if ((shader_type & ShaderType::Pixel) != 0)          ret |= uint32_t(vk::PipelineStageFlagBits::eFragmentShader);
        if ((shader_type & ShaderType::RayTracing) != 0)     ret |= uint32_t(vk::PipelineStageFlagBits::eRayTracingShaderKHR);

        return static_cast<vk::PipelineStageFlagBits>(ret);
    }

    vk::ShaderStageFlagBits convert_shader_type_to_shader_stage_flag_bits(ShaderType shaderType)
    {
        if (shaderType == ShaderType::All) return vk::ShaderStageFlagBits::eAll;
        if (shaderType == ShaderType::Graphics) return vk::ShaderStageFlagBits::eAllGraphics;

        uint32_t ret = 0;

        if ((shaderType & ShaderType::Compute) != 0)        ret |= uint32_t(vk::ShaderStageFlagBits::eCompute);
        if ((shaderType & ShaderType::Vertex) != 0)         ret |= uint32_t(vk::ShaderStageFlagBits::eVertex);
        if ((shaderType & ShaderType::Hull) != 0)           ret |= uint32_t(vk::ShaderStageFlagBits::eTessellationControl);
        if ((shaderType & ShaderType::Domain) != 0)         ret |= uint32_t(vk::ShaderStageFlagBits::eTessellationEvaluation);
        if ((shaderType & ShaderType::Geometry) != 0)       ret |= uint32_t(vk::ShaderStageFlagBits::eGeometry);
        if ((shaderType & ShaderType::Pixel) != 0)          ret |= uint32_t(vk::ShaderStageFlagBits::eFragment);

        if ((shaderType & ShaderType::RayGeneration) != 0)  ret |= uint32_t(vk::ShaderStageFlagBits::eRaygenKHR);
        if ((shaderType & ShaderType::Miss) != 0)           ret |= uint32_t(vk::ShaderStageFlagBits::eMissKHR);
        if ((shaderType & ShaderType::ClosestHit) != 0)     ret |= uint32_t(vk::ShaderStageFlagBits::eClosestHitKHR);
        if ((shaderType & ShaderType::AnyHit) != 0)         ret |= uint32_t(vk::ShaderStageFlagBits::eAnyHitKHR);
        if ((shaderType & ShaderType::Intersection) != 0)   ret |= uint32_t(vk::ShaderStageFlagBits::eIntersectionKHR);

        return static_cast<vk::ShaderStageFlagBits>(ret);
    }

    struct ResourceStateMappingInternal
    {
        ResourceStates nvrhiState;
        vk::PipelineStageFlags stageFlags;
        vk::AccessFlags accessMask;
        vk::ImageLayout imageLayout;

        ResourceStateMapping AsResourceStateMapping() const 
        {
            // It's safe to cast vk::AccessFlags2 -> vk::AccessFlags and vk::PipelineStageFlags2 -> vk::PipelineStageFlags (as long as the enum exist in both versions!),
            // synchronization2 spec says: "The new flags are identical to the old values within the 32-bit range, with new stages and bits beyond that."
            // The below stages are exclustive to synchronization2
            assert((stageFlags & vk::PipelineStageFlagBits::eMicromapBuildEXT) != vk::PipelineStageFlagBits::eMicromapBuildEXT);
            assert((accessMask & vk::AccessFlagBits2::eMicromapWriteEXT) != vk::AccessFlagBits2::eMicromapWriteEXT);
            return
                ResourceStateMapping(nvrhiState,
                    reinterpret_cast<const vk::PipelineStageFlags&>(stageFlags),
                    reinterpret_cast<const vk::AccessFlags&>(accessMask),
                    imageLayout
                );
        }
    };

    static const ResourceStateMappingInternal g_ResourceStateMap[] =
    {
        { 
            ResourceStates::Common,
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::AccessFlagBits(),
            vk::ImageLayout::eUndefined 
        },
        { 
            ResourceStates::ConstantBuffer,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::AccessFlagBits::eUniformRead,
            vk::ImageLayout::eUndefined 
        },
        { 
            ResourceStates::VertexBuffer,
            vk::PipelineStageFlagBits::eVertexInput,
            vk::AccessFlagBits::eVertexAttributeRead,
            vk::ImageLayout::eUndefined 
        },
        { 
            ResourceStates::IndexBuffer,
            vk::PipelineStageFlagBits::eVertexInput,
            vk::AccessFlagBits::eIndexRead,
            vk::ImageLayout::eUndefined 
        },
        { 
            ResourceStates::IndirectArgument,
            vk::PipelineStageFlagBits::eDrawIndirect,
            vk::AccessFlagBits::eIndirectCommandRead,
            vk::ImageLayout::eUndefined 
        },
        { 
            ResourceStates::PixelShaderResource,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::AccessFlagBits::eShaderRead,
            vk::ImageLayout::eShaderReadOnlyOptimal 
        },
        { 
            ResourceStates::NonPixelShaderResource,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::AccessFlagBits::eShaderRead,
            vk::ImageLayout::eShaderReadOnlyOptimal 
        },
        { 
            ResourceStates::UnorderedAccess,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
            vk::ImageLayout::eGeneral
        },
        { 
            ResourceStates::RenderTarget,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
            vk::ImageLayout::eColorAttachmentOptimal 
        },
        { 
            ResourceStates::DepthWrite,
            vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
            vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            vk::ImageLayout::eDepthStencilAttachmentOptimal 
        },
        { 
            ResourceStates::DepthRead,
            vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
            vk::AccessFlagBits::eDepthStencilAttachmentRead,
            vk::ImageLayout::eDepthStencilReadOnlyOptimal 
        },
        { 
            ResourceStates::StreamOut,
            vk::PipelineStageFlagBits::eTransformFeedbackEXT,
            vk::AccessFlagBits::eTransformFeedbackWriteEXT,
            vk::ImageLayout::eUndefined 
        },
        { 
            ResourceStates::CopyDest,
            vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eTransferWrite,
            vk::ImageLayout::eTransferDstOptimal 
        },
        { 
            ResourceStates::CopySource,
            vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eTransferRead,
            vk::ImageLayout::eTransferSrcOptimal 
        },
        { 
            ResourceStates::ResolveDst,
            vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eTransferWrite,
            vk::ImageLayout::eTransferDstOptimal 
        },
        { 
            ResourceStates::ResolveSrc,
            vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eTransferRead,
            vk::ImageLayout::eTransferSrcOptimal 
        },
        { 
            ResourceStates::Present,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::AccessFlagBits::eMemoryRead,
            vk::ImageLayout::ePresentSrcKHR 
        },
        { 
            ResourceStates::AccelStructRead,
            vk::PipelineStageFlagBits::eRayTracingShaderKHR | vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eAccelerationStructureReadKHR,
            vk::ImageLayout::eUndefined 
        },
        { 
            ResourceStates::AccelStructWrite,
            vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
            vk::AccessFlagBits::eAccelerationStructureWriteKHR,
            vk::ImageLayout::eUndefined 
        },
        {
            ResourceStates::AccelStructBuildInput,
            vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
            vk::AccessFlagBits::eAccelerationStructureReadKHR,
            vk::ImageLayout::eUndefined 
        },
        { 
            ResourceStates::AccelStructBuildBlas,
            vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
            vk::AccessFlagBits::eAccelerationStructureReadKHR,
            vk::ImageLayout::eUndefined 
        }
    };

    ResourceStateMappingInternal convertResourceStateInternal(ResourceStates state)
    {
        ResourceStateMappingInternal result = {};

        constexpr uint32_t numStateBits = sizeof(g_ResourceStateMap) / sizeof(g_ResourceStateMap[0]);

        uint32_t stateTmp = uint32_t(state);
        uint32_t bitIndex = 0;

        while (stateTmp != 0 && bitIndex < numStateBits)
        {
            uint32_t bit = (1 << bitIndex);

            if (stateTmp & bit)
            {
                const ResourceStateMappingInternal& mapping = g_ResourceStateMap[bitIndex];

                assert(uint32_t(mapping.nvrhiState) == bit);
                assert(result.imageLayout == vk::ImageLayout::eUndefined || mapping.imageLayout == vk::ImageLayout::eUndefined || result.imageLayout == mapping.imageLayout);

                result.nvrhiState = ResourceStates(result.nvrhiState | mapping.nvrhiState);
                result.accessMask |= mapping.accessMask;
                result.stageFlags |= mapping.stageFlags;
                if (mapping.imageLayout != vk::ImageLayout::eUndefined)
                    result.imageLayout = mapping.imageLayout;

                stateTmp &= ~bit;
            }

            bitIndex++;
        }

        assert(result.nvrhiState == state);

        return result;
    }

    ResourceStateMapping convert_resource_state(ResourceStates state)
    {
        const ResourceStateMappingInternal mapping = convertResourceStateInternal(state);
        return mapping.AsResourceStateMapping();
    }

    vk::PrimitiveTopology convert_primitive_topology(PrimitiveType topology)
    {
        switch(topology)
        {
            case PrimitiveType::PointList: return vk::PrimitiveTopology::ePointList;
            case PrimitiveType::LineList: return vk::PrimitiveTopology::eLineList;
            case PrimitiveType::TriangleList: return vk::PrimitiveTopology::eTriangleList;
            case PrimitiveType::TriangleStrip: return vk::PrimitiveTopology::eTriangleStrip;
            case PrimitiveType::TriangleListWithAdjacency: return vk::PrimitiveTopology::eTriangleListWithAdjacency;
            case PrimitiveType::TriangleStripWithAdjacency: return vk::PrimitiveTopology::eTriangleStripWithAdjacency;
            case PrimitiveType::PatchList: return vk::PrimitiveTopology::ePatchList;
            default:
                assert(false && "Invalid enum.");
                return vk::PrimitiveTopology::eTriangleList;
        }
    }

    vk::PolygonMode convert_fill_mode(RasterFillMode mode)
    {
        switch(mode)
        {
            case RasterFillMode::Solid: return vk::PolygonMode::eFill;
            case RasterFillMode::Wireframe: return vk::PolygonMode::eLine;
            default:
                assert(false && "Invalid enum.");
                return vk::PolygonMode::eFill;
        }
    }

    vk::CullModeFlagBits convert_cull_mode(RasterCullMode mode)
    {
        switch(mode)
        {
            case RasterCullMode::Back: return vk::CullModeFlagBits::eBack;
            case RasterCullMode::Front: return vk::CullModeFlagBits::eFront;
            case RasterCullMode::None: return vk::CullModeFlagBits::eNone;
            default:
                assert(false && "Invalid enum.");
                return vk::CullModeFlagBits::eNone;
        }
    }

    vk::CompareOp convert_compare_op(ComparisonFunc op)
    {
        switch(op)
        {
            case ComparisonFunc::Never: return vk::CompareOp::eNever;
            case ComparisonFunc::Less: return vk::CompareOp::eLess;
            case ComparisonFunc::Equal: return vk::CompareOp::eEqual;
            case ComparisonFunc::LessOrEqual: return vk::CompareOp::eLessOrEqual;
            case ComparisonFunc::Greater: return vk::CompareOp::eGreater;
            case ComparisonFunc::NotEqual: return vk::CompareOp::eNotEqual;
            case ComparisonFunc::GreaterOrEqual: return vk::CompareOp::eGreaterOrEqual;
            case ComparisonFunc::Always: return vk::CompareOp::eAlways;
            default:
                assert(false && "Invalid enum.");
                return vk::CompareOp::eAlways;
        }
    }

    vk::StencilOp convert_stencil_op(StencilOP op)
    {
        switch(op)
        {
            case StencilOP::Keep: return vk::StencilOp::eKeep;
            case StencilOP::Zero: return vk::StencilOp::eZero;
            case StencilOP::Replace: return vk::StencilOp::eReplace;
            case StencilOP::IncrementAndClamp: return vk::StencilOp::eIncrementAndClamp;
            case StencilOP::DecrementAndClamp: return vk::StencilOp::eDecrementAndClamp;
            case StencilOP::Invert: return vk::StencilOp::eInvert;
            case StencilOP::IncrementAndWrap: return vk::StencilOp::eIncrementAndWrap;
            case StencilOP::DecrementAndWrap: return vk::StencilOp::eDecrementAndWrap;
            default:
                assert(false && "Invalid enum.");
                return vk::StencilOp::eKeep;
        }
    }

    vk::StencilOpState convert_stencil_state(const DepthStencilState& depthStencilState, const StencilOPDesc& desc)
    {
        vk::StencilOpState ret{};
        ret.failOp = convert_stencil_op(desc.fail_op);
        ret.passOp = convert_stencil_op(desc.pass_op);
        ret.depthFailOp = convert_stencil_op(desc.depth_fail_op);
        ret.compareOp = convert_compare_op(desc.stencil_func);   
        ret.compareMask = depthStencilState.stencil_read_mask;
        ret.writeMask = depthStencilState.stencil_write_mask;
        ret.reference = depthStencilState.stencil_ref_value;   
        return ret;
    }

    vk::BlendFactor convert_blend_value(BlendFactor value)
    {
        switch(value)
        {
            case BlendFactor::Zero: return vk::BlendFactor::eZero;
            case BlendFactor::One: return vk::BlendFactor::eOne;
            case BlendFactor::SrcColor: return vk::BlendFactor::eSrcColor;
            case BlendFactor::InvSrcColor: return vk::BlendFactor::eOneMinusSrcColor;
            case BlendFactor::SrcAlpha: return vk::BlendFactor::eSrcAlpha;
            case BlendFactor::InvSrcAlpha: return vk::BlendFactor::eOneMinusSrcAlpha;
            case BlendFactor::DstAlpha: return vk::BlendFactor::eDstAlpha;
            case BlendFactor::InvDstAlpha: return vk::BlendFactor::eOneMinusDstAlpha;
            case BlendFactor::DstColor: return vk::BlendFactor::eDstColor;
            case BlendFactor::InvDstColor: return vk::BlendFactor::eOneMinusDstColor;
            case BlendFactor::SrcAlphaSaturate: return vk::BlendFactor::eSrcAlphaSaturate;
            case BlendFactor::ConstantColor: return vk::BlendFactor::eConstantColor;
            case BlendFactor::InvConstantColor: return vk::BlendFactor::eOneMinusConstantColor;
            case BlendFactor::Src1Color: return vk::BlendFactor::eSrc1Color;
            case BlendFactor::InvSrc1Color: return vk::BlendFactor::eOneMinusSrc1Color;
            case BlendFactor::Src1Alpha: return vk::BlendFactor::eSrc1Alpha;
            case BlendFactor::InvSrc1Alpha: return vk::BlendFactor::eOneMinusSrc1Alpha;
            default:
                assert(false && "Invalid enum.");
                return vk::BlendFactor::eZero;
        }
    }

    vk::BlendOp convert_blend_op(BlendOP op)
    {
        switch(op)
        {
            case BlendOP::Add:              return vk::BlendOp::eAdd;
            case BlendOP::Subtract:         return vk::BlendOp::eSubtract;
            case BlendOP::ReverseSubtract:  return vk::BlendOp::eReverseSubtract;
            case BlendOP::Min:              return vk::BlendOp::eMin;
            case BlendOP::Max:              return vk::BlendOp::eMax;

            default: assert(false && "Invalid enum."); return vk::BlendOp::eAdd;
        }
    }

    vk::ColorComponentFlags convert_color_mask(ColorMask mask)
    {
        return vk::ColorComponentFlags(uint8_t(mask));
    }

    vk::PipelineColorBlendAttachmentState convert_blend_state(const BlendState::RenderTarget& state)
    {
        vk::PipelineColorBlendAttachmentState ret{};
        ret.blendEnable = state.enable_blend;        
        ret.srcColorBlendFactor = convert_blend_value(state.src_blend);
        ret.dstColorBlendFactor = convert_blend_value(state.dst_blend);
        ret.colorBlendOp = convert_blend_op(state.blend_op);
        ret.srcAlphaBlendFactor = convert_blend_value(state.src_blend_alpha);
        ret.dstAlphaBlendFactor = convert_blend_value(state.dst_blend_alpha);
        ret.alphaBlendOp = convert_blend_op(state.blend_op_alpha);           
        ret.colorWriteMask = convert_color_mask(state.color_write_mask);       
        return ret;
    }

    vk::BuildAccelerationStructureFlagsKHR convert_accel_struct_build_flags(ray_tracing::AccelStructBuildFlags buildFlags)
    {
        vk::BuildAccelerationStructureFlagsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR(0);
        if ((buildFlags & ray_tracing::AccelStructBuildFlags::AllowUpdate) != 0)
            flags |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
        if ((buildFlags & ray_tracing::AccelStructBuildFlags::AllowCompaction) != 0)
            flags |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction;
        if ((buildFlags & ray_tracing::AccelStructBuildFlags::PreferFastTrace) != 0)
            flags |= vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
        if ((buildFlags & ray_tracing::AccelStructBuildFlags::PreferFastBuild) != 0)
            flags |= vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild;
        if ((buildFlags & ray_tracing::AccelStructBuildFlags::MinimizeMemory) != 0)
            flags |= vk::BuildAccelerationStructureFlagBitsKHR::eLowMemory;
        return flags;
    }

    vk::GeometryInstanceFlagsKHR convert_instance_flags(ray_tracing::InstanceFlags instanceFlags)
    {
        vk::GeometryInstanceFlagsKHR flags = vk::GeometryInstanceFlagBitsKHR(0);
        if ((instanceFlags & ray_tracing::InstanceFlags::ForceNonOpaque) != 0)
            flags |= vk::GeometryInstanceFlagBitsKHR::eForceNoOpaque;
        if ((instanceFlags & ray_tracing::InstanceFlags::ForceOpaque) != 0)
            flags |= vk::GeometryInstanceFlagBitsKHR::eForceOpaque;
        if ((instanceFlags & ray_tracing::InstanceFlags::TriangleCullDisable) != 0)
            flags |= vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable;
        if ((instanceFlags & ray_tracing::InstanceFlags::TriangleFrontCounterclockwise) != 0)
            flags |= vk::GeometryInstanceFlagBitsKHR::eTriangleFrontCounterclockwise;
        return flags;
    }

    vk::ImageType convert_texture_dimension(TextureDimension dimension)
    {
        switch (dimension)
        {
        case TextureDimension::Texture1D:
        case TextureDimension::Texture1DArray:
            return vk::ImageType::e1D;

        case TextureDimension::Texture2D:
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:
        case TextureDimension::Texture2DMS:
        case TextureDimension::Texture2DMSArray:
            return vk::ImageType::e2D;

        case TextureDimension::Texture3D:
            return vk::ImageType::e3D;

        case TextureDimension::Unknown:
        default:
            assert(false && "Invalid enum.");
            return vk::ImageType::e2D;
        }
    }

    vk::ImageUsageFlags get_image_usage_flag(const TextureDesc& desc)
    {
        const FormatInfo& format_info = get_format_info(desc.format);
        
        vk::ImageUsageFlags ret = vk::ImageUsageFlagBits::eTransferSrc |
                                  vk::ImageUsageFlagBits::eTransferDst;
        
        if (desc.is_shader_resource) ret |= vk::ImageUsageFlagBits::eSampled;

        if (desc.is_render_target)
        {
            if (format_info.has_depth || format_info.has_stencil)
                ret |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
            else 
                ret |= vk::ImageUsageFlagBits::eColorAttachment;
        }

        if (desc.is_uav) ret |= vk::ImageUsageFlagBits::eStorage;

        return ret;
    }

    vk::SampleCountFlagBits get_sample_count_flag(uint32_t sample_count)
    {
        switch(sample_count)
        {
            case 1: return vk::SampleCountFlagBits::e1;
            case 2: return vk::SampleCountFlagBits::e2;
            case 4: return vk::SampleCountFlagBits::e4;
            case 8: return vk::SampleCountFlagBits::e8;
            case 16: return vk::SampleCountFlagBits::e16;
            case 32: return vk::SampleCountFlagBits::e32;
            case 64: return vk::SampleCountFlagBits::e64;
            default:
                assert(false && "Invalid enum.");
                return vk::SampleCountFlagBits::e1;
        }
    }

    vk::ImageCreateFlags get_image_create_flag(const TextureDesc& desc)
    {
        vk::ImageCreateFlags flags = vk::ImageCreateFlags(0);

        if (
            desc.dimension == TextureDimension::TextureCube || 
            desc.dimension == TextureDimension::TextureCubeArray
        )
            flags |= vk::ImageCreateFlagBits::eCubeCompatible;

        return flags;
    }


    struct VKFormatMapping
    {
        Format format;
        VkFormat vk_format;
    };

    static const std::array<VKFormatMapping, static_cast<size_t>(Format::NUM)> format_map = 
    {{
        { Format::UNKNOWN,           VK_FORMAT_UNDEFINED                },
        { Format::R8_UINT,           VK_FORMAT_R8_UINT                  },
        { Format::R8_SINT,           VK_FORMAT_R8_SINT                  },
        { Format::R8_UNORM,          VK_FORMAT_R8_UNORM                 },
        { Format::R8_SNORM,          VK_FORMAT_R8_SNORM                 },
        { Format::RG8_UINT,          VK_FORMAT_R8G8_UINT                },
        { Format::RG8_SINT,          VK_FORMAT_R8G8_SINT                },
        { Format::RG8_UNORM,         VK_FORMAT_R8G8_UNORM               },
        { Format::RG8_SNORM,         VK_FORMAT_R8G8_SNORM               },
        { Format::R16_UINT,          VK_FORMAT_R16_UINT                 },
        { Format::R16_SINT,          VK_FORMAT_R16_SINT                 },
        { Format::R16_UNORM,         VK_FORMAT_R16_UNORM                },
        { Format::R16_SNORM,         VK_FORMAT_R16_SNORM                },
        { Format::R16_FLOAT,         VK_FORMAT_R16_SFLOAT               },
        { Format::BGRA4_UNORM,       VK_FORMAT_B4G4R4A4_UNORM_PACK16    },
        { Format::B5G6R5_UNORM,      VK_FORMAT_B5G6R5_UNORM_PACK16      },
        { Format::B5G5R5A1_UNORM,    VK_FORMAT_B5G5R5A1_UNORM_PACK16    },
        { Format::RGBA8_UINT,        VK_FORMAT_R8G8B8A8_UINT            },
        { Format::RGBA8_SINT,        VK_FORMAT_R8G8B8A8_SINT            },
        { Format::RGBA8_UNORM,       VK_FORMAT_R8G8B8A8_UNORM           },
        { Format::RGBA8_SNORM,       VK_FORMAT_R8G8B8A8_SNORM           },
        { Format::BGRA8_UNORM,       VK_FORMAT_B8G8R8A8_UNORM           },
        { Format::SRGBA8_UNORM,      VK_FORMAT_R8G8B8A8_SRGB            },
        { Format::SBGRA8_UNORM,      VK_FORMAT_B8G8R8A8_SRGB            },
        { Format::R10G10B10A2_UNORM, VK_FORMAT_A2B10G10R10_UNORM_PACK32 },
        { Format::R11G11B10_FLOAT,   VK_FORMAT_B10G11R11_UFLOAT_PACK32  },
        { Format::RG16_UINT,         VK_FORMAT_R16G16_UINT              },
        { Format::RG16_SINT,         VK_FORMAT_R16G16_SINT              },
        { Format::RG16_UNORM,        VK_FORMAT_R16G16_UNORM             },
        { Format::RG16_SNORM,        VK_FORMAT_R16G16_SNORM             },
        { Format::RG16_FLOAT,        VK_FORMAT_R16G16_SFLOAT            },
        { Format::R32_UINT,          VK_FORMAT_R32_UINT                 },
        { Format::R32_SINT,          VK_FORMAT_R32_SINT                 },
        { Format::R32_FLOAT,         VK_FORMAT_R32_SFLOAT               },
        { Format::RGBA16_UINT,       VK_FORMAT_R16G16B16A16_UINT        },
        { Format::RGBA16_SINT,       VK_FORMAT_R16G16B16A16_SINT        },
        { Format::RGBA16_FLOAT,      VK_FORMAT_R16G16B16A16_SFLOAT      },
        { Format::RGBA16_UNORM,      VK_FORMAT_R16G16B16A16_UNORM       },
        { Format::RGBA16_SNORM,      VK_FORMAT_R16G16B16A16_SNORM       },
        { Format::RG32_UINT,         VK_FORMAT_R32G32_UINT              },
        { Format::RG32_SINT,         VK_FORMAT_R32G32_SINT              },
        { Format::RG32_FLOAT,        VK_FORMAT_R32G32_SFLOAT            },
        { Format::RGB32_UINT,        VK_FORMAT_R32G32B32_UINT           },
        { Format::RGB32_SINT,        VK_FORMAT_R32G32B32_SINT           },
        { Format::RGB32_FLOAT,       VK_FORMAT_R32G32B32_SFLOAT         },
        { Format::RGBA32_UINT,       VK_FORMAT_R32G32B32A32_UINT        },
        { Format::RGBA32_SINT,       VK_FORMAT_R32G32B32A32_SINT        },
        { Format::RGBA32_FLOAT,      VK_FORMAT_R32G32B32A32_SFLOAT      },
        { Format::D16,               VK_FORMAT_D16_UNORM                },
        { Format::D24S8,             VK_FORMAT_D24_UNORM_S8_UINT        },
        { Format::X24G8_UINT,        VK_FORMAT_D24_UNORM_S8_UINT        },
        { Format::D32,               VK_FORMAT_D32_SFLOAT               },
        { Format::D32S8,             VK_FORMAT_D32_SFLOAT_S8_UINT       },
        { Format::X32G8_UINT,        VK_FORMAT_D32_SFLOAT_S8_UINT       },
        { Format::BC1_UNORM,         VK_FORMAT_BC1_RGBA_UNORM_BLOCK     },
        { Format::BC1_UNORM_SRGB,    VK_FORMAT_BC1_RGBA_SRGB_BLOCK      },
        { Format::BC2_UNORM,         VK_FORMAT_BC2_UNORM_BLOCK          },
        { Format::BC2_UNORM_SRGB,    VK_FORMAT_BC2_SRGB_BLOCK           },
        { Format::BC3_UNORM,         VK_FORMAT_BC3_UNORM_BLOCK          },
        { Format::BC3_UNORM_SRGB,    VK_FORMAT_BC3_SRGB_BLOCK           },
        { Format::BC4_UNORM,         VK_FORMAT_BC4_UNORM_BLOCK          },
        { Format::BC4_SNORM,         VK_FORMAT_BC4_SNORM_BLOCK          },
        { Format::BC5_UNORM,         VK_FORMAT_BC5_UNORM_BLOCK          },
        { Format::BC5_SNORM,         VK_FORMAT_BC5_SNORM_BLOCK          },
        { Format::BC6H_UFLOAT,       VK_FORMAT_BC6H_UFLOAT_BLOCK        },
        { Format::BC6H_SFLOAT,       VK_FORMAT_BC6H_SFLOAT_BLOCK        },
        { Format::BC7_UNORM,         VK_FORMAT_BC7_UNORM_BLOCK          },
        { Format::BC7_UNORM_SRGB,    VK_FORMAT_BC7_SRGB_BLOCK           },
    }};

    VkFormat convert_format(Format format)
    {
        assert(format < Format::NUM);
        assert(c_FormatMap[uint32_t(format)].rhiFormat == format);

        return format_map[uint32_t(format)].vk_format;
    }
}