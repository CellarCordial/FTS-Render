/**
 * *****************************************************************************
 * @file        DX12Pipeline.h
 * @brief       
 * @author      CellarCordial (591885295@qq.com)
 * @date        2024-06-01
 * @copyright Copyright (c) 2024
 * *****************************************************************************
 */

 #ifndef RHI_DX12_PIPELINE_H
 #define RHI_DX12_PIPELINE_H


#include "../../../Core/include/ComRoot.h"
#include "../../include/Pipeline.h"
#include "DX12Descriptor.h"
#include "DX12Forward.h"
#include <unordered_map>
#include <urlmon.h>
#include <vector>

namespace FTS
{
    class FDX12InputLayout :
        public TComObjectRoot<FComMultiThreadModel>,
        public IInputLayout
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12InputLayout)
            INTERFACE_ENTRY(IID_IInputLayout, IInputLayout)
        END_INTERFACE_MAP

        FDX12InputLayout(const FDX12Context* cpContext);

        BOOL Initialize(const FVertexAttributeDescArray& crVertexAttributeDescs);

        // IInputLayout
        UINT32 GetAttributesNum() const override;
        FVertexAttributeDesc GetAttributeDesc(UINT32 dwAttributeIndex) const override;

        
        D3D12_INPUT_LAYOUT_DESC GetD3D12InputLayoutDesc() const;

    public:
        std::unordered_map<UINT32, UINT32> m_SlotStrideMap;     /**< Maps a binding slot to an element stride.  */
        
    private:
        const FDX12Context* m_cpContext; 
        std::vector<FVertexAttributeDesc> m_VertexAttributeDescs;
        std::vector<D3D12_INPUT_ELEMENT_DESC> m_D3D12InputElementDescs;
    };


    class FDX12BindingLayout :
        public TComObjectRoot<FComMultiThreadModel>,
        public IBindingLayout
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12BindingLayout)
            INTERFACE_ENTRY(IID_IBindingLayout, IBindingLayout)
        END_INTERFACE_MAP

        FDX12BindingLayout(const FDX12Context* cpContext, const FBindingLayoutDesc& crDesc);

        BOOL Initialize();

        // IBindingLayout
        FBindingLayoutDesc GetBindingDesc() const override { return m_Desc; }
        FBindlessLayoutDesc GetBindlessDesc() const override { assert(false); return FBindlessLayoutDesc{}; }
        BOOL IsBindingless() const override { return false; }

    public:
        UINT32 m_dwPushConstantSize = 0;

        // The index in the m_RootParameters.  
        UINT32 m_dwRootParameterPushConstantsIndex = ~0u;
        UINT32 m_dwRootParameterSRVetcIndex = ~0u;
        UINT32 m_dwRootParameterSamplerIndex = ~0u;

        UINT32 m_dwDescriptorTableSRVetcSize = 0;
        UINT32 m_dwDescriptorTableSamplerSize = 0;
        
        std::vector<D3D12_ROOT_PARAMETER1> m_RootParameters;    

        // UINT32: The index in the m_RootParameters. 
        std::vector<std::pair<UINT32, D3D12_ROOT_DESCRIPTOR1>> m_DescriptorVolatileCBs;
        std::vector<D3D12_DESCRIPTOR_RANGE1> m_DescriptorSRVetcRanges;
        std::vector<D3D12_DESCRIPTOR_RANGE1> m_DescriptorSamplerRanges;

    private:
        const FDX12Context* m_cpContext;
        FBindingLayoutDesc m_Desc;

        std::vector<FBindingLayoutItem> m_SRVetcBindingLayouts;
    };


    class FDX12BindlessLayout :
        public TComObjectRoot<FComMultiThreadModel>,
        public IBindingLayout
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12BindlessLayout)
            INTERFACE_ENTRY(IID_IBindingLayout, IBindingLayout)
        END_INTERFACE_MAP

        FDX12BindlessLayout(const FDX12Context* cpContext, const FBindlessLayoutDesc& crDesc);

        BOOL Initialize();

        // IBindingLayout
        FBindingLayoutDesc GetBindingDesc() const override { assert(false); return FBindingLayoutDesc{}; }
        FBindlessLayoutDesc GetBindlessDesc() const override { return m_Desc; }
        BOOL IsBindingless() const override { return true; }

    public:
        D3D12_ROOT_PARAMETER1 m_RootParameter;

    private:
        const FDX12Context* m_cpContext;
        FBindlessLayoutDesc m_Desc;

        // There only one Root Parameter which is Descriptor Table. 
        std::vector<D3D12_DESCRIPTOR_RANGE1> m_DescriptorRanges;
    };


    class FDX12RootSignature :
        public TComObjectRoot<FComMultiThreadModel>,
        public IDX12RootSignature
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12RootSignature)
            INTERFACE_ENTRY(IID_IDX12RootSignature, IDX12RootSignature)
        END_INTERFACE_MAP

        FDX12RootSignature(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps);

        ~FDX12RootSignature() noexcept;

        BOOL Initialize(
            const FPipelineBindingLayoutArray& crpBindingLayouts,
            BOOL bAllowInputLayout,
            const D3D12_ROOT_PARAMETER1* pCustomParameters = nullptr,
            UINT32 dwCustomParametersNum = 0
        );
        
    public:
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_pD3D12RootSignature;

        // UINT32: RootParameter index. 
        std::vector<std::pair<TComPtr<IBindingLayout>, UINT32>> m_BindingLayoutIndexMap;
        
        UINT32 m_dwPushConstantSize = 0;
        UINT32 m_dwRootParameterPushConstantsIndex = ~0u;
        
        // The index in m_pDescriptorHeaps->RootSignatureMap. 
        UINT64 m_dwHashIndex = 0;

    private:
        const FDX12Context* m_cpContext;
        FDX12DescriptorHeaps* m_pDescriptorHeaps;
    };



    class FDX12GraphicsPipeline :
        public TComObjectRoot<FComMultiThreadModel>,
        public IGraphicsPipeline
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12GraphicsPipeline)
            INTERFACE_ENTRY(IID_IGraphicsPipeline, IGraphicsPipeline)
        END_INTERFACE_MAP

        FDX12GraphicsPipeline(
            const FDX12Context* cpContext,
            const FGraphicsPipelineDesc& crDesc,
            TComPtr<IDX12RootSignature> pDX12RootSignature,
            const FFrameBufferInfo& crFrameBufferInfo
        );

        BOOL Initialize();


        // IGraphicsPipeline
        FGraphicsPipelineDesc GetDesc() const override { return m_Desc; }
        FFrameBufferInfo GetFrameBufferInfo() const override { return m_FrameBufferInfo; }

    public:
        BOOL m_bRequiresBlendFactor = false;

        TComPtr<IDX12RootSignature> m_pDX12RootSignature;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pD3D12PipelineState;

    private:
        const FDX12Context* m_cpContext;

        FGraphicsPipelineDesc m_Desc;
        FFrameBufferInfo m_FrameBufferInfo;
    };

    
    class FDX12ComputePipeline :
        public TComObjectRoot<FComMultiThreadModel>,
        public IComputePipeline
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12ComputePipeline)
            INTERFACE_ENTRY(IID_IComputePipeline, IComputePipeline)
        END_INTERFACE_MAP

        FDX12ComputePipeline(const FDX12Context* cpContext, const FComputePipelineDesc& crDesc, TComPtr<IDX12RootSignature> pDX12RootSignature);

        BOOL Initialize();

        // IComputePipeline
        FComputePipelineDesc GetDesc() const override { return m_Desc; }

    public:
        TComPtr<IDX12RootSignature> m_pDX12RootSignature;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pD3D12PipelineState;
        
    private:
        const FDX12Context* m_cpContext;
        FComputePipelineDesc m_Desc;
    };


    class FDX12BindingSet :
        public TComObjectRoot<FComMultiThreadModel>,
        public IBindingSet
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12BindingSet)
            INTERFACE_ENTRY(IID_IBindingSet, IBindingSet)
        END_INTERFACE_MAP

        FDX12BindingSet(
            const FDX12Context* cpContext,
            FDX12DescriptorHeaps* pDescriptorHeaps,
            const FBindingSetDesc& crDesc,
            IBindingLayout* pBindingLayout
        );

        ~FDX12BindingSet() noexcept;

        BOOL Initialize();


        // IBindingSet
        FBindingSetDesc GetDesc() const override { return m_Desc; }
        IBindingLayout* GetLayout() const override { assert(m_pBindingLayout != nullptr); return m_pBindingLayout.Get();}
        BOOL IsDescriptorTable() const override { return false; }

    public:
        
        UINT32 m_dwDescriptorTableSRVetcBaseIndex = 0;
        UINT32 m_dwDescriptorTableSamplerBaseIndex = 0;
        UINT32 m_dwRootParameterSRVetcIndex = 0;
        UINT32 m_dwRootParameterSamplerIndex = 0;

        BOOL m_bDescriptorTableValidSRVetc = false;
        BOOL m_bDescriptorTableValidSampler = false;
        BOOL m_bHasUAVBindings = false;

        // UINT32: The index in the m_RootParameters. 
        std::vector<std::pair<UINT32, IBuffer*>> m_RootParameterIndexVolatileCBMaps;

        std::vector<UINT16> m_wBindingsWhichNeedTransition;
        

    private:
        const FDX12Context* m_cpContext;
        FDX12DescriptorHeaps* m_pDescriptorHeaps;

        FBindingSetDesc m_Desc;

        std::vector<TComPtr<IResource>> m_pRefResources;
        TComPtr<IBindingLayout> m_pBindingLayout;
    };


    class FDX12DescriptorTable :
        public TComObjectRoot<FComMultiThreadModel>,
        public IDescriptorTable
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12DescriptorTable)
            INTERFACE_ENTRY(IID_IDescriptorTable, IDescriptorTable)
        END_INTERFACE_MAP

        FDX12DescriptorTable(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps);
        ~FDX12DescriptorTable() noexcept;

        BOOL Initialize();


        // IBindingSet
        FBindingSetDesc GetDesc() const override { assert(false); return FBindingSetDesc{}; }
        IBindingLayout* GetLayout() const override { assert(false); return nullptr;}
        BOOL IsDescriptorTable() const override { return true; }

        // IDescriptorTable
        UINT32 GetCapacity() const override { return m_dwCapacity; }

    public:
        UINT32 m_dwFirstDescriptorIndex = gdwInvalidViewIndex;
        UINT32 m_dwCapacity = 0;

    private:
        const FDX12Context* m_cpContext;
        FDX12DescriptorHeaps* m_pDescriptorHeaps;
    };

}




















 #endif