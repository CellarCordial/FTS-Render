#ifndef RHI_DESCRIPTOR_H
#define RHI_DESCRIPTOR_H

#include "Format.h"
#include "Forward.h"
#include "Resource.h"
#include "Shader.h"
#include "../../Tools/include/StackArray.h"


namespace FTS
{
    enum class EResourceType : UINT8
    {
        None,

        Texture_SRV,
        Texture_UAV,
        TypedBuffer_SRV,
        TypedBuffer_UAV,
        StructuredBuffer_SRV,
        StructuredBuffer_UAV,
        RawBuffer_SRV,
        RawBuffer_UAV,
        ConstantBuffer,         // static
        VolatileConstantBuffer, // volatile

        Sampler,
        PushConstants,

        Count
    };

    // 确保 BindingLayoutItem 大小为 8 字节
    struct FBindingLayoutItem
    {
        union
        {
            UINT32 dwSlot; 
            UINT32 dwRegisterSpace;     /**< For bindless layout. */
        };

        EResourceType Type  : 8;
        UINT8 btPad         : 8;
        UINT16 wSize        : 16;   /**< For push constants. */


        static FBindingLayoutItem CreateTexture_SRV(UINT32 dwSlot)
        {
            FBindingLayoutItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::Texture_SRV;
            return Ret;
        }

        static FBindingLayoutItem CreateTexture_UAV(UINT32 dwSlot)
        {
            FBindingLayoutItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::Texture_UAV;
            return Ret;
        }

        static FBindingLayoutItem CreateTypedBuffer_SRV(UINT32 dwSlot)
        {
            FBindingLayoutItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::TypedBuffer_SRV;
            return Ret;
        }

        static FBindingLayoutItem CreateTypedBuffer_UAV(UINT32 dwSlot)
        {
            FBindingLayoutItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::TypedBuffer_UAV;
            return Ret;
        }

        static FBindingLayoutItem CreateConstantBuffer(UINT32 dwSlot, BOOL bIsVolatile = true)
        {
            FBindingLayoutItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = bIsVolatile ? EResourceType::VolatileConstantBuffer : EResourceType::ConstantBuffer;
            return Ret;
        }

        static FBindingLayoutItem CreateSampler(UINT32 dwSlot)
        {
            FBindingLayoutItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::Sampler;
            return Ret;
        }

        static FBindingLayoutItem CreateStructuredBuffer_SRV(UINT32 dwSlot)
        {
            FBindingLayoutItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::StructuredBuffer_SRV;
            return Ret;
        }

        static FBindingLayoutItem CreateStructuredBuffer_UAV(UINT32 dwSlot)
        {
            FBindingLayoutItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::StructuredBuffer_UAV;
            return Ret;
        }

        static FBindingLayoutItem CreateRawBuffer_SRV(UINT32 dwSlot)
        {
            FBindingLayoutItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::RawBuffer_SRV;
            return Ret;
        }

        static FBindingLayoutItem CreateRawBuffer_UAV(UINT32 dwSlot)
        {
            FBindingLayoutItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::RawBuffer_UAV;
            return Ret;
        }

        static FBindingLayoutItem CreatePushConstants(UINT32 dwSlot, UINT16 dwByteSize)
        {
            FBindingLayoutItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::PushConstants;
            Ret.wSize = dwByteSize;
            return Ret;
        }
    };

    using FBindingLayoutItemArray = TStackArray<FBindingLayoutItem, gdwMaxBindingsPerLayout>;

    struct FBindingLayoutDesc
    {
        EShaderType ShaderVisibility = EShaderType::All;
        UINT32 dwRegisterSpace = 0;

        // 相同类型的 item 需放在一块
        FBindingLayoutItemArray BindingLayoutItems;
    };

    struct FBindlessLayoutDesc
    {
        EShaderType ShaderVisibility = EShaderType::All;
		UINT32 dwFirstSlot = 0;

        // 不允许 PushConstants 和 VolatileConstantBuffer
        FBindingLayoutItemArray BindingLayoutItems;
    };

    extern const IID IID_IBindingLayout;

    struct IBindingLayout : public IUnknown
    {
        virtual FBindingLayoutDesc GetBindingDesc() const = 0;
        virtual FBindlessLayoutDesc GetBindlessDesc() const = 0;
        virtual BOOL IsBindingless() const = 0;

		virtual ~IBindingLayout() = default;
    };



    // 确保其为 32 字节
    struct FBindingSetItem
    {
        IResource* pResource = nullptr;

        UINT32 dwSlot = 0u;

        EResourceType Type          : 8 = EResourceType::None;
        ETextureDimension Dimension : 8 = ETextureDimension::Unknown; // 供 Texture_SRV, Texture_UAV 使用
        EFormat Format              : 8 = EFormat::UNKNOWN; // 供 Texture_SRV, Texture_UAV, Buffer_SRV, Buffer_UAV 使用
        UINT8 btPad                 : 8 = 0;

        union 
        {
            FTextureSubresourceSet Subresource; // 供 valid for Texture_SRV, Texture_UAV 使用，且必须得为 16 字节
            FBufferRange Range;                 // 供 Buffer_SRV, Buffer_UAV, ConstantBuffer 使用，且必须得为 16 字节
            UINT64 stPads[2] = { 0, 0 };
        };


        static FBindingSetItem CreateTexture_SRV(
            UINT32 dwSlot,
            ITexture* pTexture,
            ETextureDimension Dimension = ETextureDimension::Texture2D,
            FTextureSubresourceSet Subresource = gAllSubresource
        )
        {
            assert(pTexture != nullptr);
            FTextureDesc Desc = pTexture->GetDesc();

            FBindingSetItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::Texture_SRV;
            Ret.pResource = pTexture;
            Ret.Format = Desc.Format;
            Ret.Dimension = Desc.Dimension;
            Ret.Subresource = Subresource;
            Ret.btPad = 0;
            return Ret;
        }

        static FBindingSetItem CreateTexture_UAV(
            UINT32 dwSlot,
            ITexture* pTexture,
            FTextureSubresourceSet Subresource = FTextureSubresourceSet{}
        )
        {
            assert(pTexture != nullptr);
			FTextureDesc Desc = pTexture->GetDesc();

			FBindingSetItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::Texture_UAV;
            Ret.pResource = pTexture;
            Ret.Format = Desc.Format;
            Ret.Dimension = Desc.Dimension;
            Ret.Subresource = Subresource;
            Ret.btPad = 0;
            return Ret;
        }

        static FBindingSetItem CreateTypedBuffer_SRV(
            UINT32 dwSlot,
            IBuffer* pBuffer
        )
        {
			assert(pBuffer != nullptr);
			FBufferDesc Desc = pBuffer->GetDesc();

            FBindingSetItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::TypedBuffer_SRV;
            Ret.pResource = pBuffer;
            Ret.Format = Desc.Format;
            Ret.Dimension = ETextureDimension::Unknown;
            Ret.Range = FBufferRange{ .stByteSize = Desc.stByteSize };
            Ret.btPad = 0;
            return Ret;
        }

        static FBindingSetItem CreateTypedBuffer_UAV(
            UINT32 dwSlot,
            IBuffer* pBuffer
        )
        {
            assert(pBuffer != nullptr);
			FBufferDesc Desc = pBuffer->GetDesc();

			FBindingSetItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::TypedBuffer_UAV;
            Ret.pResource = pBuffer;
            Ret.Format = Desc.Format;
            Ret.Dimension = ETextureDimension::Unknown;
            Ret.Range = FBufferRange{ .stByteSize = Desc.stByteSize };
            Ret.btPad = 0;
            return Ret;
        }

        static FBindingSetItem CreateConstantBuffer(
            UINT32 dwSlot,
            IBuffer* pBuffer
        )
        {
            assert(pBuffer != nullptr);

            FBufferDesc Desc = pBuffer->GetDesc();

            const BOOL bIsVolatile = pBuffer && Desc.bIsVolatile;

            FBindingSetItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = bIsVolatile ? EResourceType::VolatileConstantBuffer : EResourceType::ConstantBuffer;
            Ret.pResource = pBuffer;
            Ret.Format = EFormat::UNKNOWN;
            Ret.Dimension = ETextureDimension::Unknown;
            Ret.Range = FBufferRange{ .stByteSize = Desc.stByteSize };
            Ret.btPad = 0;
            return Ret;
        }

        static FBindingSetItem CreateSampler(
            UINT32 dwSlot,
            ISampler* pSampler
        )
        {
            assert(pSampler != nullptr);
            FBindingSetItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::Sampler;
            Ret.pResource = pSampler;
            Ret.Format = EFormat::UNKNOWN;
            Ret.Dimension = ETextureDimension::Unknown;
            Ret.stPads[0] = 0;
            Ret.stPads[1] = 0;
            Ret.btPad = 0;
            return Ret;
        }

        static FBindingSetItem CreateStructuredBuffer_SRV(
            UINT32 dwSlot,
            IBuffer* pBuffer
        )
        {
            assert(pBuffer != nullptr);
            FBindingSetItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::StructuredBuffer_SRV;
            Ret.pResource = pBuffer;
            Ret.Format = EFormat::UNKNOWN;
            Ret.Dimension = ETextureDimension::Unknown;
            Ret.Range = FBufferRange{ .stByteSize = pBuffer->GetDesc().stByteSize };
            Ret.btPad = 0;
            return Ret;
        }

        static FBindingSetItem CreateStructuredBuffer_UAV(
            UINT32 dwSlot,
            IBuffer* pBuffer
        )
        {
            assert(pBuffer != nullptr);
            FBindingSetItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::StructuredBuffer_UAV;
            Ret.pResource = pBuffer;
            Ret.Format = EFormat::UNKNOWN;
            Ret.Dimension = ETextureDimension::Unknown;
            Ret.Range = FBufferRange{ .stByteSize = pBuffer->GetDesc().stByteSize };
            Ret.btPad = 0;
            return Ret;
        }

        static FBindingSetItem CreateRawBuffer_SRV(
            UINT32 dwSlot,
            IBuffer* pBuffer
        )
        {
            assert(pBuffer != nullptr);
            FBindingSetItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::RawBuffer_SRV;
            Ret.pResource = pBuffer;
            Ret.Format = EFormat::UNKNOWN;
            Ret.Dimension = ETextureDimension::Unknown;
            Ret.Range = FBufferRange{ .stByteSize = pBuffer->GetDesc().stByteSize };
            Ret.btPad = 0;
            return Ret;
        }

        static FBindingSetItem CreateRawBuffer_UAV(
            UINT32 dwSlot,
            IBuffer* pBuffer
        )
        {
            assert(pBuffer != nullptr);
            FBindingSetItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::RawBuffer_UAV;
            Ret.pResource = pBuffer;
            Ret.Format = EFormat::UNKNOWN;
            Ret.Dimension = ETextureDimension::Unknown;
            Ret.Range = FBufferRange{ .stByteSize = pBuffer->GetDesc().stByteSize };
            Ret.btPad = 0;
            return Ret;
        }

        static FBindingSetItem CreatePushConstants(
            UINT32 dwSlot,
            UINT32 dwByteSize
        )
        {
            FBindingSetItem Ret;
            Ret.dwSlot = dwSlot;
            Ret.Type = EResourceType::PushConstants;
            Ret.pResource = nullptr;
            Ret.Format = EFormat::UNKNOWN;
            Ret.Dimension = ETextureDimension::Unknown;
            Ret.Range.stByteOffset = 0;
            Ret.Range.stByteSize = dwByteSize;
            Ret.btPad = 0;
            return Ret;
        }

    };

    using FBindingSetItemArray = TStackArray<FBindingSetItem, gdwMaxBindingsPerLayout>;


    struct FBindingSetDesc
    {
        FBindingSetItemArray BindingItems;
       
        // Enables automatic liveness tracking of this binding set by nvrhi command lists.
        // By setting trackLiveness to false, you take the responsibility of not releasing it 
        // until all rendering commands using the binding set are finished.
        BOOL bTrackLiveness = true;
    };


    extern const IID IID_IBindingSet;

    struct IBindingSet : public IResource
    {
        virtual BOOL IsBindless() const = 0;
        virtual FBindingSetDesc GetDesc() const = 0;
        virtual IBindingLayout* GetLayout() const = 0;

		virtual ~IBindingSet() = default;
    };


    extern const IID IID_IDescriptorTable;

    struct IBindlessSet : public IBindingSet
    {
        virtual UINT32 GetCapacity() const = 0;
		virtual void Resize(UINT32 dwNewSize, BOOL bKeepContents) = 0;
		virtual BOOL SetSlot(const FBindingSetItem& crItem, UINT32 dwSlot) = 0;

		virtual ~IBindlessSet() = default;
    };
    
}







#endif