#ifndef RHI_D3D12_CONVERTS_H
#define RHI_D3D12_CONVERTS_H


#include "../../include/Pipeline.h"
#include "../../include/DynamicRHI.h"
#include <d3d12.h>

#ifdef RAY_TRACING
#include "../../include/RayTracing.h"
#endif

namespace FTS
{


    // 若 ShaderVisibility 非图形管线内的 stage, 则默认为所有着色器阶段均可见
    D3D12_SHADER_VISIBILITY ConvertShaderStage(EShaderType ShaderVisibility);

    D3D12_BLEND ConvertBlendValue(EBlendFactor Factor);

    D3D12_BLEND_OP ConvertBlendOp(EBlendOP BlendOP);

    D3D12_STENCIL_OP ConvertStencilOp(EStencilOP StencilOP);

    D3D12_COMPARISON_FUNC ConvertComparisonFunc(EComparisonFunc Func);

    // dwControlPoints 用于 PatchList
    D3D_PRIMITIVE_TOPOLOGY ConvertPrimitiveType(EPrimitiveType Type, UINT32 dwControlPoints = 0);

    D3D12_TEXTURE_ADDRESS_MODE ConvertSamplerAddressMode(ESamplerAddressMode Mode);

    UINT ConvertSamplerReductionType(ESamplerReductionType ReductionType);

    D3D12_RESOURCE_STATES ConvertResourceStates(EResourceStates States);

    D3D12_BLEND_DESC ConvertBlendState(const FBlendState& crInState);

    D3D12_DEPTH_STENCIL_DESC ConvertDepthStencilState(const FDepthStencilState& crInState);

    D3D12_RASTERIZER_DESC ConvertRasterizerState(const FRasterState& crInState);
    
    struct FDxgiFormatMapping
    {
        EFormat Format;
        DXGI_FORMAT ResourceFormat;
        DXGI_FORMAT SRVFormat;
        DXGI_FORMAT RTVFormat;
    };
    const FDxgiFormatMapping& GetDxgiFormatMapping(EFormat Format);

    FDX12ViewportState ConvertViewportState(const FRasterState& crRasterState, const FFrameBufferInfo& crFramebufferInfo, const FViewportState& crViewportState);

    D3D12_RESOURCE_DESC ConvertTextureDesc(const FTextureDesc& crDesc);

    D3D12_CLEAR_VALUE ConvertClearValue(const FTextureDesc& crDesc);


#ifdef RAY_TRACING
    namespace RayTracing 
    {
        inline D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS ConvertAccelStructureBuildFlags(EAccelStructBuildFlags Flags)
        {
            switch (Flags)
            {
            case EAccelStructBuildFlags::None: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
            case EAccelStructBuildFlags::AllowUpdate: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
            case EAccelStructBuildFlags::AllowCompaction: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
            case EAccelStructBuildFlags::PreferFastTrace: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
            case EAccelStructBuildFlags::PreferFastBuild: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
            case EAccelStructBuildFlags::MinimizeMemory: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY;
            case EAccelStructBuildFlags::PerformUpdate: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
            }
        }

        inline D3D12_RAYTRACING_GEOMETRY_FLAGS ConvertGeometryFlags(EGeometryFlags Flags)
        {
            switch (Flags)
            {
               case EGeometryFlags::None: return D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
			   case EGeometryFlags::Opaque: return D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
			   case EGeometryFlags::NoDuplicateAnyHitInvocation: return D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;
            }
        }
    }

#endif
}
























#endif