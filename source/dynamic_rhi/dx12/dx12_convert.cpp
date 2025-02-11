#include "dx12_convert.h"
#include <algorithm>
#include <cstring>
#include <d3d12.h>
#include <minwindef.h>



namespace fantasy
{
    D3D12_SHADER_VISIBILITY convert_shader_stage(ShaderType shader_visibility)
    {
        switch (shader_visibility)  // NOLINT(clang-diagnostic-switch-enum)
        {
        case ShaderType::Vertex       : return D3D12_SHADER_VISIBILITY_VERTEX;
        case ShaderType::Hull         : return D3D12_SHADER_VISIBILITY_HULL;
        case ShaderType::Domain       : return D3D12_SHADER_VISIBILITY_DOMAIN;
        case ShaderType::Geometry     : return D3D12_SHADER_VISIBILITY_GEOMETRY;
        case ShaderType::Pixel        : return D3D12_SHADER_VISIBILITY_PIXEL;

        default:
            // catch-all case - actually some of the bitfield combinations are unrepresentable in DX12
            return D3D12_SHADER_VISIBILITY_ALL;
        }
    }

    D3D12_BLEND convert_blend_value(BlendFactor factor)
    {
        switch (factor)
        {
        case BlendFactor::Zero            : return D3D12_BLEND_ZERO;
        case BlendFactor::One             : return D3D12_BLEND_ONE;
        case BlendFactor::SrcColor        : return D3D12_BLEND_SRC_COLOR;
        case BlendFactor::InvSrcColor     : return D3D12_BLEND_INV_SRC_COLOR;
        case BlendFactor::SrcAlpha        : return D3D12_BLEND_SRC_ALPHA;
        case BlendFactor::InvSrcAlpha     : return D3D12_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DstAlpha        : return D3D12_BLEND_DEST_ALPHA;
        case BlendFactor::InvDstAlpha     : return D3D12_BLEND_INV_DEST_ALPHA;
        case BlendFactor::DstColor        : return D3D12_BLEND_DEST_COLOR;
        case BlendFactor::InvDstColor     : return D3D12_BLEND_INV_DEST_COLOR;
        case BlendFactor::SrcAlphaSaturate: return D3D12_BLEND_SRC_ALPHA_SAT;
        case BlendFactor::ConstantColor   : return D3D12_BLEND_BLEND_FACTOR;
        case BlendFactor::InvConstantColor: return D3D12_BLEND_INV_BLEND_FACTOR;
        case BlendFactor::Src1Color       : return D3D12_BLEND_SRC1_COLOR;
        case BlendFactor::InvSrc1Color    : return D3D12_BLEND_INV_SRC1_COLOR;
        case BlendFactor::Src1Alpha       : return D3D12_BLEND_SRC1_ALPHA;
        case BlendFactor::InvSrc1Alpha    : return D3D12_BLEND_INV_SRC1_ALPHA;
        }
        return D3D12_BLEND_ZERO;
    }

    D3D12_BLEND_OP convert_blend_op(BlendOP op)
    {
        switch (op)
        {
        case BlendOP::Add            : return D3D12_BLEND_OP_ADD;
        case BlendOP::Subtract       : return D3D12_BLEND_OP_SUBTRACT;
        case BlendOP::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
        case BlendOP::Min            : return D3D12_BLEND_OP_MIN;
        case BlendOP::Max            : return D3D12_BLEND_OP_MAX;
        }
        return D3D12_BLEND_OP_ADD;
    }

    D3D12_STENCIL_OP convert_stencil_op(StencilOP op)
    {
        switch (op)
        {
        case StencilOP::Keep             : return D3D12_STENCIL_OP_KEEP;
        case StencilOP::Zero             : return D3D12_STENCIL_OP_ZERO;
        case StencilOP::Replace          : return D3D12_STENCIL_OP_REPLACE;
        case StencilOP::IncrementAndClamp: return D3D12_STENCIL_OP_INCR_SAT;
        case StencilOP::DecrementAndClamp: return D3D12_STENCIL_OP_DECR_SAT;
        case StencilOP::Invert           : return D3D12_STENCIL_OP_INVERT;
        case StencilOP::IncrementAndWrap : return D3D12_STENCIL_OP_INCR;
        case StencilOP::DecrementAndWrap : return D3D12_STENCIL_OP_DECR;
        }
        return D3D12_STENCIL_OP_KEEP;
    }

    D3D12_COMPARISON_FUNC convert_comparison_func(ComparisonFunc func)
    {
        switch (func)
        {
        case ComparisonFunc::Never         : return D3D12_COMPARISON_FUNC_NEVER;
        case ComparisonFunc::Less          : return D3D12_COMPARISON_FUNC_LESS;
        case ComparisonFunc::Equal         : return D3D12_COMPARISON_FUNC_EQUAL;
        case ComparisonFunc::LessOrEqual   : return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case ComparisonFunc::Greater       : return D3D12_COMPARISON_FUNC_GREATER;
        case ComparisonFunc::NotEqual      : return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case ComparisonFunc::GreaterOrEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case ComparisonFunc::Always        : return D3D12_COMPARISON_FUNC_ALWAYS;
        }
        return D3D12_COMPARISON_FUNC_NEVER;
    }

    D3D_PRIMITIVE_TOPOLOGY convert_primitive_type(PrimitiveType type, uint32_t control_points)
    {
        switch (type)
        {
        case PrimitiveType::PointList                 : return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveType::LineList                  : return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveType::TriangleList              : return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveType::TriangleStrip             : return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case PrimitiveType::TriangleListWithAdjacency : return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
        case PrimitiveType::TriangleStripWithAdjacency: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
        case PrimitiveType::PatchList:
            if (control_points == 0 || control_points > 32)
            {
                assert(!"invalid Enumeration value");
                return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
            }
            return static_cast<D3D_PRIMITIVE_TOPOLOGY>(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (control_points - 1));
        }
        return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }

    D3D12_TEXTURE_ADDRESS_MODE convert_sampler_address_mode(SamplerAddressMode mode)
    {
        switch (mode)
        {
        case SamplerAddressMode::Clamp     : return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case SamplerAddressMode::Wrap      : return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case SamplerAddressMode::Border    : return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case SamplerAddressMode::Mirror    : return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case SamplerAddressMode::MirrorOnce: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        }
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    }

    UINT convert_sampler_reduction_type(SamplerReductionType reduction_type)
    {
        switch (reduction_type)
        {
        case SamplerReductionType::Standard  : return D3D12_FILTER_REDUCTION_TYPE_STANDARD;
        case SamplerReductionType::Comparison: return D3D12_FILTER_REDUCTION_TYPE_COMPARISON;
        case SamplerReductionType::Minimum   : return D3D12_FILTER_REDUCTION_TYPE_MINIMUM;
        case SamplerReductionType::Maximum   : return D3D12_FILTER_REDUCTION_TYPE_MAXIMUM;
        }
        return D3D12_FILTER_REDUCTION_TYPE_STANDARD;
    }

    D3D12_RESOURCE_STATES convert_resource_states(ResourceStates states)
    {
        if (states == ResourceStates::Common) return D3D12_RESOURCE_STATE_COMMON;

        D3D12_RESOURCE_STATES ret = D3D12_RESOURCE_STATE_COMMON;

        if ((states & ResourceStates::ConstantBuffer) != 0) ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        if ((states & ResourceStates::VertexBuffer) != 0) ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        if ((states & ResourceStates::IndexBuffer) != 0) ret |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
        if ((states & ResourceStates::ShaderResource) != 0) ret |= D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
        if ((states & ResourceStates::UnorderedAccess) != 0) ret |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        if ((states & ResourceStates::RenderTarget) != 0) ret |= D3D12_RESOURCE_STATE_RENDER_TARGET;
        if ((states & ResourceStates::DepthWrite) != 0) ret |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
        if ((states & ResourceStates::DepthRead) != 0) ret |= D3D12_RESOURCE_STATE_DEPTH_READ;
        if ((states & ResourceStates::StreamOut) != 0) ret |= D3D12_RESOURCE_STATE_STREAM_OUT;
        if ((states & ResourceStates::CopyDst) != 0) ret |= D3D12_RESOURCE_STATE_COPY_DEST;
        if ((states & ResourceStates::CopySrc) != 0) ret |= D3D12_RESOURCE_STATE_COPY_SOURCE;
        if ((states & ResourceStates::Present) != 0) ret |= D3D12_RESOURCE_STATE_PRESENT;
        if ((states & ResourceStates::IndirectArgument) != 0) ret |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        // if ((states & ResourceStates::AccelStructRead) != 0) ret |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        // if ((states & ResourceStates::AccelStructWrite) != 0) ret |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        // if ((states & ResourceStates::AccelStructBuildBlas) != 0) ret |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        // if ((states & ResourceStates::AccelStructBuildInput) != 0) ret |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        return ret;
    }

    D3D12_BLEND_DESC convert_blend_state(const BlendState& state)
    {
        D3D12_BLEND_DESC ret{};
        ret.AlphaToCoverageEnable = state.enable_alpha_to_converage;
        ret.IndependentBlendEnable = false;

        for (uint32_t ix = 0; ix < MAX_RENDER_TARGETS; ix++)
        {
            const BlendState::RenderTarget& src = state.target_blends[ix];
            D3D12_RENDER_TARGET_BLEND_DESC& dst = ret.RenderTarget[ix];

            dst.LogicOpEnable = false;
            dst.LogicOp = D3D12_LOGIC_OP_NOOP;
            dst.BlendEnable = src.enable_blend;
            dst.SrcBlend = convert_blend_value(src.src_blend);
            dst.DestBlend = convert_blend_value(src.dst_blend);
            dst.BlendOp = convert_blend_op(src.blend_op);
            dst.SrcBlendAlpha = convert_blend_value(src.src_blend_alpha);
            dst.DestBlendAlpha = convert_blend_value(src.dst_blend_alpha);
            dst.BlendOpAlpha = convert_blend_op(src.blend_op_alpha);
            dst.RenderTargetWriteMask = static_cast<uint8_t>(src.color_write_mask);
        }
        return ret;
    }

    D3D12_DEPTH_STENCIL_DESC convert_depth_stencil_state(const DepthStencilState& state)
    {
        D3D12_DEPTH_STENCIL_DESC ret;
        ret.DepthEnable                  = state.enable_depth_test ? TRUE : FALSE;
        ret.DepthWriteMask               = state.enable_depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        ret.DepthFunc                    = convert_comparison_func(state.depth_func);
        ret.StencilEnable                = state.enable_stencil ? TRUE : FALSE;
        ret.StencilReadMask              = static_cast<uint8_t>(state.stencil_read_mask);
        ret.StencilWriteMask             = static_cast<uint8_t>(state.stencil_write_mask);
        ret.FrontFace.StencilFailOp      = convert_stencil_op(state.front_face_stencil.fail_op);
        ret.FrontFace.StencilDepthFailOp = convert_stencil_op(state.front_face_stencil.depth_fail_op);
        ret.FrontFace.StencilPassOp      = convert_stencil_op(state.front_face_stencil.pass_op);
        ret.FrontFace.StencilFunc        = convert_comparison_func(state.front_face_stencil.stencil_func);
        ret.BackFace.StencilFailOp       = convert_stencil_op(state.back_face_stencil.fail_op);
        ret.BackFace.StencilDepthFailOp  = convert_stencil_op(state.back_face_stencil.depth_fail_op);
        ret.BackFace.StencilPassOp       = convert_stencil_op(state.back_face_stencil.pass_op);
        ret.BackFace.StencilFunc         = convert_comparison_func(state.back_face_stencil.stencil_func);

        return ret;
    }

    D3D12_RASTERIZER_DESC convert_rasterizer_state(const RasterState& state)
    {
        D3D12_RASTERIZER_DESC ret;
        switch (state.fill_mode)
        {
        case RasterFillMode::Solid: ret.FillMode     = D3D12_FILL_MODE_SOLID; break;
        case RasterFillMode::Wireframe: ret.FillMode = D3D12_FILL_MODE_WIREFRAME; break;
        }

        switch (state.cull_mode)
        {
        case RasterCullMode::Back: ret.CullMode  = D3D12_CULL_MODE_BACK; break;
        case RasterCullMode::Front: ret.CullMode = D3D12_CULL_MODE_FRONT; break;
        case RasterCullMode::None: ret.CullMode  = D3D12_CULL_MODE_NONE; break;
        }

        ret.FrontCounterClockwise = state.front_counter_clock_wise;
        ret.DepthBias             = state.depth_bias;
        ret.DepthBiasClamp        = state.depth_bias_clamp;
        ret.SlopeScaledDepthBias  = state.slope_scale_depth_bias;
        ret.DepthClipEnable       = state.enable_depth_clip;
        ret.MultisampleEnable     = state.enable_multi_sample;
        ret.AntialiasedLineEnable = state.enable_anti_aliased_line;
        ret.ConservativeRaster    = state.enable_conservative_raster ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        ret.ForcedSampleCount     = state.forced_sample_count;

        return ret;
    }

    D3D12_PRIMITIVE_TOPOLOGY_TYPE convert_primitive_topology(PrimitiveType type)
    {
        D3D12_PRIMITIVE_TOPOLOGY_TYPE ret;
        switch (type)
        {
        case PrimitiveType::PointList: ret = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
        case PrimitiveType::LineList: ret = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;

        case PrimitiveType::TriangleList: 
        case PrimitiveType::TriangleStrip: 
        case PrimitiveType::TriangleListWithAdjacency:
        case PrimitiveType::TriangleStripWithAdjacency:
            ret = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
            
        case PrimitiveType::PatchList: ret = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH; break;
        }
        return ret;
    }


    DX12ViewportState convert_viewport_state(const RasterState& raster_state, const FrameBufferInfo& frame_buffer_info, const ViewportState& viewport_state)
    {
        DX12ViewportState ret;
        
        ret.viewports.resize(viewport_state.viewports.size());
        for (uint32_t ix = 0; ix < ret.viewports.size(); ++ix)
        {
            auto& src_viewport = viewport_state.viewports[ix];
            auto& ret_viewport = ret.viewports[ix];

            ret_viewport.TopLeftX = src_viewport.min_x;
            ret_viewport.TopLeftY = src_viewport.min_y;
            ret_viewport.Width = src_viewport.max_x - src_viewport.min_x;
            ret_viewport.Height = src_viewport.max_y - src_viewport.min_y;
            ret_viewport.MinDepth = src_viewport.min_z;
            ret_viewport.MaxDepth = src_viewport.max_z;
        }

        ret.scissor_rects.resize(viewport_state.scissor_rects.size());
        for (uint32_t ix = 0; ix < ret.scissor_rects.size(); ++ix)
        {
            auto& src_scissor_rect = viewport_state.scissor_rects[ix];
            auto& dst_scissor_rect = ret.scissor_rects[ix];

            if (raster_state.enable_scissor)
            {
                dst_scissor_rect.left = static_cast<long>(src_scissor_rect.min_x);
                dst_scissor_rect.top = static_cast<long>(src_scissor_rect.min_y);
                dst_scissor_rect.right = static_cast<long>(src_scissor_rect.max_x);
                dst_scissor_rect.bottom = static_cast<long>(src_scissor_rect.max_y);
            }
            else 
            {
                auto& view_port = viewport_state.viewports[ix];

                dst_scissor_rect.left = static_cast<long>(view_port.min_x);
                dst_scissor_rect.top = static_cast<long>(view_port.min_y);
                dst_scissor_rect.right = static_cast<long>(view_port.max_x);
                dst_scissor_rect.bottom = static_cast<long>(view_port.max_y);

                if (frame_buffer_info.width > 0)
                {
                    dst_scissor_rect.left = std::max(dst_scissor_rect.left, static_cast<long>(0));
                    dst_scissor_rect.top = std::max(dst_scissor_rect.top, static_cast<long>(0));
                    dst_scissor_rect.right = std::max(dst_scissor_rect.right, static_cast<long>(frame_buffer_info.width));
                    dst_scissor_rect.bottom = std::max(dst_scissor_rect.bottom, static_cast<long>(frame_buffer_info.height));
                }
            }
        }

        return ret;
    }


    D3D12_RESOURCE_DESC convert_texture_desc(const TextureDesc& desc)
    {
        const DxgiFormatMapping& format_mapping = get_dxgi_format_mapping(desc.format);

        D3D12_RESOURCE_DIMENSION d3d12_resource_dimension;
        UINT16 depth_or_array_size;
        
        switch(desc.dimension)
        {
        case TextureDimension::Texture1D:
        case TextureDimension::Texture1DArray:
            d3d12_resource_dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
            depth_or_array_size = static_cast<uint16_t>(desc.array_size);
            break;

        case TextureDimension::Texture2D:
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:
            d3d12_resource_dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            depth_or_array_size = static_cast<uint16_t>(desc.array_size);
            break;

        case TextureDimension::Texture3D:
            d3d12_resource_dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
            depth_or_array_size = static_cast<uint16_t>(desc.depth);
            break;
        default:
            assert(!"Invalid enum");
        }
        
        D3D12_RESOURCE_FLAGS d3d12_resource_flags;
        if (!desc.allow_shader_resource) d3d12_resource_flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        if (desc.allow_unordered_access) d3d12_resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if (desc.allow_render_target)    d3d12_resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (desc.allow_depth_stencil)    d3d12_resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;


        D3D12_RESOURCE_DESC ret{};
        ret.Dimension = d3d12_resource_dimension;
        ret.Alignment = 0;
        ret.Width = desc.width;
        ret.Height = desc.height;
        ret.DepthOrArraySize = depth_or_array_size;
        ret.MipLevels = static_cast<uint16_t>(desc.mip_levels);
        ret.Format = format_mapping.rtv_dsv_format;
        ret.SampleDesc = { 1, 0 };
        ret.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        ret.Flags = d3d12_resource_flags;

        return ret;
    }

    D3D12_RESOURCE_DESC convert_buffer_desc(const BufferDesc& desc)
    {
        D3D12_RESOURCE_DESC ret{};
        ret.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ret.Alignment = 0;
        ret.Width = desc.byte_size;
        ret.Height = 1;
        ret.DepthOrArraySize = 1;
        ret.MipLevels = 1;
        ret.Format = DXGI_FORMAT_UNKNOWN;
        ret.SampleDesc = { 1, 0 };
        ret.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ret.Flags = D3D12_RESOURCE_FLAG_NONE;

        if (!desc.allow_shader_resource) ret.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        if (desc.allow_unordered_access) ret.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        
        return ret;
    }

    D3D12_SAMPLER_DESC convert_sampler_desc(const SamplerDesc& desc)
    {
        D3D12_FILTER d3d12_filter;
        uint32_t reduction_type = convert_sampler_reduction_type(desc.reduction_type);
        if (desc.max_anisotropy > 1.0f)
        {
            d3d12_filter = D3D12_ENCODE_ANISOTROPIC_FILTER(reduction_type);
        }
        else 
        {
            d3d12_filter = D3D12_ENCODE_BASIC_FILTER(
                desc.min_filter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
                desc.max_filter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
                desc.mip_filter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
                reduction_type
            );
        }

        D3D12_SAMPLER_DESC ret;
        ret.Filter = d3d12_filter;
        ret.AddressU = convert_sampler_address_mode(desc.address_u);
        ret.AddressV = convert_sampler_address_mode(desc.address_v);
        ret.AddressW = convert_sampler_address_mode(desc.address_w);

        ret.MipLODBias = desc.mip_bias;
        ret.MaxAnisotropy = std::max(static_cast<uint32_t>(desc.max_anisotropy), 1u);
        ret.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
        ret.BorderColor[0] = desc.border_color.r;
        ret.BorderColor[1] = desc.border_color.g;
        ret.BorderColor[2] = desc.border_color.b;
        ret.BorderColor[3] = desc.border_color.a;
        ret.MinLOD = 0;
        ret.MaxLOD = D3D12_FLOAT32_MAX;
        
        return ret;
    }

    D3D12_RESOURCE_STATES get_buffer_initial_state(const BufferDesc& desc)
    {
        D3D12_RESOURCE_STATES ret = D3D12_RESOURCE_STATE_COMMON;
        if (desc.struct_stride != 0) ret |= D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
        if (desc.is_vertex_buffer) ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        if (desc.is_index_buffer) ret |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
        if (desc.is_indirect_buffer) ret |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        if (desc.is_constant_buffer) ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        // if (desc.is_accel_struct_storage) usage_flags |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        // if (desc.is_shader_binding_table) usage_flags |= ;

        if (desc.cpu_access == CpuAccessMode::Read) ret |= D3D12_RESOURCE_STATE_COPY_DEST;
        if (desc.cpu_access == CpuAccessMode::Write) ret |= D3D12_RESOURCE_STATE_GENERIC_READ;
        return ret;
    }


    D3D12_CLEAR_VALUE convert_clear_value(const TextureDesc& desc)
    {
        const DxgiFormatMapping& format_mapping = get_dxgi_format_mapping(desc.format);
        const FormatInfo& format_info = get_format_info(desc.format);

        D3D12_CLEAR_VALUE ret;
        ret.Format = format_mapping.rtv_dsv_format;
        if (format_info.has_depth || format_info.has_stencil)
        {
            ret.DepthStencil.Depth = desc.clear_value.r;
            ret.DepthStencil.Stencil = static_cast<uint8_t>(desc.clear_value.g);
        }
        else 
        {
            ret.Color[0] = desc.clear_value.r;
            ret.Color[1] = desc.clear_value.g;
            ret.Color[2] = desc.clear_value.b;
            ret.Color[3] = desc.clear_value.a;
        }

        return ret;
    }

    namespace ray_tracing 
    {

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS convert_accel_struct_build_flags(AccelStructBuildFlags flags)
        {
            switch (flags)
            {
            case AccelStructBuildFlags::None: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
            case AccelStructBuildFlags::AllowUpdate: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
            case AccelStructBuildFlags::AllowCompaction: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
            case AccelStructBuildFlags::PreferFastTrace: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
            case AccelStructBuildFlags::PreferFastBuild: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
            case AccelStructBuildFlags::MinimizeMemory: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY;
            case AccelStructBuildFlags::PerformUpdate: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
            }
            return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
        }

        D3D12_RAYTRACING_GEOMETRY_FLAGS convert_geometry_flags(GeometryFlags flags)
        {
            switch (flags)
            {
                case GeometryFlags::None: return D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
                case GeometryFlags::Opaque: return D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
                case GeometryFlags::NoDuplicateAnyHitInvocation: return D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;
            }
            return D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
        }

        D3D12_RAYTRACING_INSTANCE_FLAGS convert_instance_flags(InstanceFlags flags)
        {
            switch (flags) 
            {
                case InstanceFlags::None: return D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
                case InstanceFlags::TriangleCullDisable: return D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
                case InstanceFlags::TriangleFrontCounterclockwise: return D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
                case InstanceFlags::ForceOpaque: return D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
                case InstanceFlags::ForceNonOpaque: return D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
            }
            return D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        }


        D3D12_RAYTRACING_INSTANCE_DESC convert_instance_desc(const InstanceDesc& instance_desc)
        {
            D3D12_RAYTRACING_INSTANCE_DESC ret;
            memcpy(ret.Transform, instance_desc.affine_matrix._data, sizeof(float) * 3 * 4);
            ret.InstanceID = instance_desc.instance_id;
            ret.InstanceMask = instance_desc.instance_mask;
            ret.InstanceContributionToHitGroupIndex = instance_desc.instance_contibution_to_hit_group_index;
            ret.AccelerationStructure = instance_desc.blas_device_address;
            ret.Flags = convert_instance_flags(instance_desc.flags);
            return ret;
        }
    }

    static const DxgiFormatMapping dxgi_format_mappings[] = {
        { Format::UNKNOWN,              DXGI_FORMAT_UNKNOWN,                DXGI_FORMAT_UNKNOWN,                  DXGI_FORMAT_UNKNOWN                },

        { Format::R8_UINT,              DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_UINT,                  DXGI_FORMAT_R8_UINT                },
        { Format::R8_SINT,              DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_SINT,                  DXGI_FORMAT_R8_SINT                },
        { Format::R8_UNORM,             DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_UNORM,                 DXGI_FORMAT_R8_UNORM               },
        { Format::R8_SNORM,             DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_SNORM,                 DXGI_FORMAT_R8_SNORM               },
        { Format::RG8_UINT,             DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_UINT,                DXGI_FORMAT_R8G8_UINT              },
        { Format::RG8_SINT,             DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_SINT,                DXGI_FORMAT_R8G8_SINT              },
        { Format::RG8_UNORM,            DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_UNORM,               DXGI_FORMAT_R8G8_UNORM             },
        { Format::RG8_SNORM,            DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_SNORM,               DXGI_FORMAT_R8G8_SNORM             },
        { Format::R16_UINT,             DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_UINT,                 DXGI_FORMAT_R16_UINT               },
        { Format::R16_SINT,             DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_SINT,                 DXGI_FORMAT_R16_SINT               },
        { Format::R16_UNORM,            DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_UNORM,                DXGI_FORMAT_R16_UNORM              },
        { Format::R16_SNORM,            DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_SNORM,                DXGI_FORMAT_R16_SNORM              },
        { Format::R16_FLOAT,            DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_FLOAT,                DXGI_FORMAT_R16_FLOAT              },
        { Format::BGRA4_UNORM,          DXGI_FORMAT_B4G4R4A4_UNORM,         DXGI_FORMAT_B4G4R4A4_UNORM,           DXGI_FORMAT_B4G4R4A4_UNORM         },
        { Format::B5G6R5_UNORM,         DXGI_FORMAT_B5G6R5_UNORM,           DXGI_FORMAT_B5G6R5_UNORM,             DXGI_FORMAT_B5G6R5_UNORM           },
        { Format::B5G5R5A1_UNORM,       DXGI_FORMAT_B5G5R5A1_UNORM,         DXGI_FORMAT_B5G5R5A1_UNORM,           DXGI_FORMAT_B5G5R5A1_UNORM         },
        { Format::RGBA8_UINT,           DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_UINT,            DXGI_FORMAT_R8G8B8A8_UINT          },
        { Format::RGBA8_SINT,           DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_SINT,            DXGI_FORMAT_R8G8B8A8_SINT          },
        { Format::RGBA8_UNORM,          DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_UNORM,           DXGI_FORMAT_R8G8B8A8_UNORM         },
        { Format::RGBA8_SNORM,          DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_SNORM,           DXGI_FORMAT_R8G8B8A8_SNORM         },
        { Format::BGRA8_UNORM,          DXGI_FORMAT_B8G8R8A8_TYPELESS,      DXGI_FORMAT_B8G8R8A8_UNORM,           DXGI_FORMAT_B8G8R8A8_UNORM         },
        { Format::SRGBA8_UNORM,         DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB    },
        { Format::SBGRA8_UNORM,         DXGI_FORMAT_B8G8R8A8_TYPELESS,      DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,      DXGI_FORMAT_B8G8R8A8_UNORM_SRGB    },
        { Format::R10G10B10A2_UNORM,    DXGI_FORMAT_R10G10B10A2_TYPELESS,   DXGI_FORMAT_R10G10B10A2_UNORM,        DXGI_FORMAT_R10G10B10A2_UNORM      },
        { Format::R11G11B10_FLOAT,      DXGI_FORMAT_R11G11B10_FLOAT,        DXGI_FORMAT_R11G11B10_FLOAT,          DXGI_FORMAT_R11G11B10_FLOAT        },
        { Format::RG16_UINT,            DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_UINT,              DXGI_FORMAT_R16G16_UINT            },
        { Format::RG16_SINT,            DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_SINT,              DXGI_FORMAT_R16G16_SINT            },
        { Format::RG16_UNORM,           DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_UNORM,             DXGI_FORMAT_R16G16_UNORM           },
        { Format::RG16_SNORM,           DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_SNORM,             DXGI_FORMAT_R16G16_SNORM           },
        { Format::RG16_FLOAT,           DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_FLOAT,             DXGI_FORMAT_R16G16_FLOAT           },
        { Format::R32_UINT,             DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_UINT,                 DXGI_FORMAT_R32_UINT               },
        { Format::R32_SINT,             DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_SINT,                 DXGI_FORMAT_R32_SINT               },
        { Format::R32_FLOAT,            DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_FLOAT,                DXGI_FORMAT_R32_FLOAT              },
        { Format::RGBA16_UINT,          DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_UINT,        DXGI_FORMAT_R16G16B16A16_UINT      },
        { Format::RGBA16_SINT,          DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_SINT,        DXGI_FORMAT_R16G16B16A16_SINT      },
        { Format::RGBA16_FLOAT,         DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_FLOAT,       DXGI_FORMAT_R16G16B16A16_FLOAT     },
        { Format::RGBA16_UNORM,         DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_UNORM,       DXGI_FORMAT_R16G16B16A16_UNORM     },
        { Format::RGBA16_SNORM,         DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_SNORM,       DXGI_FORMAT_R16G16B16A16_SNORM     },
        { Format::RG32_UINT,            DXGI_FORMAT_R32G32_TYPELESS,        DXGI_FORMAT_R32G32_UINT,              DXGI_FORMAT_R32G32_UINT            },
        { Format::RG32_SINT,            DXGI_FORMAT_R32G32_TYPELESS,        DXGI_FORMAT_R32G32_SINT,              DXGI_FORMAT_R32G32_SINT            },
        { Format::RG32_FLOAT,           DXGI_FORMAT_R32G32_TYPELESS,        DXGI_FORMAT_R32G32_FLOAT,             DXGI_FORMAT_R32G32_FLOAT           },
        { Format::RGB32_UINT,           DXGI_FORMAT_R32G32B32_TYPELESS,     DXGI_FORMAT_R32G32B32_UINT,           DXGI_FORMAT_R32G32B32_UINT         },
        { Format::RGB32_SINT,           DXGI_FORMAT_R32G32B32_TYPELESS,     DXGI_FORMAT_R32G32B32_SINT,           DXGI_FORMAT_R32G32B32_SINT         },
        { Format::RGB32_FLOAT,          DXGI_FORMAT_R32G32B32_TYPELESS,     DXGI_FORMAT_R32G32B32_FLOAT,          DXGI_FORMAT_R32G32B32_FLOAT        },
        { Format::RGBA32_UINT,          DXGI_FORMAT_R32G32B32A32_TYPELESS,  DXGI_FORMAT_R32G32B32A32_UINT,        DXGI_FORMAT_R32G32B32A32_UINT      },
        { Format::RGBA32_SINT,          DXGI_FORMAT_R32G32B32A32_TYPELESS,  DXGI_FORMAT_R32G32B32A32_SINT,        DXGI_FORMAT_R32G32B32A32_SINT      },
        { Format::RGBA32_FLOAT,         DXGI_FORMAT_R32G32B32A32_TYPELESS,  DXGI_FORMAT_R32G32B32A32_FLOAT,       DXGI_FORMAT_R32G32B32A32_FLOAT     },

        { Format::D16,                  DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_UNORM,                DXGI_FORMAT_D16_UNORM              },
        { Format::D24S8,                DXGI_FORMAT_R24G8_TYPELESS,         DXGI_FORMAT_R24_UNORM_X8_TYPELESS,    DXGI_FORMAT_D24_UNORM_S8_UINT      },
        { Format::X24G8_UINT,           DXGI_FORMAT_R24G8_TYPELESS,         DXGI_FORMAT_X24_TYPELESS_G8_UINT,     DXGI_FORMAT_D24_UNORM_S8_UINT      },
        { Format::D32,                  DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_FLOAT,                DXGI_FORMAT_D32_FLOAT              },
        { Format::D32S8,                DXGI_FORMAT_R32G8X24_TYPELESS,      DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, DXGI_FORMAT_D32_FLOAT_S8X24_UINT   },
        { Format::X32G8_UINT,           DXGI_FORMAT_R32G8X24_TYPELESS,      DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,  DXGI_FORMAT_D32_FLOAT_S8X24_UINT   },
    };

    const DxgiFormatMapping& get_dxgi_format_mapping(Format format)
    {
        static_assert(sizeof(dxgi_format_mappings) / sizeof(DxgiFormatMapping) == size_t(Format::NUM));

        const auto& mapping = dxgi_format_mappings[static_cast<uint32_t>(format)];
        assert(mapping.format == format); 
        return mapping;
    }

}
