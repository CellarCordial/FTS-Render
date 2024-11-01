#include "DX12Converts.h"
#include <algorithm>
#include <d3d12.h>
#include <minwindef.h>



namespace FTS
{
    D3D12_SHADER_VISIBILITY ConvertShaderStage(EShaderType ShaderVisibility)
    {
        switch (ShaderVisibility)  // NOLINT(clang-diagnostic-switch-enum)
        {
        case EShaderType::Vertex       : return D3D12_SHADER_VISIBILITY_VERTEX;
        case EShaderType::Hull         : return D3D12_SHADER_VISIBILITY_HULL;
        case EShaderType::Domain       : return D3D12_SHADER_VISIBILITY_DOMAIN;
        case EShaderType::Geometry     : return D3D12_SHADER_VISIBILITY_GEOMETRY;
        case EShaderType::Pixel        : return D3D12_SHADER_VISIBILITY_PIXEL;

        default:
            // catch-all case - actually some of the bitfield combinations are unrepresentable in DX12
            return D3D12_SHADER_VISIBILITY_ALL;
        }
    }

    D3D12_BLEND ConvertBlendValue(EBlendFactor Factor)
    {
        switch (Factor)
        {
        case EBlendFactor::Zero            : return D3D12_BLEND_ZERO;
        case EBlendFactor::One             : return D3D12_BLEND_ONE;
        case EBlendFactor::SrcColor        : return D3D12_BLEND_SRC_COLOR;
        case EBlendFactor::InvSrcColor     : return D3D12_BLEND_INV_SRC_COLOR;
        case EBlendFactor::SrcAlpha        : return D3D12_BLEND_SRC_ALPHA;
        case EBlendFactor::InvSrcAlpha     : return D3D12_BLEND_INV_SRC_ALPHA;
        case EBlendFactor::DstAlpha        : return D3D12_BLEND_DEST_ALPHA;
        case EBlendFactor::InvDstAlpha     : return D3D12_BLEND_INV_DEST_ALPHA;
        case EBlendFactor::DstColor        : return D3D12_BLEND_DEST_COLOR;
        case EBlendFactor::InvDstColor     : return D3D12_BLEND_INV_DEST_COLOR;
        case EBlendFactor::SrcAlphaSaturate: return D3D12_BLEND_SRC_ALPHA_SAT;
        case EBlendFactor::ConstantColor   : return D3D12_BLEND_BLEND_FACTOR;
        case EBlendFactor::InvConstantColor: return D3D12_BLEND_INV_BLEND_FACTOR;
        case EBlendFactor::Src1Color       : return D3D12_BLEND_SRC1_COLOR;
        case EBlendFactor::InvSrc1Color    : return D3D12_BLEND_INV_SRC1_COLOR;
        case EBlendFactor::Src1Alpha       : return D3D12_BLEND_SRC1_ALPHA;
        case EBlendFactor::InvSrc1Alpha    : return D3D12_BLEND_INV_SRC1_ALPHA;
        }
        return D3D12_BLEND_ZERO;
    }

    D3D12_BLEND_OP ConvertBlendOp(EBlendOP BlendOP)
    {
        switch (BlendOP)
        {
        case EBlendOP::Add            : return D3D12_BLEND_OP_ADD;
        case EBlendOP::Subtract       : return D3D12_BLEND_OP_SUBTRACT;
        case EBlendOP::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
        case EBlendOP::Min            : return D3D12_BLEND_OP_MIN;
        case EBlendOP::Max            : return D3D12_BLEND_OP_MAX;
        }
        return D3D12_BLEND_OP_ADD;
    }

    D3D12_STENCIL_OP ConvertStencilOp(EStencilOP StencilOP)
    {
        switch (StencilOP)
        {
        case EStencilOP::Keep             : return D3D12_STENCIL_OP_KEEP;
        case EStencilOP::Zero             : return D3D12_STENCIL_OP_ZERO;
        case EStencilOP::Replace          : return D3D12_STENCIL_OP_REPLACE;
        case EStencilOP::IncrementAndClamp: return D3D12_STENCIL_OP_INCR_SAT;
        case EStencilOP::DecrementAndClamp: return D3D12_STENCIL_OP_DECR_SAT;
        case EStencilOP::Invert           : return D3D12_STENCIL_OP_INVERT;
        case EStencilOP::IncrementAndWrap : return D3D12_STENCIL_OP_INCR;
        case EStencilOP::DecrementAndWrap : return D3D12_STENCIL_OP_DECR;
        }
        return D3D12_STENCIL_OP_KEEP;
    }

    D3D12_COMPARISON_FUNC ConvertComparisonFunc(EComparisonFunc Func)
    {
        switch (Func)
        {
        case EComparisonFunc::Never         : return D3D12_COMPARISON_FUNC_NEVER;
        case EComparisonFunc::Less          : return D3D12_COMPARISON_FUNC_LESS;
        case EComparisonFunc::Equal         : return D3D12_COMPARISON_FUNC_EQUAL;
        case EComparisonFunc::LessOrEqual   : return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case EComparisonFunc::Greater       : return D3D12_COMPARISON_FUNC_GREATER;
        case EComparisonFunc::NotEqual      : return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case EComparisonFunc::GreaterOrEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case EComparisonFunc::Always        : return D3D12_COMPARISON_FUNC_ALWAYS;
        }
        return D3D12_COMPARISON_FUNC_NEVER;
    }

    D3D_PRIMITIVE_TOPOLOGY ConvertPrimitiveType(EPrimitiveType Type, UINT32 dwControlPoints)
    {
        switch (Type)
        {
        case EPrimitiveType::PointList                 : return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case EPrimitiveType::LineList                  : return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case EPrimitiveType::TriangleList              : return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case EPrimitiveType::TriangleStrip             : return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case EPrimitiveType::TriangleListWithAdjacency : return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
        case EPrimitiveType::TriangleStripWithAdjacency: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
        case EPrimitiveType::PatchList:
            if (dwControlPoints == 0 || dwControlPoints > 32)
            {
                assert(false && "Invalid Enumeration Value");
                return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
            }
            return static_cast<D3D_PRIMITIVE_TOPOLOGY>(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (dwControlPoints - 1));
        }
        return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }

    D3D12_TEXTURE_ADDRESS_MODE ConvertSamplerAddressMode(ESamplerAddressMode Mode)
    {
        switch (Mode)
        {
        case ESamplerAddressMode::Clamp     : return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case ESamplerAddressMode::Wrap      : return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case ESamplerAddressMode::Border    : return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case ESamplerAddressMode::Mirror    : return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case ESamplerAddressMode::MirrorOnce: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        }
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    }

    UINT ConvertSamplerReductionType(ESamplerReductionType ReductionType)
    {
        switch (ReductionType)
        {
        case ESamplerReductionType::Standard  : return D3D12_FILTER_REDUCTION_TYPE_STANDARD;
        case ESamplerReductionType::Comparison: return D3D12_FILTER_REDUCTION_TYPE_COMPARISON;
        case ESamplerReductionType::Minimum   : return D3D12_FILTER_REDUCTION_TYPE_MINIMUM;
        case ESamplerReductionType::Maximum   : return D3D12_FILTER_REDUCTION_TYPE_MAXIMUM;
        }
        return D3D12_FILTER_REDUCTION_TYPE_STANDARD;
    }

    D3D12_RESOURCE_STATES ConvertResourceStates(EResourceStates States)
    {
        if (States == EResourceStates::Common) return D3D12_RESOURCE_STATE_COMMON;

        D3D12_RESOURCE_STATES Ret = D3D12_RESOURCE_STATE_COMMON;

        if ((States & EResourceStates::ConstantBuffer) != 0) Ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        if ((States & EResourceStates::VertexBuffer) != 0) Ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        if ((States & EResourceStates::IndexBuffer) != 0) Ret |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
        if ((States & EResourceStates::PixelShaderResource) != 0) Ret |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        if ((States & EResourceStates::NonPixelShaderResource) != 0) Ret |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        if ((States & EResourceStates::UnorderedAccess) != 0) Ret |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        if ((States & EResourceStates::RenderTarget) != 0) Ret |= D3D12_RESOURCE_STATE_RENDER_TARGET;
        if ((States & EResourceStates::DepthWrite) != 0) Ret |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
        if ((States & EResourceStates::DepthRead) != 0) Ret |= D3D12_RESOURCE_STATE_DEPTH_READ;
        if ((States & EResourceStates::StreamOut) != 0) Ret |= D3D12_RESOURCE_STATE_STREAM_OUT;
        if ((States & EResourceStates::CopyDest) != 0) Ret |= D3D12_RESOURCE_STATE_COPY_DEST;
        if ((States & EResourceStates::CopySource) != 0) Ret |= D3D12_RESOURCE_STATE_COPY_SOURCE;
        if ((States & EResourceStates::ResolveDest) != 0) Ret |= D3D12_RESOURCE_STATE_RESOLVE_DEST;
        if ((States & EResourceStates::ResolveSource) != 0) Ret |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
        if ((States & EResourceStates::Present) != 0) Ret |= D3D12_RESOURCE_STATE_PRESENT;

        return Ret;
    }

    D3D12_BLEND_DESC ConvertBlendState(const FBlendState& crState)
    {
        D3D12_BLEND_DESC Ret{};
        Ret.AlphaToCoverageEnable = crState.bAlphaToCoverageEnable;
        Ret.IndependentBlendEnable = false;

        for (UINT32 ix = 0; ix < gdwMaxRenderTargets; ix++)
        {
            const FBlendState::RenderTarget& Src = crState.TargetBlends[ix];
            D3D12_RENDER_TARGET_BLEND_DESC& Dst = Ret.RenderTarget[ix];

            Dst.LogicOpEnable = false;
            Dst.LogicOp = D3D12_LOGIC_OP_NOOP;
            Dst.BlendEnable = Src.bEnableBlend;
            Dst.SrcBlend = ConvertBlendValue(Src.SrcBlend);
            Dst.DestBlend = ConvertBlendValue(Src.DstBlend);
            Dst.BlendOp = ConvertBlendOp(Src.BlendOp);
            Dst.SrcBlendAlpha = ConvertBlendValue(Src.SrcBlendAlpha);
            Dst.DestBlendAlpha = ConvertBlendValue(Src.DstBlendAlpha);
            Dst.BlendOpAlpha = ConvertBlendOp(Src.BlendOpAlpha);
            Dst.RenderTargetWriteMask = static_cast<UINT8>(Src.ColorWriteMask);
        }
        return Ret;
    }

    D3D12_DEPTH_STENCIL_DESC ConvertDepthStencilState(const FDepthStencilState& crState)
    {
        D3D12_DEPTH_STENCIL_DESC Ret;
        Ret.DepthEnable                  = crState.bDepthTestEnable ? TRUE : FALSE;
        Ret.DepthWriteMask               = crState.bDepthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        Ret.DepthFunc                    = ConvertComparisonFunc(crState.DepthFunc);
        Ret.StencilEnable                = crState.bStencilEnable ? TRUE : FALSE;
        Ret.StencilReadMask              = static_cast<UINT8>(crState.btStencilReadMask);
        Ret.StencilWriteMask             = static_cast<UINT8>(crState.btStencilWriteMask);
        Ret.FrontFace.StencilFailOp      = ConvertStencilOp(crState.FrontFaceStencil.FailOp);
        Ret.FrontFace.StencilDepthFailOp = ConvertStencilOp(crState.FrontFaceStencil.DepthFailOp);
        Ret.FrontFace.StencilPassOp      = ConvertStencilOp(crState.FrontFaceStencil.PassOp);
        Ret.FrontFace.StencilFunc        = ConvertComparisonFunc(crState.FrontFaceStencil.StencilFunc);
        Ret.BackFace.StencilFailOp       = ConvertStencilOp(crState.BackFaceStencil.FailOp);
        Ret.BackFace.StencilDepthFailOp  = ConvertStencilOp(crState.BackFaceStencil.DepthFailOp);
        Ret.BackFace.StencilPassOp       = ConvertStencilOp(crState.BackFaceStencil.PassOp);
        Ret.BackFace.StencilFunc         = ConvertComparisonFunc(crState.BackFaceStencil.StencilFunc);

        return Ret;
    }

    D3D12_RASTERIZER_DESC ConvertRasterizerState(const FRasterState& crState)
    {
        D3D12_RASTERIZER_DESC Ret;
        switch (crState.FillMode)
        {
        case ERasterFillMode::Solid: Ret.FillMode     = D3D12_FILL_MODE_SOLID; break;
        case ERasterFillMode::Wireframe: Ret.FillMode = D3D12_FILL_MODE_WIREFRAME; break;
        }

        switch (crState.CullMode)
        {
        case ERasterCullMode::Back: Ret.CullMode  = D3D12_CULL_MODE_BACK; break;
        case ERasterCullMode::Front: Ret.CullMode = D3D12_CULL_MODE_FRONT; break;
        case ERasterCullMode::None: Ret.CullMode  = D3D12_CULL_MODE_NONE; break;
        }

        Ret.FrontCounterClockwise = crState.bFrontCounterClockWise;
        Ret.DepthBias             = crState.dwDepthBias;
        Ret.DepthBiasClamp        = crState.fDepthBiasClamp;
        Ret.SlopeScaledDepthBias  = crState.fSlopeScaledDepthBias;
        Ret.DepthClipEnable       = crState.bDepthClipEnable;
        Ret.MultisampleEnable     = crState.bMultisampleEnable;
        Ret.AntialiasedLineEnable = crState.bAntiAliasedLineEnable;
        Ret.ConservativeRaster    = crState.bConservativeRasterEnable ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        Ret.ForcedSampleCount     = crState.btForcedSampleCount;

        return Ret;
    }

    FDX12ViewportState ConvertViewportState(const FRasterState& crRasterState, const FFrameBufferInfo& crFramebufferInfo, const FViewportState& crViewportState)
    {
        FDX12ViewportState Ret;
        
        Ret.Viewports.Resize(crViewportState.Viewports.Size());
        for (UINT32 ix = 0; ix < Ret.Viewports.Size(); ++ix)
        {
            auto& rViewport = crViewportState.Viewports[ix];
            auto& rRetViewport = Ret.Viewports[ix];

            rRetViewport.TopLeftX = rViewport.fMinY;
            rRetViewport.TopLeftX = rViewport.fMinX;
            rRetViewport.TopLeftY = rViewport.fMinY;
            rRetViewport.Width = rViewport.fMaxX - rViewport.fMinX;
            rRetViewport.Height = rViewport.fMaxY - rViewport.fMinY;
            rRetViewport.MinDepth = rViewport.fMinZ;
            rRetViewport.MaxDepth = rViewport.fMaxZ;
        }

        Ret.ScissorRects.Resize(crViewportState.Rects.Size());
        for (UINT32 ix = 0; ix < Ret.ScissorRects.Size(); ++ix)
        {
            auto& rScissorRect = crViewportState.Rects[ix];
            auto& rRetScissorRect = Ret.ScissorRects[ix];

            if (crRasterState.bScissorEnable)
            {
                rRetScissorRect.left = static_cast<LONG>(rScissorRect.dwMinX);
                rRetScissorRect.top = static_cast<LONG>(rScissorRect.dwMinY);
                rRetScissorRect.right = static_cast<LONG>(rScissorRect.dwMaxX);
                rRetScissorRect.bottom = static_cast<LONG>(rScissorRect.dwMaxY);
            }
            else 
            {
                auto& rViewport = crViewportState.Viewports[ix];

                rRetScissorRect.left = static_cast<LONG>(rViewport.fMinX);
                rRetScissorRect.top = static_cast<LONG>(rViewport.fMinY);
                rRetScissorRect.right = static_cast<LONG>(rViewport.fMaxX);
                rRetScissorRect.bottom = static_cast<LONG>(rViewport.fMaxY);

                if (crFramebufferInfo.dwWidth > 0)
                {
                    rRetScissorRect.left = std::max(rRetScissorRect.left, static_cast<LONG>(0));
                    rRetScissorRect.top = std::max(rRetScissorRect.top, static_cast<LONG>(0));
                    rRetScissorRect.right = std::max(rRetScissorRect.right, static_cast<LONG>(crFramebufferInfo.dwWidth));
                    rRetScissorRect.bottom = std::max(rRetScissorRect.bottom, static_cast<LONG>(crFramebufferInfo.dwHeight));
                }
            }
        }

        return Ret;
    }


    D3D12_RESOURCE_DESC ConvertTextureDesc(const FTextureDesc& crDesc)
    {
        const FDxgiFormatMapping& crFormatMapping = GetDxgiFormatMapping(crDesc.Format);

        D3D12_RESOURCE_DESC Ret{};
        Ret.SampleDesc = { crDesc.dwSampleCount, crDesc.dwSampleQuality };
        Ret.Alignment = 0;
        Ret.Height = crDesc.dwHeight;
        Ret.Width = crDesc.dwWidth;
        Ret.MipLevels = static_cast<UINT16>(crDesc.dwMipLevels);
        Ret.Format = crDesc.bIsTypeless ? crFormatMapping.ResourceFormat : crFormatMapping.RTVFormat;

        switch(crDesc.Dimension)
        {
        case ETextureDimension::Texture1D:
        case ETextureDimension::Texture1DArray:
            Ret.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
            Ret.DepthOrArraySize = static_cast<UINT16>(crDesc.dwArraySize);
            break;

        case ETextureDimension::Texture2D:
        case ETextureDimension::Texture2DArray:
        case ETextureDimension::TextureCube:
        case ETextureDimension::TextureCubeArray:
        case ETextureDimension::Texture2DMS:
        case ETextureDimension::Texture2DMSArray:
            Ret.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            Ret.DepthOrArraySize = static_cast<UINT16>(crDesc.dwArraySize);
            break;

        case ETextureDimension::Texture3D:
            Ret.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
            Ret.DepthOrArraySize = static_cast<UINT16>(crDesc.dwDepth);
            break;
        case ETextureDimension::Unknown:
            assert(!"Invalid Enumeration Value");
        }

        const FFormatInfo& crFormatInfo = GetFormatInfo(crDesc.Format);

        if (!crDesc.bIsShaderResource)  Ret.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        if (crDesc.bIsUAV)              Ret.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if (crDesc.bIsRenderTarget)     Ret.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (crDesc.bIsDepthStencil && ( crFormatInfo.bHasDepth || crFormatInfo.bHasStencil)) Ret.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        return Ret;
    }

    D3D12_CLEAR_VALUE ConvertClearValue(const FTextureDesc& crDesc)
    {
        const FDxgiFormatMapping& crFormatMapping = GetDxgiFormatMapping(crDesc.Format);
        const FFormatInfo& crFormatInfo = GetFormatInfo(crDesc.Format);

        D3D12_CLEAR_VALUE Ret;
        Ret.Format = crFormatMapping.RTVFormat;
        if (crFormatInfo.bHasDepth || crFormatInfo.bHasStencil)
        {
            Ret.DepthStencil.Depth = crDesc.ClearValue.r;
            Ret.DepthStencil.Stencil = static_cast<UINT8>(crDesc.ClearValue.g);
        }
        else 
        {
            Ret.Color[0] = crDesc.ClearValue.r;
            Ret.Color[1] = crDesc.ClearValue.g;
            Ret.Color[2] = crDesc.ClearValue.b;
            Ret.Color[3] = crDesc.ClearValue.a;
        }

        return Ret;
    }


    static const FDxgiFormatMapping gcDxgiFormatMappings[] = {
        { EFormat::UNKNOWN,              DXGI_FORMAT_UNKNOWN,                DXGI_FORMAT_UNKNOWN,                  DXGI_FORMAT_UNKNOWN                },

        { EFormat::R8_UINT,              DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_UINT,                  DXGI_FORMAT_R8_UINT                },
        { EFormat::R8_SINT,              DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_SINT,                  DXGI_FORMAT_R8_SINT                },
        { EFormat::R8_UNORM,             DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_UNORM,                 DXGI_FORMAT_R8_UNORM               },
        { EFormat::R8_SNORM,             DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_SNORM,                 DXGI_FORMAT_R8_SNORM               },
        { EFormat::RG8_UINT,             DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_UINT,                DXGI_FORMAT_R8G8_UINT              },
        { EFormat::RG8_SINT,             DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_SINT,                DXGI_FORMAT_R8G8_SINT              },
        { EFormat::RG8_UNORM,            DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_UNORM,               DXGI_FORMAT_R8G8_UNORM             },
        { EFormat::RG8_SNORM,            DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_SNORM,               DXGI_FORMAT_R8G8_SNORM             },
        { EFormat::R16_UINT,             DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_UINT,                 DXGI_FORMAT_R16_UINT               },
        { EFormat::R16_SINT,             DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_SINT,                 DXGI_FORMAT_R16_SINT               },
        { EFormat::R16_UNORM,            DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_UNORM,                DXGI_FORMAT_R16_UNORM              },
        { EFormat::R16_SNORM,            DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_SNORM,                DXGI_FORMAT_R16_SNORM              },
        { EFormat::R16_FLOAT,            DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_FLOAT,                DXGI_FORMAT_R16_FLOAT              },
        { EFormat::BGRA4_UNORM,          DXGI_FORMAT_B4G4R4A4_UNORM,         DXGI_FORMAT_B4G4R4A4_UNORM,           DXGI_FORMAT_B4G4R4A4_UNORM         },
        { EFormat::B5G6R5_UNORM,         DXGI_FORMAT_B5G6R5_UNORM,           DXGI_FORMAT_B5G6R5_UNORM,             DXGI_FORMAT_B5G6R5_UNORM           },
        { EFormat::B5G5R5A1_UNORM,       DXGI_FORMAT_B5G5R5A1_UNORM,         DXGI_FORMAT_B5G5R5A1_UNORM,           DXGI_FORMAT_B5G5R5A1_UNORM         },
        { EFormat::RGBA8_UINT,           DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_UINT,            DXGI_FORMAT_R8G8B8A8_UINT          },
        { EFormat::RGBA8_SINT,           DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_SINT,            DXGI_FORMAT_R8G8B8A8_SINT          },
        { EFormat::RGBA8_UNORM,          DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_UNORM,           DXGI_FORMAT_R8G8B8A8_UNORM         },
        { EFormat::RGBA8_SNORM,          DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_SNORM,           DXGI_FORMAT_R8G8B8A8_SNORM         },
        { EFormat::BGRA8_UNORM,          DXGI_FORMAT_B8G8R8A8_TYPELESS,      DXGI_FORMAT_B8G8R8A8_UNORM,           DXGI_FORMAT_B8G8R8A8_UNORM         },
        { EFormat::SRGBA8_UNORM,         DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB    },
        { EFormat::SBGRA8_UNORM,         DXGI_FORMAT_B8G8R8A8_TYPELESS,      DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,      DXGI_FORMAT_B8G8R8A8_UNORM_SRGB    },
        { EFormat::R10G10B10A2_UNORM,    DXGI_FORMAT_R10G10B10A2_TYPELESS,   DXGI_FORMAT_R10G10B10A2_UNORM,        DXGI_FORMAT_R10G10B10A2_UNORM      },
        { EFormat::R11G11B10_FLOAT,      DXGI_FORMAT_R11G11B10_FLOAT,        DXGI_FORMAT_R11G11B10_FLOAT,          DXGI_FORMAT_R11G11B10_FLOAT        },
        { EFormat::RG16_UINT,            DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_UINT,              DXGI_FORMAT_R16G16_UINT            },
        { EFormat::RG16_SINT,            DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_SINT,              DXGI_FORMAT_R16G16_SINT            },
        { EFormat::RG16_UNORM,           DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_UNORM,             DXGI_FORMAT_R16G16_UNORM           },
        { EFormat::RG16_SNORM,           DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_SNORM,             DXGI_FORMAT_R16G16_SNORM           },
        { EFormat::RG16_FLOAT,           DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_FLOAT,             DXGI_FORMAT_R16G16_FLOAT           },
        { EFormat::R32_UINT,             DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_UINT,                 DXGI_FORMAT_R32_UINT               },
        { EFormat::R32_SINT,             DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_SINT,                 DXGI_FORMAT_R32_SINT               },
        { EFormat::R32_FLOAT,            DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_FLOAT,                DXGI_FORMAT_R32_FLOAT              },
        { EFormat::RGBA16_UINT,          DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_UINT,        DXGI_FORMAT_R16G16B16A16_UINT      },
        { EFormat::RGBA16_SINT,          DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_SINT,        DXGI_FORMAT_R16G16B16A16_SINT      },
        { EFormat::RGBA16_FLOAT,         DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_FLOAT,       DXGI_FORMAT_R16G16B16A16_FLOAT     },
        { EFormat::RGBA16_UNORM,         DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_UNORM,       DXGI_FORMAT_R16G16B16A16_UNORM     },
        { EFormat::RGBA16_SNORM,         DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_SNORM,       DXGI_FORMAT_R16G16B16A16_SNORM     },
        { EFormat::RG32_UINT,            DXGI_FORMAT_R32G32_TYPELESS,        DXGI_FORMAT_R32G32_UINT,              DXGI_FORMAT_R32G32_UINT            },
        { EFormat::RG32_SINT,            DXGI_FORMAT_R32G32_TYPELESS,        DXGI_FORMAT_R32G32_SINT,              DXGI_FORMAT_R32G32_SINT            },
        { EFormat::RG32_FLOAT,           DXGI_FORMAT_R32G32_TYPELESS,        DXGI_FORMAT_R32G32_FLOAT,             DXGI_FORMAT_R32G32_FLOAT           },
        { EFormat::RGB32_UINT,           DXGI_FORMAT_R32G32B32_TYPELESS,     DXGI_FORMAT_R32G32B32_UINT,           DXGI_FORMAT_R32G32B32_UINT         },
        { EFormat::RGB32_SINT,           DXGI_FORMAT_R32G32B32_TYPELESS,     DXGI_FORMAT_R32G32B32_SINT,           DXGI_FORMAT_R32G32B32_SINT         },
        { EFormat::RGB32_FLOAT,          DXGI_FORMAT_R32G32B32_TYPELESS,     DXGI_FORMAT_R32G32B32_FLOAT,          DXGI_FORMAT_R32G32B32_FLOAT        },
        { EFormat::RGBA32_UINT,          DXGI_FORMAT_R32G32B32A32_TYPELESS,  DXGI_FORMAT_R32G32B32A32_UINT,        DXGI_FORMAT_R32G32B32A32_UINT      },
        { EFormat::RGBA32_SINT,          DXGI_FORMAT_R32G32B32A32_TYPELESS,  DXGI_FORMAT_R32G32B32A32_SINT,        DXGI_FORMAT_R32G32B32A32_SINT      },
        { EFormat::RGBA32_FLOAT,         DXGI_FORMAT_R32G32B32A32_TYPELESS,  DXGI_FORMAT_R32G32B32A32_FLOAT,       DXGI_FORMAT_R32G32B32A32_FLOAT     },

        { EFormat::D16,                  DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_UNORM,                DXGI_FORMAT_D16_UNORM              },
        { EFormat::D24S8,                DXGI_FORMAT_R24G8_TYPELESS,         DXGI_FORMAT_R24_UNORM_X8_TYPELESS,    DXGI_FORMAT_D24_UNORM_S8_UINT      },
        { EFormat::X24G8_UINT,           DXGI_FORMAT_R24G8_TYPELESS,         DXGI_FORMAT_X24_TYPELESS_G8_UINT,     DXGI_FORMAT_D24_UNORM_S8_UINT      },
        { EFormat::D32,                  DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_FLOAT,                DXGI_FORMAT_D32_FLOAT              },
        { EFormat::D32S8,                DXGI_FORMAT_R32G8X24_TYPELESS,      DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, DXGI_FORMAT_D32_FLOAT_S8X24_UINT   },
        { EFormat::X32G8_UINT,           DXGI_FORMAT_R32G8X24_TYPELESS,      DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,  DXGI_FORMAT_D32_FLOAT_S8X24_UINT   },

        { EFormat::BC1_UNORM,            DXGI_FORMAT_BC1_TYPELESS,           DXGI_FORMAT_BC1_UNORM,                DXGI_FORMAT_BC1_UNORM              },
        { EFormat::BC1_UNORM_SRGB,       DXGI_FORMAT_BC1_TYPELESS,           DXGI_FORMAT_BC1_UNORM_SRGB,           DXGI_FORMAT_BC1_UNORM_SRGB         },
        { EFormat::BC2_UNORM,            DXGI_FORMAT_BC2_TYPELESS,           DXGI_FORMAT_BC2_UNORM,                DXGI_FORMAT_BC2_UNORM              },
        { EFormat::BC2_UNORM_SRGB,       DXGI_FORMAT_BC2_TYPELESS,           DXGI_FORMAT_BC2_UNORM_SRGB,           DXGI_FORMAT_BC2_UNORM_SRGB         },
        { EFormat::BC3_UNORM,            DXGI_FORMAT_BC3_TYPELESS,           DXGI_FORMAT_BC3_UNORM,                DXGI_FORMAT_BC3_UNORM              },
        { EFormat::BC3_UNORM_SRGB,       DXGI_FORMAT_BC3_TYPELESS,           DXGI_FORMAT_BC3_UNORM_SRGB,           DXGI_FORMAT_BC3_UNORM_SRGB         },
        { EFormat::BC4_UNORM,            DXGI_FORMAT_BC4_TYPELESS,           DXGI_FORMAT_BC4_UNORM,                DXGI_FORMAT_BC4_UNORM              },
        { EFormat::BC4_SNORM,            DXGI_FORMAT_BC4_TYPELESS,           DXGI_FORMAT_BC4_SNORM,                DXGI_FORMAT_BC4_SNORM              },
        { EFormat::BC5_UNORM,            DXGI_FORMAT_BC5_TYPELESS,           DXGI_FORMAT_BC5_UNORM,                DXGI_FORMAT_BC5_UNORM              },
        { EFormat::BC5_SNORM,            DXGI_FORMAT_BC5_TYPELESS,           DXGI_FORMAT_BC5_SNORM,                DXGI_FORMAT_BC5_SNORM              },
        { EFormat::BC6H_UFLOAT,          DXGI_FORMAT_BC6H_TYPELESS,          DXGI_FORMAT_BC6H_UF16,                DXGI_FORMAT_BC6H_UF16              },
        { EFormat::BC6H_SFLOAT,          DXGI_FORMAT_BC6H_TYPELESS,          DXGI_FORMAT_BC6H_SF16,                DXGI_FORMAT_BC6H_SF16              },
        { EFormat::BC7_UNORM,            DXGI_FORMAT_BC7_TYPELESS,           DXGI_FORMAT_BC7_UNORM,                DXGI_FORMAT_BC7_UNORM              },
        { EFormat::BC7_UNORM_SRGB,       DXGI_FORMAT_BC7_TYPELESS,           DXGI_FORMAT_BC7_UNORM_SRGB,           DXGI_FORMAT_BC7_UNORM_SRGB         },
    };

    const FDxgiFormatMapping& GetDxgiFormatMapping(EFormat Format)
    {
        static_assert(sizeof(gcDxgiFormatMappings) / sizeof(FDxgiFormatMapping) == size_t(EFormat::NUM));

        const auto& crMapping = gcDxgiFormatMappings[static_cast<UINT32>(Format)];
        assert(crMapping.Format == Format); 
        return crMapping;
    }

}
