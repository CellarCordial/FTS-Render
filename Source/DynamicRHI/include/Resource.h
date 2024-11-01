#ifndef RHI_RESOURCE_H
#define RHI_RESOURCE_H

#include "Format.h"
#include "Forward.h"
#include <minwindef.h>
#include <string>

#include "../../Math/include/Common.h"

namespace FTS
{
    extern const IID IID_IResource;

    struct IResource : public IUnknown
    {
		virtual ~IResource() = default;
    };
    
    enum class ETextureDimension : UINT8
    {
        Unknown,
        Texture1D,
        Texture1DArray,
        Texture2D,
        Texture2DArray,
        TextureCube,
        TextureCubeArray,
        Texture2DMS,
        Texture2DMSArray,
        Texture3D
    };

    enum class ECpuAccessMode : UINT8
    {
        None,
        Read,
        Write
    };
    
    enum class EResourceStates : UINT32
    {
        Common                  = 0,
        ConstantBuffer          = 0x00000001,
        VertexBuffer            = 0x00000002,
        IndexBuffer             = 0x00000004,
        PixelShaderResource     = 0x00000008,
        NonPixelShaderResource  = 0x00000020,
        UnorderedAccess         = 0x00000040,
        RenderTarget            = 0x00000080,
        DepthWrite              = 0x00000100,
        DepthRead               = 0x00000200,
        StreamOut               = 0x00000400,
        CopyDest                = 0x00000800,
        CopySource              = 0x00001000,
        ResolveDest             = 0x00002000,
        ResolveSource           = 0x00004000,
        Present                 = 0x00008000
    };
    FTS_ENUM_CLASS_FLAG_OPERATORS(EResourceStates)


    // 默认 Texture 为着色器资源
    struct FTextureDesc
    {
        std::string strName;

        UINT32 dwWidth = 1;
        UINT32 dwHeight = 1;
        UINT32 dwDepth = 1;
        UINT32 dwArraySize = 1;
        UINT32 dwMipLevels = 1;

        UINT32 dwSampleCount = 1;
        UINT32 dwSampleQuality = 0;
        
        EFormat Format = EFormat::UNKNOWN;
        ETextureDimension Dimension = ETextureDimension::Texture2D;
        
        BOOL bIsShaderResource = true;
        BOOL bIsRenderTarget = false;
        BOOL bIsDepthStencil = false;
        BOOL bIsUAV = false;
        BOOL bIsTypeless = false;

        // 设置为 true 时，表示纹理被创建时没有实际的内存支持，而是在后续使用 bindTextureMemory 方法将内存绑定到纹理上 (Vulkan)
        // 在 DirectX 12 中，纹理资源是在绑定内存时创建的，而不是在纹理创建时就分配内存
        BOOL bIsVirtual = false;

        BOOL bUseClearValue = false;
        FColor ClearValue;

        EResourceStates InitialState = EResourceStates::Common;


        static FTextureDesc CreateRenderTarget(UINT32 dwWidth, UINT32 dwHeight, EFormat Format, std::string strDebugName = "")
        {
            FTextureDesc Ret;
            Ret.bIsRenderTarget = true;
            Ret.InitialState = EResourceStates::RenderTarget;
            Ret.ClearValue = FColor{ 0.0f, 0.0f, 0.0f, 0.0f };
            Ret.bUseClearValue = true;
            Ret.dwWidth = dwWidth;
            Ret.dwHeight = dwHeight;
            Ret.Format = Format;
            Ret.strName = strDebugName;
            return Ret;
        }

        static FTextureDesc CreateDepth(UINT32 dwWidth, UINT32 dwHeight, EFormat Format, std::string strDebugName = "")
        {
            FTextureDesc Ret;
            Ret.bIsDepthStencil = true;
            Ret.InitialState = EResourceStates::DepthWrite;
            Ret.bUseClearValue = true;
            Ret.ClearValue = FColor{ 1.0f, 0.0f, 0.0f, 0.0f };
            Ret.dwWidth = dwWidth;
            Ret.dwHeight = dwHeight;
            Ret.Format = Format;
            Ret.strName = strDebugName;
            return Ret;
        }

        static FTextureDesc CreateShaderResource(
            UINT32 dwWidth, UINT32 dwHeight, EFormat Format, BOOL bComputePass = false, std::string strDebugName = ""
        )
        {
            FTextureDesc Ret;
            Ret.bIsShaderResource = true;
            Ret.dwWidth = dwWidth;
            Ret.dwHeight = dwHeight;
            Ret.Format = Format;
			Ret.InitialState = bComputePass ? EResourceStates::NonPixelShaderResource : EResourceStates::PixelShaderResource;
            Ret.strName = strDebugName;
            return Ret;
        }

        static FTextureDesc CreateShaderResource(
            UINT32 dwWidth, UINT32 dwHeight, UINT32 dwDepth, EFormat Format, BOOL bComputePass = false, std::string strDebugName = ""
        )
        {
            FTextureDesc Ret;
            Ret.bIsShaderResource = true;
			Ret.dwWidth = dwWidth;
            Ret.dwHeight = dwHeight;
            Ret.dwDepth = dwDepth;
            Ret.Dimension = ETextureDimension::Texture3D;
            Ret.Format = Format;
            Ret.InitialState = bComputePass ? EResourceStates::NonPixelShaderResource : EResourceStates::PixelShaderResource;
            Ret.strName = strDebugName;
            return Ret;
        }

        static FTextureDesc CreateReadWrite(UINT32 dwWidth, UINT32 dwHeight, EFormat Format, std::string strDebugName = "")
        {
            FTextureDesc Ret;
            Ret.dwWidth = dwWidth;
            Ret.dwHeight = dwHeight;
            Ret.Format = Format;
            Ret.InitialState = EResourceStates::UnorderedAccess;
            Ret.bIsUAV = true;
            Ret.strName = strDebugName;
            return Ret;
        }

		static FTextureDesc CreateReadWrite(UINT32 dwWidth, UINT32 dwHeight, UINT32 dwDepth, EFormat Format, std::string strDebugName = "")
		{
			FTextureDesc Ret;
			Ret.dwWidth = dwWidth;
			Ret.dwHeight = dwHeight;
            Ret.dwDepth = dwDepth;
			Ret.Format = Format;
            Ret.Dimension = ETextureDimension::Texture3D;
			Ret.InitialState = EResourceStates::UnorderedAccess;
			Ret.bIsUAV = true;
			Ret.strName = strDebugName;
			return Ret;
		}

        static FTextureDesc CreateReadBack(UINT32 dwWidth, UINT32 dwHeight, EFormat Format, std::string strDebugName = "")
        {
            FTextureDesc Ret;
            Ret.dwWidth = dwWidth;
            Ret.dwHeight = dwHeight;
            Ret.Format = Format;
            Ret.InitialState = EResourceStates::CopyDest;
            Ret.strName = strDebugName;
            return Ret;
        }
    };
    
    struct FTextureSlice
    {
        // 纹理切片的起始坐标
        UINT32 x = 0;
        UINT32 y = 0;
        UINT32 z = 0;
        
        // 当值为 -1 时，表示该维度（分量）的整个尺寸都是切片的一部分
        // 使用 Resolve 方法，会根据给定的纹理描述 crDesc 来确定实际的尺寸
        UINT32 dwWidth = static_cast<UINT32>(-1);
        UINT32 dwHeight = static_cast<UINT32>(-1);
        UINT32 dwDepth = static_cast<UINT32>(-1);

        UINT32 dwMipLevel = 0;
        UINT32 dwArraySlice = 0;

        FTextureSlice Resolve(const FTextureDesc& crDesc) const;
    };
    
    struct FTextureSubresourceSet
    {
        UINT32 dwBaseMipLevelIndex = 0;
        UINT32 dwMipLevelsNum = 1;
        UINT32 dwBaseArraySliceIndex = 0;
        UINT32 dwArraySlicesNum = 1;

        BOOL IsEntireTexture(const FTextureDesc& crDesc) const;
        FTextureSubresourceSet Resolve(const FTextureDesc& crDesc, BOOL bIsSingleMipLevel) const;
    };



    inline const FTextureSubresourceSet gAllSubresource = FTextureSubresourceSet{
        .dwBaseMipLevelIndex    = 0,
        .dwMipLevelsNum         = ~0u,  // AllMipLevels
        .dwBaseArraySliceIndex  = 0,
        .dwArraySlicesNum       = ~0u   // AllArraySlices
    };


    extern const IID IID_ITexture;

    struct ITexture : public IResource
    {
        virtual FTextureDesc GetDesc() const = 0;
        virtual SIZE_T GetNativeView(
            EViewType ViewType,
            EFormat Format,
            FTextureSubresourceSet Subresource,
            ETextureDimension Dimension,
            BOOL bIsReadOnlyDSV
        ) = 0;

        virtual BOOL BindMemory(IHeap* pHeap, UINT64 stOffset) = 0;
        virtual FMemoryRequirements GetMemoryRequirements() = 0;
        
		virtual ~ITexture() = default;
    };

    
    extern const IID IID_IStagingTexture;

    struct IStagingTexture : public IResource
    {
        virtual FTextureDesc GetDesc() const = 0;
        virtual void* Map(const FTextureSlice& crTextureSlice, ECpuAccessMode CpuAccessMode, HANDLE FenceEvent, UINT64* pstRowPitch) = 0;
        virtual void Unmap() = 0;

		virtual ~IStagingTexture() = default;
    };



    struct FBufferDesc
    {
        std::string strName;

        UINT64 stByteSize = 0;
        UINT32 dwStructStride = 0; // 若不为 0, 则表示该 Buffer 为 Structured Buffer
        
        EFormat Format = EFormat::UNKNOWN; // 供 Typed Buffer 使用 
        BOOL bCanHaveUAVs = false;
        BOOL bIsVertexBuffer = false;
        BOOL bIsIndexBuffer = false;
        BOOL bIsConstantBuffer = false;
        BOOL bIsShaderBindingTable = false;

        // dynamic/upload 缓冲区，其内容仅存在于当前命令列表中
        BOOL bIsVolatile = false;

        BOOL bIsVirtual = false;
        
        EResourceStates InitialState = EResourceStates::Common;

        ECpuAccessMode CpuAccess = ECpuAccessMode::None;

        UINT32 dwMaxVersions = 0; // 仅在 Vulkan 的 volatile 缓冲区中有效，表示最大版本数，必须为非零值


        static FBufferDesc CreateConstant(UINT64 stByteSize, BOOL bIsVolatial = true, std::string strDebugName = "")
        {
            FBufferDesc Ret;
            Ret.bIsVolatile = bIsVolatial;
            Ret.bIsConstantBuffer = true;
            Ret.stByteSize = stByteSize;
            Ret.InitialState = EResourceStates::ConstantBuffer;
            Ret.strName = strDebugName;
            return Ret;
        }

        static FBufferDesc CreateVertex(UINT64 stByteSize, std::string strDebugName = "")
        {
            FBufferDesc Ret;
            Ret.bIsVertexBuffer = true;
            Ret.stByteSize = stByteSize;
            Ret.InitialState = EResourceStates::VertexBuffer;
            Ret.strName = strDebugName;
            return Ret;
        }

        static FBufferDesc CreateIndex(UINT64 stByteSize, std::string strDebugName = "")
        {
            FBufferDesc Ret;
            Ret.bIsIndexBuffer = true;
            Ret.stByteSize = stByteSize;
            Ret.InitialState = EResourceStates::IndexBuffer;
            Ret.strName = strDebugName;
            return Ret;
        }

        static FBufferDesc CreateStructured(UINT64 stByteSize, UINT32 dwStride, BOOL bComputePass = false, std::string strDebugName = "")
        {
            FBufferDesc Ret;
			Ret.InitialState = bComputePass ? EResourceStates::NonPixelShaderResource : EResourceStates::PixelShaderResource;
			Ret.stByteSize = stByteSize;
            Ret.dwStructStride = dwStride;
            Ret.strName = strDebugName;
            return Ret;
        }

        static FBufferDesc CreateRWStructured(UINT64 stByteSize, UINT32 dwStride, std::string strDebugName = "")
        {
            FBufferDesc Ret;
            Ret.InitialState = EResourceStates::UnorderedAccess;
            Ret.bCanHaveUAVs = true;
            Ret.stByteSize = stByteSize;
            Ret.dwStructStride = dwStride;
            Ret.strName = strDebugName;
            return Ret;
        }

        static FBufferDesc CreateReadBack(UINT64 stByteSize, std::string strDebugName = "")
        {
            FBufferDesc Ret;
            Ret.InitialState = EResourceStates::CopyDest;
            Ret.CpuAccess = ECpuAccessMode::Read;
            Ret.stByteSize = stByteSize;
            Ret.strName = strDebugName;
            return Ret;
        }
    };

    struct FBufferRange
    {
        UINT64 stByteOffset = 0;
        UINT64 stByteSize = 0;

        FBufferRange Resolve(const FBufferDesc& crDesc) const;
        BOOL IsEntireBuffer(const FBufferDesc& crDesc) const;
    };
    
    
    inline const FBufferRange gEntireBufferRange = FBufferRange{ 0, ~0ull };


    extern const IID IID_IBuffer;

    struct IBuffer : public IResource
    {
        virtual FBufferDesc GetDesc() const = 0;
        virtual void* Map(ECpuAccessMode CpuAccess, HANDLE FenceEvent) = 0;
        virtual void Unmap() = 0;
        virtual FMemoryRequirements GetMemoryRequirements() = 0;
        virtual BOOL BindMemory(IHeap* pHeap, UINT64 stOffset) = 0;


		virtual ~IBuffer() = default;
    };


    enum class ESamplerAddressMode : UINT8
    {
        Clamp       = 1,
        Wrap        = 2,
        Border      = 3,
        Mirror      = 4,
        MirrorOnce  = 5,
    };

    enum class ESamplerReductionType : UINT8
    {
        Standard    = 1,
        Comparison  = 2,
        Minimum     = 3,
        Maximum     = 4
    };

    struct FSamplerDesc
    {
        std::string strName;

        FColor BorderColor;
        FLOAT fMaxAnisotropy = 1.0f;
        FLOAT fMipBias = 0.0f;

        BOOL bMinFilter = true;
        BOOL bMaxFilter = true;
        BOOL bMipFilter = true;
        ESamplerAddressMode AddressU = ESamplerAddressMode::Wrap;
        ESamplerAddressMode AddressV = ESamplerAddressMode::Wrap;
        ESamplerAddressMode AddressW = ESamplerAddressMode::Wrap;
        ESamplerReductionType ReductionType = ESamplerReductionType::Standard;

        void SetAddressMode(ESamplerAddressMode Mode)
        {
			AddressU = Mode;
			AddressV = Mode;
            AddressW = Mode;
		}
        
        void SetFilter(BOOL bMinMaxMip)
        {
			bMinFilter = bMinMaxMip;
			bMaxFilter = bMinMaxMip;
			bMipFilter = bMinMaxMip;
        }
    };



    extern const IID IID_ISampler;

    struct ISampler : public IResource
    {
        virtual FSamplerDesc GetDesc() const = 0;
        
		virtual ~ISampler() = default;
    };


    inline FBufferDesc CreateStaticBufferDesc(UINT32 dwByteSize)
    {
        FBufferDesc BufferDesc{};
        BufferDesc.stByteSize = dwByteSize;
        BufferDesc.bIsConstantBuffer = true;
        BufferDesc.bIsVolatile = false;
        return BufferDesc;
    }


    inline FBufferDesc CreateVolatileBufferDesc(UINT32 dwByteSize, UINT32 dwMaxVersion = 0)
    {
        FBufferDesc BufferDesc{};
        BufferDesc.stByteSize = dwByteSize;
        BufferDesc.bIsConstantBuffer = true;
        BufferDesc.bIsVolatile = true;
        BufferDesc.dwMaxVersions = dwMaxVersion;
        return BufferDesc;
    }


    extern const IID IID_IBufferStateTrack;

    struct IBufferStateTrack : public IUnknown
    {
        virtual FBufferDesc GetDesc() const = 0;
		virtual ~IBufferStateTrack() = default;
    };
    
    extern const IID IID_ITextureStateTrack;

    struct ITextureStateTrack : public IUnknown
    {   
        virtual FTextureDesc GetDesc() const = 0;
		virtual ~ITextureStateTrack() = default;
    };
}




















#endif