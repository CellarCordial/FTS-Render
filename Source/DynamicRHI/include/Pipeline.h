#ifndef RHI_PIPELINE_H
#define RHI_PIPELINE_H

#include "Descriptor.h"
#include "Forward.h"
#include "FrameBuffer.h"

namespace FTS
{
    struct FVertexAttributeDesc
    {
        std::string strName;
        EFormat Format = EFormat::UNKNOWN;
        UINT32 dwArraySize = 1;
        UINT32 dwBufferIndex = 0;
        UINT32 dwOffset = 0;
            
        UINT32 dwElementStride = 0;     // 对于大部分 api 来说, 该 stride 应设置为 sizeof(Vertex) 
        BOOL bIsInstanced = false;
    };

    using FVertexAttributeDescArray = TStackArray<FVertexAttributeDesc, gdwMaxVertexAttributes>;


    extern const IID IID_IInputLayout;
    struct IInputLayout : public IUnknown
    {
        virtual UINT32 GetAttributesNum() const = 0;
        virtual FVertexAttributeDesc GetAttributeDesc(UINT32 dwAttributeIndex) const = 0;

		virtual ~IInputLayout() = default;
    };
    

    enum class EBlendFactor : UINT8
    {
        Zero = 1,
        One = 2,
        SrcColor = 3,
        InvSrcColor = 4,
        SrcAlpha = 5,
        InvSrcAlpha = 6,
        DstAlpha  = 7,
        InvDstAlpha = 8,
        DstColor = 9,
        InvDstColor = 10,
        SrcAlphaSaturate = 11,
        ConstantColor = 14,
        InvConstantColor = 15,
        Src1Color = 16,
        InvSrc1Color = 17,
        Src1Alpha = 18,
        InvSrc1Alpha = 19,
    };

    enum class EBlendOP : UINT8
    {
        Add             = 1,
        Subtract        = 2,
        ReverseSubtract = 3,
        Min             = 4,
        Max             = 5
    };

    enum class EColorMask : UINT8
    {
        None    = 0,
        Red     = 1,
        Green   = 2,
        Blue    = 4,
        Alpha   = 8,
        All     = 0xF
    };

    
    


    struct FBlendState
    {
        struct RenderTarget
        {
            BOOL         bEnableBlend = false;
            EBlendFactor SrcBlend     = EBlendFactor::One;
            EBlendFactor DstBlend     = EBlendFactor::Zero;
            EBlendOP     BlendOp      = EBlendOP::Add;
            
            EBlendFactor SrcBlendAlpha = EBlendFactor::One;
            EBlendFactor DstBlendAlpha = EBlendFactor::Zero;
            EBlendOP     BlendOpAlpha  = EBlendOP::Add;
            
            EColorMask ColorWriteMask = EColorMask::All;

            BOOL IfUseConstantColor() const
            {
                return SrcBlend == EBlendFactor::ConstantColor ||
                    SrcBlend == EBlendFactor::InvConstantColor ||
                    DstBlend == EBlendFactor::ConstantColor ||
                    DstBlend == EBlendFactor::InvConstantColor ||
                    SrcBlendAlpha == EBlendFactor::ConstantColor ||
                    SrcBlendAlpha == EBlendFactor::InvConstantColor ||
                    DstBlendAlpha == EBlendFactor::ConstantColor ||
                    DstBlendAlpha == EBlendFactor::InvConstantColor;
            }
        };

        RenderTarget TargetBlends[gdwMaxRenderTargets];
        BOOL bAlphaToCoverageEnable = false;
        
        BOOL IfUseConstantColor(UINT32 dwTargetNum) const
        {
            for (UINT32 ix = 0; ix < dwTargetNum; ++ix)
            {
                if (TargetBlends[ix].IfUseConstantColor()) return true;
            }
            return false;
        }
    };

    enum class ERasterFillMode : UINT8
    {
        Solid,
        Wireframe,
    };

    enum class ERasterCullMode : UINT8
    {
        Back,
        Front,
        None
    };
    
    struct FRasterState
    {
        ERasterFillMode FillMode = ERasterFillMode::Solid;
        ERasterCullMode CullMode = ERasterCullMode::Back;
        BOOL bFrontCounterClockWise = false;

        BOOL bDepthClipEnable = true;
        BOOL bScissorEnable = false;
        BOOL bMultisampleEnable = false;
        BOOL bAntiAliasedLineEnable = false;
        INT32 dwDepthBias = 0;
        FLOAT fDepthBiasClamp = 0.0f;
        FLOAT fSlopeScaledDepthBias = 0.0f;

        UINT8 btForcedSampleCount = 0;
        BOOL bConservativeRasterEnable = false;
    };


    enum class EStencilOP : UINT8
    {
        Keep                = 1,
        Zero                = 2,
        Replace             = 3,
        IncrementAndClamp   = 4,
        DecrementAndClamp   = 5,
        Invert              = 6,
        IncrementAndWrap    = 7,
        DecrementAndWrap    = 8
    };

    enum class EComparisonFunc : UINT8
    {
        Never           = 1,
        Less            = 2,
        Equal           = 3,
        LessOrEqual     = 4,
        Greater         = 5,
        NotEqual        = 6,
        GreaterOrEqual  = 7,
        Always          = 8
    };

    struct FStencilOpDesc
    {
        EStencilOP FailOp = EStencilOP::Keep;
        EStencilOP DepthFailOp = EStencilOP::Keep;
        EStencilOP PassOp = EStencilOP::Keep;
        EComparisonFunc StencilFunc = EComparisonFunc::Always;
    };

    struct FDepthStencilState
    {
        BOOL            bDepthTestEnable = false;
        BOOL            bDepthWriteEnable = true;
        EComparisonFunc DepthFunc = EComparisonFunc::Less;
        
        BOOL            bStencilEnable = false;
        UINT8           btStencilReadMask = 0xff;
        UINT8           btStencilWriteMask = 0xff;
        UINT8           btStencilRefValue = 0;
        BOOL            bDynamicStencilRef = false;
        FStencilOpDesc   FrontFaceStencil;
        FStencilOpDesc   BackFaceStencil;
    };

    using FViewportArray = TStackArray<FViewport, gdwMaxViewports>;
    using FRectArray = TStackArray<FRect, gdwMaxViewports>;

    struct FViewportState
    {
        FViewportArray Viewports;
        FRectArray Rects;


        static FViewportState CreateSingleViewport(UINT32 dwWidth, UINT32 dwHeight)
        {
            FViewportState Ret;
            Ret.Viewports.PushBack(FViewport{ 0.0f, static_cast<FLOAT>(dwWidth), 0.0f, static_cast<FLOAT>(dwHeight), 0.0f, 1.0f });
            Ret.Rects.PushBack(FRect{ 0, dwWidth, 0, dwHeight });
            return Ret;
        }
    };


    enum class EPrimitiveType : UINT8
    {
        PointList,
        LineList,
        TriangleList,
        TriangleStrip,
        TriangleListWithAdjacency,
        TriangleStripWithAdjacency,
        PatchList
    };


    struct FRenderState
    {
        FBlendState BlendState;
        FDepthStencilState DepthStencilState;
        FRasterState RasterizerState;
    };

    using FPipelineBindingLayoutArray = TStackArray<IBindingLayout*, gdwMaxBindingLayouts>;
    
    struct FGraphicsPipelineDesc
    {
        EPrimitiveType PrimitiveType = EPrimitiveType::TriangleList;
        UINT32 dwPatchControlPoints = 0;

        IShader* VS = nullptr;
        IShader* HS = nullptr;
        IShader* DS = nullptr;
        IShader* GS = nullptr;
        IShader* PS = nullptr;

        FRenderState RenderState;

        IInputLayout* pInputLayout = nullptr;
        FPipelineBindingLayoutArray pBindingLayouts;
    };


    extern const IID IID_IGraphicsPipeline;

    struct IGraphicsPipeline : public IResource
    {
        virtual FGraphicsPipelineDesc GetDesc() const = 0;
        virtual FFrameBufferInfo GetFrameBufferInfo() const = 0;

		virtual ~IGraphicsPipeline() = default;
    };


    struct FComputePipelineDesc
    {
        IShader* CS = nullptr;
        FPipelineBindingLayoutArray pBindingLayouts;
    };


    extern const IID IID_IComputePipeline;

    struct IComputePipeline : public IResource
    {
        virtual FComputePipelineDesc GetDesc() const = 0;
        
		virtual ~IComputePipeline() = default;
    };
    

    FBlendState::RenderTarget CreateRenderTargetBlend(EBlendFactor SrcBlend, EBlendFactor DstBlend);
}
    




















#endif