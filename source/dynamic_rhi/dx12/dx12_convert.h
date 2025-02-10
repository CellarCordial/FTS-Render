#ifndef RHI_D3D12_CONVERTS_H
#define RHI_D3D12_CONVERTS_H


#include "../ray_tracing.h"
#include "dx12_forward.h"
#include <d3d12.h>


namespace fantasy
{
    D3D12_RESOURCE_STATES convert_resource_states(ResourceStates states);
    D3D12_TEXTURE_ADDRESS_MODE convert_sampler_address_mode(SamplerAddressMode mode);
    UINT convert_sampler_reduction_type(SamplerReductionType reduction_type);

    D3D12_SHADER_VISIBILITY convert_shader_stage(ShaderType shader_visibility);
    D3D12_BLEND convert_blend_value(BlendFactor Factor);
    D3D12_BLEND_OP convert_blend_op(BlendOP BlendOP);
    D3D12_STENCIL_OP convert_stencil_op(StencilOP StencilOP);
    D3D12_COMPARISON_FUNC convert_comparison_func(ComparisonFunc func);
    D3D_PRIMITIVE_TOPOLOGY convert_primitive_type(PrimitiveType type, uint32_t control_points = 0);
    D3D12_BLEND_DESC convert_blend_state(const BlendState& crInState);
    D3D12_DEPTH_STENCIL_DESC convert_depth_stencil_state(const DepthStencilState& crInState);
    D3D12_RASTERIZER_DESC convert_rasterizer_state(const RasterState& crInState);
    D3D12_PRIMITIVE_TOPOLOGY_TYPE convert_primitive_topology(PrimitiveType type);

    
    struct DxgiFormatMapping
    {
        Format format;
        DXGI_FORMAT typeless_format;
        DXGI_FORMAT srv_format;
        DXGI_FORMAT rtv_dsv_format;
    };
    const DxgiFormatMapping& get_dxgi_format_mapping(Format Format);
    DX12ViewportState convert_viewport_state(const RasterState& crRasterState, const FrameBufferInfo& crFramebufferInfo, const ViewportState& crViewportState);
    D3D12_RESOURCE_DESC convert_texture_desc(const TextureDesc& desc);
    D3D12_RESOURCE_DESC convert_buffer_desc(const BufferDesc& desc);
    D3D12_SAMPLER_DESC convert_sampler_desc(const SamplerDesc& desc);
    D3D12_RESOURCE_STATES get_buffer_initial_state(const BufferDesc& desc);
    D3D12_CLEAR_VALUE convert_clear_value(const TextureDesc& desc);

    namespace ray_tracing 
    {
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS convert_accel_struct_build_flags(AccelStructBuildFlags Flags);
        D3D12_RAYTRACING_GEOMETRY_FLAGS convert_geometry_flags(GeometryFlags Flags);
        D3D12_RAYTRACING_INSTANCE_FLAGS convert_instance_flags(InstanceFlags Flags);
        D3D12_RAYTRACING_INSTANCE_DESC convert_instance_desc(const InstanceDesc& crInstanceDesc);
    }
}
























#endif