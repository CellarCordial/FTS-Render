#ifndef RHI_DYNAMIC_H
#define RHI_DYNAMIC_H

#include "Forward.h"
#include <d3d12.h>
#include "../../Tools/include/StackArray.h"


namespace FTS
{


    //////////////////////////////////////////////////////////////////////////////////////////////
    // D3D12
    /////////////////////////////////////////////////////////////////////////////////////////////
    
    struct FDX12DeviceDesc
    {
        ID3D12Device* pD3D12Device = nullptr;
        ID3D12CommandQueue* pD3D12GraphicsCommandQueue = nullptr;
        ID3D12CommandQueue* pD3D12ComputeCommandQueue = nullptr;
        ID3D12CommandQueue* pD3D12CopyCommandQueue = nullptr;

        UINT32 dwRenderTargetViewHeapSize = 1024;
        UINT32 dwDepthStencilViewHeapSize = 1024;
        UINT32 dwShaderResourceViewHeapSize = 16384;
        UINT32 dwSamplerHeapSize = 1024;
        UINT32 dwMaxTimerQueries = 256;
    };

    BOOL CreateDevice(const FDX12DeviceDesc&, CREFIID criid, void** ppvDevice);




    extern const IID IID_IDX12RootSignature;

    struct IDX12RootSignature : public IUnknown
    {
		virtual ~IDX12RootSignature() = default;
    };
    


    struct FDX12SliceRegion
    {
        UINT64 stOffset;
        UINT64 stSize;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
    };

    enum class EDescriptorHeapType : UINT8
    {
        RenderTargetView,
        DepthStencilView,
        ShaderResourceView,
        Sampler
    };

    using FD3D12ViewportArray = TStackArray<D3D12_VIEWPORT, gdwMaxViewports>;
    using FD3D12ScissorRectArray = TStackArray<D3D12_RECT, gdwMaxViewports>;

    struct FDX12ViewportState
    {
        FD3D12ViewportArray Viewports;
        FD3D12ScissorRectArray ScissorRects;
    };

    
    //////////////////////////////////////////////////////////////////////////////////////////////
    // Vulkan
    /////////////////////////////////////////////////////////////////////////////////////////////

    // todo: 未完成





}























#endif