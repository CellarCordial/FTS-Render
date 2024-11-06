#include "DX12Pipeline.h"
#include "DX12Converts.h"
#include "../Utils.h"
#include "DX12Forward.h"
#include "DX12Resource.h"
#include <cassert>
#include <combaseapi.h>
#include <d3d12.h>
#include <d3dcommon.h>
#include <sstream>
#include <utility>
#include "../StateTrack.h"

namespace FTS 
{
    void CreateNullBufferSRV(UINT64 stDescriptorAddress, EFormat Format, const FDX12Context* cpContext)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC ViewDesc{};
        ViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        ViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        ViewDesc.Format = GetDxgiFormatMapping(Format == EFormat::UNKNOWN ? EFormat::R32_UINT : Format).SRVFormat;
        cpContext->pDevice->CreateShaderResourceView(nullptr, &ViewDesc, D3D12_CPU_DESCRIPTOR_HANDLE{stDescriptorAddress });
    }
    
    void CreateNullBufferUAV(UINT64 stDescriptorAddress, EFormat Format, const FDX12Context* cpContext)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC ViewDesc{};
        ViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        ViewDesc.Format = GetDxgiFormatMapping(Format == EFormat::UNKNOWN ? EFormat::R32_UINT : Format).SRVFormat;
        cpContext->pDevice->CreateUnorderedAccessView(nullptr, nullptr, &ViewDesc, D3D12_CPU_DESCRIPTOR_HANDLE{stDescriptorAddress });
    }

    FDX12InputLayout::FDX12InputLayout(const FDX12Context* cpContext) :
        m_cpContext(cpContext)
    {
    }

    BOOL FDX12InputLayout::Initialize(const FVertexAttributeDescArray& crVertexAttributeDescs)
    {
        m_VertexAttributeDescs.resize(crVertexAttributeDescs.Size());
        for (UINT32 ix = 0; ix < m_VertexAttributeDescs.size(); ++ix)
        {
            auto& rAttributeDesc = m_VertexAttributeDescs[ix];
            rAttributeDesc = crVertexAttributeDescs[ix];
            
            if (rAttributeDesc.dwArraySize == 0)
            {
                std::stringstream ss;
                ss << "Create FDX12InputLayout failed for FVertexAttributeDesc.dwArraySize = " << rAttributeDesc.dwArraySize << "";
                LOG_ERROR(ss.str());
                return false;
            }

            const auto& crFormatMapping = GetDxgiFormatMapping(rAttributeDesc.Format);
            const auto& crFormatInfo = GetFormatInfo(rAttributeDesc.Format);

            for (UINT32 dwSemanticIndex = 0; dwSemanticIndex < rAttributeDesc.dwArraySize; ++dwSemanticIndex)
            {
                D3D12_INPUT_ELEMENT_DESC& rInputElementDesc = m_D3D12InputElementDescs.emplace_back();
                rInputElementDesc.SemanticName = rAttributeDesc.strName.c_str();
                rInputElementDesc.SemanticIndex = dwSemanticIndex;
                rInputElementDesc.InputSlot = rAttributeDesc.dwBufferIndex;
                rInputElementDesc.Format = crFormatMapping.SRVFormat;
                rInputElementDesc.AlignedByteOffset = rAttributeDesc.dwOffset + dwSemanticIndex * crFormatInfo.btBytesPerBlock;

                if (rAttributeDesc.bIsInstanced)
                {
                    rInputElementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                    rInputElementDesc.InstanceDataStepRate = 1; 
                }
                else 
                {
                    rInputElementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                    rInputElementDesc.InstanceDataStepRate = 0;
                }
            }

            if (!m_SlotStrideMap.contains(rAttributeDesc.dwBufferIndex))
            {
                m_SlotStrideMap[rAttributeDesc.dwBufferIndex] = rAttributeDesc.dwElementStride;
            }
            else 
            {
                if (m_SlotStrideMap[rAttributeDesc.dwBufferIndex] != rAttributeDesc.dwElementStride)
                {
                    std::stringstream ss;
                    ss  << "Create FDX12InputLayout failed for "
                        <<"m_SlotStrideMap[rAttributeDesc.dwBufferIndex]: " << m_SlotStrideMap[rAttributeDesc.dwBufferIndex]
                        << " != rAttributeDesc.dwElementStride: " << rAttributeDesc.dwElementStride << "";
                    LOG_ERROR(ss.str());
                    return false;
                }
            }
        }
        return true;
    }


    UINT32 FDX12InputLayout::GetAttributesNum() const
    {
        return static_cast<UINT32>(m_VertexAttributeDescs.size());
    }

    FVertexAttributeDesc FDX12InputLayout::GetAttributeDesc(UINT32 dwAttributeIndex) const
    {
        assert(dwAttributeIndex < static_cast<UINT32>(m_VertexAttributeDescs.size()));
        return m_VertexAttributeDescs[dwAttributeIndex];
    }


    D3D12_INPUT_LAYOUT_DESC FDX12InputLayout::GetD3D12InputLayoutDesc() const
    {
        D3D12_INPUT_LAYOUT_DESC Res;
        Res.NumElements = static_cast<UINT>(m_D3D12InputElementDescs.size());
        Res.pInputElementDescs = m_D3D12InputElementDescs.data();
        return Res;
    }


    FDX12BindingLayout::FDX12BindingLayout(const FDX12Context* cpContext, const FBindingLayoutDesc& crDesc) :
        m_cpContext(cpContext), m_Desc(crDesc)
    {
    }

    BOOL FDX12BindingLayout::Initialize()
    {
        EResourceType CurrentResourceType = EResourceType(-1);
        UINT32 dwCurrSlot = ~0u;

        D3D12_ROOT_CONSTANTS RootConstants = {};    // 只允许一个 PushConstants

        for (const auto& crBinding : m_Desc.BindingLayoutItems)
        {
            if (crBinding.Type == EResourceType::PushConstants)
            {
                m_dwPushConstantSize = crBinding.wSize;
                RootConstants.Num32BitValues = crBinding.wSize / 4;
                RootConstants.RegisterSpace = m_Desc.dwRegisterSpace;
                RootConstants.ShaderRegister = crBinding.dwSlot;
            }
            else if (crBinding.Type == EResourceType::VolatileConstantBuffer)
            {
                D3D12_ROOT_DESCRIPTOR1 RootDescriptor;
                RootDescriptor.RegisterSpace = m_Desc.dwRegisterSpace;
                RootDescriptor.ShaderRegister = crBinding.dwSlot;

                /**
                 * @brief       Volatile CBs are static descriptors, however strange that may seem.
                                A volatile CB can only be bound to a command list after it's been written into, and 
                                after that the data will not change until the command list has finished executing.
                                Subsequent writes will be made into a newly allocated portion of an upload buffer.
                 * 
                 */
                RootDescriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
                m_DescriptorVolatileCBs.push_back(std::make_pair(-1, RootDescriptor));
            }
            else if (!AreResourceTypesCompatible(crBinding.Type, CurrentResourceType) || crBinding.dwSlot != dwCurrSlot + 1)
            {
                // If resource type changes or resource binding slot changes, then start a new range. 

                if (crBinding.Type == EResourceType::Sampler)
                {
                    auto& rRange = m_DescriptorSamplerRanges.emplace_back();
                    rRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                    rRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
                    rRange.NumDescriptors = 1;
                    rRange.BaseShaderRegister = crBinding.dwSlot;
                    rRange.RegisterSpace = m_Desc.dwRegisterSpace;
                    rRange.OffsetInDescriptorsFromTableStart = m_dwDescriptorTableSamplerSize++;
                }
                else 
                {
                    auto& rRange = m_DescriptorSRVetcRanges.emplace_back();

                    switch (crBinding.Type) 
                    {
                    case EResourceType::Texture_SRV:
                    case EResourceType::TypedBuffer_SRV:
                    case EResourceType::StructuredBuffer_SRV:
                    case EResourceType::RawBuffer_SRV:
                        rRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                        break;

                    case EResourceType::Texture_UAV:
                    case EResourceType::TypedBuffer_UAV:
                    case EResourceType::StructuredBuffer_UAV:
                    case EResourceType::RawBuffer_UAV:
                        rRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                        break;

                    case EResourceType::ConstantBuffer:
                        rRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                        break;

                    case EResourceType::None:
                    case EResourceType::VolatileConstantBuffer:
                    case EResourceType::Sampler:
                    case EResourceType::PushConstants:
                    case EResourceType::Count:
                        assert(!"Invalid Enumeration Value");
                        continue;
                    }

                    rRange.BaseShaderRegister = crBinding.dwSlot;
                    rRange.RegisterSpace = m_Desc.dwRegisterSpace;
                    rRange.NumDescriptors = 1;
                    rRange.OffsetInDescriptorsFromTableStart = m_dwDescriptorTableSRVetcSize++;

                    /**
                     * @brief       We don't know how apps will use resources referenced in a binding set. 
                                    They may bind a buffer to the command list and then copy data into it.  
                     * 
                     */
                    rRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

                    m_SRVetcBindingLayouts.push_back(crBinding);
                }

                dwCurrSlot++;
                CurrentResourceType = crBinding.Type;
            }
            else 
            {
                // The resource type doesn't change or resource binding slot doesn't change, 
                // then extend the current range. 
                if (crBinding.Type == EResourceType::Sampler)
                {
                    if (m_DescriptorSamplerRanges.empty())
                    {
                        LOG_ERROR("Create FDX12BindingLayout failed because m_DescriptorSamplaerRanges is empty.");
                        return false;
                    }
                    auto& rRange = m_DescriptorSamplerRanges.back();
                    rRange.NumDescriptors++;
                    m_dwDescriptorTableSamplerSize++;
                }
                else 
                {
                    if (m_DescriptorSRVetcRanges.empty())
                    {
                        LOG_ERROR("Create FDX12BindingLayout failed because m_DescriptorSamplaerRanges is empty.");
                        return false;
                    } 
                    auto& rRange = m_DescriptorSRVetcRanges.back();
                    rRange.NumDescriptors++;
                    m_dwDescriptorTableSRVetcSize++;
                    m_SRVetcBindingLayouts.push_back(crBinding);
                }

                dwCurrSlot = crBinding.dwSlot;
            }
        }

        // A PipelineBindingLayout occupies a contiguous segment of a root signature.
        // The root parameter indices stored here are relative to the beginning of that segment, not to the RS item 0.

        D3D12_SHADER_VISIBILITY ShaderVisibility = ConvertShaderStage(m_Desc.ShaderVisibility);
        m_RootParameters.resize(0);

        if (RootConstants.Num32BitValues > 0)
        {
            auto& rRootParameter = m_RootParameters.emplace_back();
            rRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            rRootParameter.ShaderVisibility = ShaderVisibility;
            rRootParameter.Constants = RootConstants;

            m_dwRootParameterPushConstantsIndex = static_cast<UINT32>(m_RootParameters.size()) - 1;
        }

        for (auto& rVolatileCB : m_DescriptorVolatileCBs)
        {
            auto& rRootParameter = m_RootParameters.emplace_back();
            rRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            rRootParameter.ShaderVisibility = ShaderVisibility;
            rRootParameter.Descriptor = rVolatileCB.second;

            rVolatileCB.first = static_cast<UINT32>(m_RootParameters.size()) - 1;
        }

        if (m_dwDescriptorTableSRVetcSize > 0)
        {
            auto& rRootParameter = m_RootParameters.emplace_back();
            rRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rRootParameter.ShaderVisibility = ShaderVisibility;
            rRootParameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT32>(m_DescriptorSRVetcRanges.size());
            rRootParameter.DescriptorTable.pDescriptorRanges = m_DescriptorSRVetcRanges.data();

            m_dwRootParameterSRVetcIndex = static_cast<UINT32>(m_RootParameters.size()) - 1;
        }

        if (m_dwDescriptorTableSamplerSize > 0)
        {
            auto& rRootParameter = m_RootParameters.emplace_back();
            rRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rRootParameter.ShaderVisibility = ShaderVisibility;
            rRootParameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT32>(m_DescriptorSamplerRanges.size());
            rRootParameter.DescriptorTable.pDescriptorRanges = m_DescriptorSamplerRanges.data();

            m_dwRootParameterSamplerIndex = static_cast<UINT32>(m_RootParameters.size()) - 1;
        }

        return true;
    }

    FDX12BindlessLayout::FDX12BindlessLayout(const FDX12Context* cpContext, const FBindlessLayoutDesc& crDesc) :
        m_cpContext(cpContext), m_Desc(crDesc), m_RootParameter()
    {
    }
    
    BOOL FDX12BindlessLayout::Initialize()
    {
        for (const auto& crBinding : m_Desc.BindingLayoutItems)
        {
            auto& rRange = m_DescriptorRanges.emplace_back();

            switch (crBinding.Type)
            {
            case EResourceType::Texture_SRV: 
            case EResourceType::TypedBuffer_SRV:
            case EResourceType::StructuredBuffer_SRV:
            case EResourceType::RawBuffer_SRV:
                rRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                break;

            case EResourceType::ConstantBuffer:
                rRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                break;

            case EResourceType::Texture_UAV:
            case EResourceType::TypedBuffer_UAV:
            case EResourceType::StructuredBuffer_UAV:
            case EResourceType::RawBuffer_UAV:
                rRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                break;

            case EResourceType::Sampler:
                rRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                break;

            case EResourceType::None:
            case EResourceType::VolatileConstantBuffer:
            case EResourceType::PushConstants:
            case EResourceType::Count:
                assert(!"Invalid Enumeration Value");
                continue;
            }

            rRange.NumDescriptors = ~0u;    // Unbounded. 
            rRange.BaseShaderRegister = m_Desc.dwFirstSlot;
            rRange.RegisterSpace = crBinding.dwRegisterSpace;
            rRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
            rRange.OffsetInDescriptorsFromTableStart = 0;
        }

        m_RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        m_RootParameter.ShaderVisibility = ConvertShaderStage(m_Desc.ShaderVisibility);
        m_RootParameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT32>(m_DescriptorRanges.size());
        m_RootParameter.DescriptorTable.pDescriptorRanges = m_DescriptorRanges.data();
        
        return true;
    }

    FDX12RootSignature::FDX12RootSignature(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps) :
        m_cpContext(cpContext), m_pDescriptorHeaps(pDescriptorHeaps)
    {
    }

    FDX12RootSignature::~FDX12RootSignature() noexcept
    {
        auto Iter = m_pDescriptorHeaps->RootSignatureMap.find(m_dwHashIndex);
        if (Iter != m_pDescriptorHeaps->RootSignatureMap.end())
        {
            m_pDescriptorHeaps->RootSignatureMap.erase(Iter);
        }
    }

    BOOL FDX12RootSignature::Initialize(
        const FPipelineBindingLayoutArray& crpBindingLayouts,
        BOOL bAllowInputLayout,
        const D3D12_ROOT_PARAMETER1* pCustomParameters,
        UINT32 dwCustomParametersNum
    )
    {
        std::vector<D3D12_ROOT_PARAMETER1> D3D12RootParameters(
            &pCustomParameters[0], 
            &pCustomParameters[dwCustomParametersNum]
        );

        for (UINT32 ix = 0; ix < crpBindingLayouts.Size(); ++ix)
        {
            UINT32 dwOffset = static_cast<UINT32>(D3D12RootParameters.size());
            m_BindingLayoutIndexMap.emplace_back(std::make_pair(crpBindingLayouts[ix], dwOffset));
            

            if (crpBindingLayouts[ix]->IsBindingless())
            {
                FDX12BindlessLayout* pDX12BindlessLayout = CheckedCast<FDX12BindlessLayout*>(crpBindingLayouts[ix]);

                D3D12RootParameters.push_back(pDX12BindlessLayout->m_RootParameter);
            }
            else
            {
                FDX12BindingLayout* pDX12BindingLayout = CheckedCast<FDX12BindingLayout*>(crpBindingLayouts[ix]);

                D3D12RootParameters.insert(
                    D3D12RootParameters.end(),
                    pDX12BindingLayout->m_RootParameters.begin(),
                    pDX12BindingLayout->m_RootParameters.end()
                );

                m_dwPushConstantSize = pDX12BindingLayout->m_dwPushConstantSize;
                if (m_dwPushConstantSize > 0)
                {
                    m_dwRootParameterPushConstantsIndex = pDX12BindingLayout->m_dwRootParameterPushConstantsIndex + dwOffset;
                }
            }
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC Desc = {};
        Desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (bAllowInputLayout) Desc.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        Desc.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

        if (!D3D12RootParameters.empty())
        {
            Desc.Desc_1_1.pParameters = D3D12RootParameters.data();
            Desc.Desc_1_1.NumParameters = static_cast<UINT32>(D3D12RootParameters.size());
        }

        TComPtr<ID3DBlob> pRootSignatureBlob;
        TComPtr<ID3DBlob> pErrorBlob;

        if (FAILED(D3D12SerializeVersionedRootSignature(&Desc, pRootSignatureBlob.GetAddressOf(), pErrorBlob.GetAddressOf())))
        {
            std::stringstream ss("D3D12SerializeVersionedRootSignature call failed.\n");
            ss << static_cast<CHAR*>(pErrorBlob->GetBufferPointer());
            LOG_ERROR(ss.str());
            return false;
        }

        if (FAILED(m_cpContext->pDevice->CreateRootSignature(
            0, 
            pRootSignatureBlob->GetBufferPointer(), 
            pRootSignatureBlob->GetBufferSize(), 
            IID_PPV_ARGS(m_pD3D12RootSignature.GetAddressOf())
        )))
        {
            LOG_ERROR("ID3D12Device::CreateRootSignature call failed.");
            return false;
        }
        return true;
    }

    FDX12GraphicsPipeline::FDX12GraphicsPipeline(
        const FDX12Context* cpContext,
        const FGraphicsPipelineDesc& crDesc,
        TComPtr<IDX12RootSignature> pDX12RootSignature,
        const FFrameBufferInfo& crFrameBufferInfo
    ) :
        m_cpContext(cpContext), 
        m_Desc(crDesc), 
        m_pDX12RootSignature(pDX12RootSignature), 
        m_FrameBufferInfo(crFrameBufferInfo),
        m_bRequiresBlendFactor(crDesc.RenderState.BlendState.IfUseConstantColor(crFrameBufferInfo.RTVFormats.Size()))
    {
    }

    BOOL FDX12GraphicsPipeline::Initialize()
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateDesc{};
        PipelineStateDesc.SampleMask = ~0u;

        FDX12RootSignature* pRootSignature = CheckedCast<FDX12RootSignature*>(m_pDX12RootSignature.Get());
        PipelineStateDesc.pRootSignature = pRootSignature->m_pD3D12RootSignature.Get();

        if (m_Desc.VS != nullptr)
        {
            if (!m_Desc.VS->GetBytecode(&PipelineStateDesc.VS.pShaderBytecode, &PipelineStateDesc.VS.BytecodeLength))
            {
                FShaderDesc Desc = m_Desc.VS->GetDesc();
                std::stringstream ss;
                ss << "Load vertex shader: " << Desc.strDebugName << " failed.";
                LOG_ERROR(ss.str().c_str());
                return false;
            }
        }
        if (m_Desc.HS != nullptr)
        {
            if (!m_Desc.HS->GetBytecode(&PipelineStateDesc.HS.pShaderBytecode, &PipelineStateDesc.HS.BytecodeLength))
            {
                FShaderDesc Desc = m_Desc.HS->GetDesc();
                std::stringstream ss;
                ss << "Load hull shader: " << Desc.strDebugName << " failed.";
                LOG_ERROR(ss.str().c_str());
                return false;
            }
        }
        if (m_Desc.DS != nullptr)
        {
            if (!m_Desc.DS->GetBytecode(&PipelineStateDesc.DS.pShaderBytecode, &PipelineStateDesc.DS.BytecodeLength))
            {
                FShaderDesc Desc = m_Desc.DS->GetDesc();
                std::stringstream ss;
                ss << "Load domain shader: " << Desc.strDebugName << " failed.";
                LOG_ERROR(ss.str().c_str());
                return false;
            }
        }
        if (m_Desc.GS != nullptr)
        {
            if (!m_Desc.GS->GetBytecode(&PipelineStateDesc.GS.pShaderBytecode, &PipelineStateDesc.GS.BytecodeLength))
            {
                FShaderDesc Desc = m_Desc.GS->GetDesc();
                std::stringstream ss;
                ss << "Load geometry shader: " << Desc.strDebugName << " failed.";
                LOG_ERROR(ss.str().c_str());
                return false;
            }
        }
        if (m_Desc.PS != nullptr)
        {
            if (!m_Desc.PS->GetBytecode(&PipelineStateDesc.PS.pShaderBytecode, &PipelineStateDesc.PS.BytecodeLength))
            {
                FShaderDesc Desc = m_Desc.PS->GetDesc();
                std::stringstream ss;
                ss << "Load pixel shader: " << Desc.strDebugName << " failed.";
                LOG_ERROR(ss.str().c_str());
                return false;
            }
        }

        PipelineStateDesc.BlendState = ConvertBlendState(m_Desc.RenderState.BlendState);

        const FDepthStencilState& crDepthStencilState = m_Desc.RenderState.DepthStencilState;
        PipelineStateDesc.DepthStencilState = ConvertDepthStencilState(crDepthStencilState);

        if ((crDepthStencilState.bDepthTestEnable || crDepthStencilState.bStencilEnable) && m_FrameBufferInfo.DepthFormat == EFormat::UNKNOWN)
        {
            PipelineStateDesc.DepthStencilState.DepthEnable = false;
            PipelineStateDesc.DepthStencilState.StencilEnable = false;

            LOG_WARN("DepthEnable or stencilEnable is true, but no depth target is bound.");
        }

        PipelineStateDesc.RasterizerState = ConvertRasterizerState(m_Desc.RenderState.RasterizerState);

        switch (m_Desc.PrimitiveType)
        {
        case EPrimitiveType::PointList: PipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
        case EPrimitiveType::LineList: PipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;

        case EPrimitiveType::TriangleList: 
        case EPrimitiveType::TriangleStrip: 
        case EPrimitiveType::TriangleListWithAdjacency:
        case EPrimitiveType::TriangleStripWithAdjacency:
            PipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
            
        case EPrimitiveType::PatchList: PipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH; break;
        }
        
        PipelineStateDesc.SampleDesc = { m_FrameBufferInfo.dwSampleCount, m_FrameBufferInfo.dwSampleQuality };

        PipelineStateDesc.DSVFormat = GetDxgiFormatMapping(m_FrameBufferInfo.DepthFormat).RTVFormat;
        PipelineStateDesc.NumRenderTargets = m_FrameBufferInfo.RTVFormats.Size();
        for (UINT32 ix = 0; ix < m_FrameBufferInfo.RTVFormats.Size(); ++ix)
        {
            PipelineStateDesc.RTVFormats[ix] = GetDxgiFormatMapping(m_FrameBufferInfo.RTVFormats[ix]).RTVFormat;
        }

        if (m_Desc.pInputLayout != nullptr)
        {
            FDX12InputLayout* pDX12InputLayout = CheckedCast<FDX12InputLayout*>(m_Desc.pInputLayout);

            PipelineStateDesc.InputLayout = pDX12InputLayout->GetD3D12InputLayoutDesc();

            if (PipelineStateDesc.InputLayout.NumElements == 0) PipelineStateDesc.InputLayout.pInputElementDescs = nullptr;
        }

        if (FAILED(m_cpContext->pDevice->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(m_pD3D12PipelineState.GetAddressOf()))))
        {
            LOG_ERROR("Failed to create D3D12 graphics pipeline state.");
            return false;
        }
        return true;
    }

    FDX12ComputePipeline::FDX12ComputePipeline(const FDX12Context* cpContext, const FComputePipelineDesc& crDesc, TComPtr<IDX12RootSignature> pDX12RootSignature) :
        m_cpContext(cpContext), m_Desc(crDesc), m_pDX12RootSignature(pDX12RootSignature)
    {
    }

    BOOL FDX12ComputePipeline::Initialize()
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC PipelineStateDesc{};

        FDX12RootSignature* pRootSignature = CheckedCast<FDX12RootSignature*>(m_pDX12RootSignature.Get());
        PipelineStateDesc.pRootSignature = pRootSignature->m_pD3D12RootSignature.Get();
        
        if (m_Desc.CS == nullptr) 
        {
            LOG_ERROR("Compute shader is missing.");
            return false;
        }
        else if (!m_Desc.CS->GetBytecode(&PipelineStateDesc.CS.pShaderBytecode, &PipelineStateDesc.CS.BytecodeLength))
        {
            FShaderDesc Desc = m_Desc.CS->GetDesc();
            std::stringstream ss;
            ss << "Load compute shader: " << Desc.strDebugName << " failed.";
            LOG_ERROR(ss.str().c_str());
            return false;
        }
        
        if (FAILED(m_cpContext->pDevice->CreateComputePipelineState(&PipelineStateDesc, IID_PPV_ARGS(m_pD3D12PipelineState.GetAddressOf()))))
        {
            LOG_ERROR("Failed to create D3D12 compute pipeline state.");
            return false;
        }
        return true;
    }
    

    FDX12BindingSet::FDX12BindingSet(
        const FDX12Context* cpContext,
        FDX12DescriptorHeaps* pDescriptorHeaps,
        const FBindingSetDesc& crDesc,
        IBindingLayout* pBindingLayout
    ) :
        m_cpContext(cpContext), 
        m_pDescriptorHeaps(pDescriptorHeaps), 
        m_Desc(crDesc), 
        m_pBindingLayout(pBindingLayout)
    {
    }

    BOOL FDX12BindingSet::Initialize()
    {
        // 下面都是为各个 resource 创建 view

        FDX12BindingLayout* pDX12BindingLayout = CheckedCast<FDX12BindingLayout*>(m_pBindingLayout.Get());

        for (const auto& [dwIndex, crD3D12RootDescriptor] : pDX12BindingLayout->m_DescriptorVolatileCBs)
        {
            IBuffer* pFoundBuffer;
            for (const auto& crBinding : m_Desc.BindingItems)
            {
                if (crBinding.Type == EResourceType::VolatileConstantBuffer && crBinding.dwSlot == crD3D12RootDescriptor.ShaderRegister)
                {
                    m_pRefResources.push_back(crBinding.pResource);
                    ReturnIfFalse(crBinding.pResource->QueryInterface(IID_IBuffer, PPV_ARG(&pFoundBuffer)));
                    break;
                }
            }
            m_RootParameterIndexVolatileCBMaps.push_back(std::make_pair(dwIndex, pFoundBuffer));
        }

        if (pDX12BindingLayout->m_dwDescriptorTableSRVetcSize > 0)
        {
            m_dwDescriptorTableSRVetcBaseIndex = m_pDescriptorHeaps->ShaderResourceHeap.AllocateDescriptors(pDX12BindingLayout->m_dwDescriptorTableSRVetcSize);
            m_dwRootParameterSRVetcIndex = pDX12BindingLayout->m_dwRootParameterSRVetcIndex;
            m_bDescriptorTableValidSRVetc = true;

            for (const auto& crRange : pDX12BindingLayout->m_DescriptorSRVetcRanges)
            {
                for (UINT32 ix = 0; ix < crRange.NumDescriptors; ++ix)
                {
                    UINT32 dwSlot = crRange.BaseShaderRegister + ix;
                    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle = m_pDescriptorHeaps->ShaderResourceHeap.GetCpuHandle(
                        m_dwDescriptorTableSRVetcBaseIndex + crRange.OffsetInDescriptorsFromTableStart + ix
                    );

                    BOOL bFound = false;
                    IResource* pResource = nullptr;
                    for (UINT32 jx = 0; jx < m_Desc.BindingItems.Size(); ++jx)
                    {
                        const auto& crBinding = m_Desc.BindingItems[jx];
                        if (crBinding.dwSlot != dwSlot) continue;

                        EResourceType Type = GetNormalizedResourceType(crBinding.Type);
                        if (Type == EResourceType::TypedBuffer_SRV && crRange.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
                        {
                            if (crBinding.pResource)
                            {
                                FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(crBinding.pResource);
                                pDX12Buffer->CreateSRV(DescriptorHandle.ptr, crBinding.Format, crBinding.Range, crBinding.Type);

                                pResource = crBinding.pResource;
                                m_wBindingsWhichNeedTransition.push_back(static_cast<UINT16>(jx));
                            }
                            else 
                            {
                                CreateNullBufferSRV(DescriptorHandle.ptr, crBinding.Format, m_cpContext);
                                LOG_WARN("There is no resource binding to set, it will create a null view.");
                            }
                            bFound = true;
                            break;
                        }
                        else if (Type == EResourceType::TypedBuffer_UAV && crRange.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
                        {
                            if (crBinding.pResource)
                            {
                                FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(crBinding.pResource);
                                pDX12Buffer->CreateUAV(DescriptorHandle.ptr, crBinding.Format, crBinding.Range, crBinding.Type);

                                pResource = crBinding.pResource;
                                m_wBindingsWhichNeedTransition.push_back(static_cast<UINT16>(jx));
                            }
                            else 
                            {
                                CreateNullBufferUAV(DescriptorHandle.ptr, crBinding.Format, m_cpContext);
                                LOG_WARN("There is no resource binding to set, it will create a null view.");
                            }
                            m_bHasUAVBindings = true;
                            bFound = true;
                            break;
                        }
                        else if (Type == EResourceType::Texture_SRV && crRange.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
                        {
                            FDX12Texture* pDX12Texture = CheckedCast<FDX12Texture*>(crBinding.pResource);
                            pDX12Texture->CreateSRV(DescriptorHandle.ptr, crBinding.Format, crBinding.Dimension, crBinding.Subresource);

                            pResource = crBinding.pResource;
                            m_wBindingsWhichNeedTransition.push_back(static_cast<UINT16>(jx));
                            bFound = true;
                            break;
                        }
                        else if (Type == EResourceType::Texture_UAV && crRange.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
                        {
                            FDX12Texture* pDX12Texture = CheckedCast<FDX12Texture*>(crBinding.pResource);
                            pDX12Texture->CreateUAV(DescriptorHandle.ptr, crBinding.Format, crBinding.Dimension, crBinding.Subresource);

                            pResource = crBinding.pResource;
                            m_wBindingsWhichNeedTransition.push_back(static_cast<UINT16>(jx));
                            m_bHasUAVBindings = true;
                            bFound = true;
                            break;
                        }
                        else if (Type == EResourceType::ConstantBuffer && crRange.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
                        {
                            FDX12Buffer* pDX12Buffer = CheckedCast<FDX12Buffer*>(crBinding.pResource);
                            pDX12Buffer->CreateCBV(DescriptorHandle.ptr, crBinding.Range);

                            pResource = crBinding.pResource;

                            if (pDX12Buffer->m_Desc.bIsVolatile)
                            {
                                LOG_ERROR("Attempted to bind a volatile constant buffer to a non-volatile CB layout.");
                                return false;
                            }
                            else 
                            {
                                m_wBindingsWhichNeedTransition.push_back(static_cast<UINT16>(jx));
                            }
                            bFound = true;
                            break;
                        }
                    }

                    if (pResource) m_pRefResources.push_back(pResource);

                    if (!bFound) return false;
                }
            }

            m_pDescriptorHeaps->ShaderResourceHeap.CopyToShaderVisibleHeap(m_dwDescriptorTableSRVetcBaseIndex, pDX12BindingLayout->m_dwDescriptorTableSRVetcSize);
        }

        if (pDX12BindingLayout->m_dwDescriptorTableSamplerSize > 0)
        {
            m_dwDescriptorTableSamplerBaseIndex = m_pDescriptorHeaps->SamplerHeap.AllocateDescriptors(pDX12BindingLayout->m_dwDescriptorTableSamplerSize);
            m_dwRootParameterSamplerIndex = pDX12BindingLayout->m_dwRootParameterSamplerIndex;
            m_bDescriptorTableValidSampler = true;

            for (const auto& crRange : pDX12BindingLayout->m_DescriptorSamplerRanges)
            {
                for (UINT32 ix = 0; ix < crRange.NumDescriptors; ++ix)
                {
                    UINT32 dwSlot = crRange.BaseShaderRegister + ix;
                    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle = m_pDescriptorHeaps->SamplerHeap.GetCpuHandle(
                        m_dwDescriptorTableSamplerBaseIndex + crRange.OffsetInDescriptorsFromTableStart + ix
                    );

                    BOOL bFound = false;

                    for (const auto& crBinding : m_Desc.BindingItems)
                    {
                        if (crBinding.Type == EResourceType::Sampler && crBinding.dwSlot == dwSlot)
                        {
                            FDX12Sampler* pDX12Sampler = CheckedCast<FDX12Sampler*>(crBinding.pResource);
                            pDX12Sampler->CreateDescriptor(DescriptorHandle.ptr);

                            m_pRefResources.push_back(crBinding.pResource);
                            bFound = true;
                            break;
                        }
                    }
                    if (!bFound) return false;
                }
            }

            m_pDescriptorHeaps->SamplerHeap.CopyToShaderVisibleHeap(m_dwDescriptorTableSamplerBaseIndex, pDX12BindingLayout->m_dwDescriptorTableSamplerSize);
        }

        return true;
    }


    FDX12BindingSet::~FDX12BindingSet() noexcept
    {
        FDX12BindingLayout* pDX12BindingLayout = CheckedCast<FDX12BindingLayout*>(m_pBindingLayout.Get());
        m_pDescriptorHeaps->ShaderResourceHeap.ReleaseDescriptors(m_dwDescriptorTableSRVetcBaseIndex, pDX12BindingLayout->m_dwDescriptorTableSRVetcSize);
        m_pDescriptorHeaps->SamplerHeap.ReleaseDescriptors(m_dwDescriptorTableSamplerBaseIndex, pDX12BindingLayout->m_dwDescriptorTableSamplerSize);
    }

    FDX12DescriptorTable::FDX12DescriptorTable(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps) :
        m_cpContext(cpContext), m_pDescriptorHeaps(pDescriptorHeaps)
    {
    }

    BOOL FDX12DescriptorTable::Initialize()
    {
        return true;
    }


    FDX12DescriptorTable::~FDX12DescriptorTable() noexcept
    {
        m_pDescriptorHeaps->ShaderResourceHeap.ReleaseDescriptors(m_dwFirstDescriptorIndex, m_dwCapacity);
    }




}