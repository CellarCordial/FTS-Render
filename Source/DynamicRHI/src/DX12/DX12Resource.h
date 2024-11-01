#ifndef RHI_DX12_RESOURCE_H
#define RHI_DX12_RESOURCE_H

#include "../../include/DynamicRHI.h"
#include "../../include/Descriptor.h"
#include "../../../Core/include/ComRoot.h"
#include "DX12Descriptor.h"
#include "DX12Forward.h"
#include <functional>
#include <unordered_map>

namespace FTS
{
    struct FDX12TextureBindingKey : public FTextureSubresourceSet
    {
        EFormat Format;
        BOOL bIsReadOnlyDSV;

        FDX12TextureBindingKey(const FTextureSubresourceSet& crSubresourceSet, EFormat Format, BOOL bIsReadOnlyDSV = false) :
            FTextureSubresourceSet{ crSubresourceSet }, 
            Format(Format), 
            bIsReadOnlyDSV(bIsReadOnlyDSV)
        {
        }

        BOOL operator==(const FDX12TextureBindingKey& crOther) const
        {
            return  dwBaseMipLevelIndex == crOther.dwBaseMipLevelIndex &&
                    dwMipLevelsNum == crOther.dwMipLevelsNum &&
                    dwBaseArraySliceIndex == crOther.dwBaseArraySliceIndex &&
                    dwArraySlicesNum == crOther.dwArraySlicesNum &&
                    Format == crOther.Format &&
                    bIsReadOnlyDSV == crOther.bIsReadOnlyDSV;
        }
    };

    template <typename T>
    using FTextureBindingKeyHashMap = std::unordered_map<FDX12TextureBindingKey, T>;
}

namespace std 
{
    template<>
    struct hash<FTS::FDX12TextureBindingKey>
    {
        std::size_t operator()(const FTS::FDX12TextureBindingKey& s) const noexcept
        {
            return std::hash<FTS::UINT32>()(s.dwArraySlicesNum) ^
                   std::hash<FTS::UINT32>()(s.dwBaseArraySliceIndex) ^
                   std::hash<FTS::UINT32>()(s.dwBaseMipLevelIndex) ^
                   std::hash<FTS::UINT32>()(s.dwMipLevelsNum) ^
                   std::hash<FTS::BOOL>()(s.bIsReadOnlyDSV) ^
                   std::hash<FTS::EFormat>()(s.Format);
        }
    };
}

namespace FTS
{
    // Foward shims. 
    // It doesn't allow FDX12Texture has GetDesc(FTextureDesc* pDesc) const Function. 
    template <typename Deriving>
    struct NO_VTABLE _ITexture : public ITexture
    {
        FTextureDesc GetDesc() const override
        {
            return static_cast<const Deriving*>(this)->TextureGetDesc();
        }
    };

    template <typename Deriving>
    struct NO_VTABLE _ITextureStateTrack : public ITextureStateTrack
    {
        FTextureDesc GetDesc() const override
        {
            return static_cast<const Deriving*>(this)->TextureStateTrackGetDesc();
        }
    };


    class FDX12Texture :
        public TComObjectRoot<FComMultiThreadModel>,
        public _ITexture<FDX12Texture>,
        public _ITextureStateTrack<FDX12Texture>
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12Texture)
            INTERFACE_ENTRY(IID_ITexture, ITexture)
            INTERFACE_ENTRY(IID_ITextureStateTrack, ITextureStateTrack)
        END_INTERFACE_MAP

        FDX12Texture(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps, const FTextureDesc& crDesc);
        ~FDX12Texture() noexcept;

        BOOL Initialize();

        // ITexture
        SIZE_T GetNativeView(
            EViewType ViewType,
            EFormat Format,
            FTextureSubresourceSet Subresource,
            ETextureDimension Dimension,
            BOOL bIsReadOnlyDSV
        ) override;
        BOOL BindMemory(IHeap* pHeap, UINT64 stOffset) override;
        FMemoryRequirements GetMemoryRequirements() override;
        FTextureDesc TextureGetDesc() const { return m_Desc; }

		// ITextureStateTrack
		FTextureDesc TextureStateTrackGetDesc() const { return m_Desc; }

        UINT32 GetClearMipLevelUAVIndex(UINT32 dwMipLevel);
        void CreateSRV(UINT64 stDescriptorAddress, EFormat Format, ETextureDimension Dimension, const FTextureSubresourceSet& crSubresourceSet);
        void CreateUAV(UINT64 stDescriptorAddress, EFormat Format, ETextureDimension Dimension, const FTextureSubresourceSet& crSubresourceSet);
        void CreateRTV(UINT64 stDescriptorAddress, EFormat Format, const FTextureSubresourceSet& crSubresourceSet);
        void CreateDSV(UINT64 stDescriptorAddress, const FTextureSubresourceSet& crSubresourceSet, BOOL bIsReadOnly);

    public:
        FTextureDesc m_Desc;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_pD3D12Resource;
        D3D12_RESOURCE_DESC m_D3D12ResourceDesc{};
        
        UINT8 m_btPlaneCount = 1;

        FTextureBindingKeyHashMap<UINT32> m_RTVIndexMap;
        FTextureBindingKeyHashMap<UINT32> m_DSVIndexMap;
    
    private:
        const FDX12Context* m_cpContext;
        FDX12DescriptorHeaps* m_pDescriptorHeaps;

        FTextureBindingKeyHashMap<UINT32> m_SRVIndexMap;
        FTextureBindingKeyHashMap<UINT32> m_UAVIndexMap;
        
        TComPtr<IHeap> m_pHeap;

        std::vector<UINT32> m_ClearMipLevelUAVIndices;
    };

    
    // Foward shims. 
    // It doesn't allow FDX12Buffer has GetDesc(FBufferDesc* pDesc) const Function. 
    template <typename Deriving>
    struct NO_VTABLE _IBuffer : public IBuffer
    {
        FBufferDesc GetDesc() const override
        {
            return static_cast<const Deriving*>(this)->BufferGetDesc();
        }
    };

    template <typename Deriving>
    struct NO_VTABLE _IBufferStateTrack : public IBufferStateTrack
    {
        FBufferDesc GetDesc() const override
        {
            return static_cast<const Deriving*>(this)->BufferStateTrackGetDesc();
        }
    };

    class FDX12Buffer : 
        public TComObjectRoot<FComMultiThreadModel>,
        public _IBuffer<FDX12Buffer>,
        public _IBufferStateTrack<FDX12Buffer>
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12Buffer)
            INTERFACE_ENTRY(IID_IBuffer, IBuffer)
            INTERFACE_ENTRY(IID_IBufferStateTrack, IBufferStateTrack)
        END_INTERFACE_MAP

        FDX12Buffer(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps, const FBufferDesc& crDesc);
        ~FDX12Buffer() noexcept;

        BOOL Initialize();

        // IBuffer
        void* Map(ECpuAccessMode CpuAccess, HANDLE FenceEvent) override;
        void Unmap() override;
        FMemoryRequirements GetMemoryRequirements() override;
        BOOL BindMemory(IHeap* pHeap, UINT64 stOffset) override;
        FBufferDesc BufferGetDesc() const { return m_Desc; }

        // IBufferStateTrack
        FBufferDesc BufferStateTrackGetDesc() const { return m_Desc; }


        
        UINT32 GetClearUAVIndex();
        void CreateCBV(UINT64 stDescriptorAddress, const FBufferRange& crRange);
        void CreateSRV(UINT64 stDescriptorAddress, EFormat Format, const FBufferRange& crRange, EResourceType ResourceType);
        void CreateUAV(UINT64 stDescriptorAddress, EFormat Format, const FBufferRange& crRange, EResourceType ResourceType);

    public:
        Microsoft::WRL::ComPtr<ID3D12Resource> m_pD3D12Resource;
        FBufferDesc m_Desc;
        D3D12_GPU_VIRTUAL_ADDRESS m_GpuAddress = 0;

        // 用于 map 资源
        Microsoft::WRL::ComPtr<ID3D12Fence> m_pLastUsedFence;
        UINT64 m_stLastUsedFenceValue = 0;

    private:
        const FDX12Context* m_cpContext;
        FDX12DescriptorHeaps* m_pDescriptorHeaps;
        
        D3D12_RESOURCE_DESC m_D3D12ResourceDesc{};
        
        TComPtr<IHeap> m_pHeap;

        UINT32 m_dwClearUAVIndex = gdwInvalidViewIndex;
    };


    class FDX12StagingTexture :
        public TComObjectRoot<FComMultiThreadModel>,
        public IStagingTexture
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12StagingTexture)
            INTERFACE_ENTRY(IID_IStagingTexture, IStagingTexture)
        END_INTERFACE_MAP

        FDX12StagingTexture(const FDX12Context* cpContext, const FTextureDesc& crDesc, ECpuAccessMode CpuAccessMode);

        BOOL Initialize(FDX12DescriptorHeaps* pDescriptorHeaps);

        // IStagingTexture
        FTextureDesc GetDesc() const override { return m_Desc; }
        void* Map(const FTextureSlice& crTextureSlice, ECpuAccessMode CpuAccessMode, HANDLE FenceEvent, UINT64* pstRowPitch) override;
        void Unmap() override;

        
        FDX12SliceRegion GetSliceRegion(const FTextureSlice& crTextureSlice) const;
        UINT64 GetRequiredSize() const;
        void ComputeSubresourceOffsets();

    public:
        Microsoft::WRL::ComPtr<ID3D12Fence> m_pLastUsedFence;
        UINT64 m_stLastUsedFenceValue = 0;
        FTextureDesc m_Desc;

    private:
        TComPtr<IBuffer> m_pBuffer;

        const FDX12Context* m_cpContext;
        D3D12_RESOURCE_DESC m_D3D12ResourceDesc{};
        ECpuAccessMode m_MappedCpuAccessMode = ECpuAccessMode::None;
        std::vector<UINT64> m_SubresourceOffsets;

        FDX12SliceRegion m_MappedRegion{};
        ECpuAccessMode m_MappedAccessMode = ECpuAccessMode::None;
    };

    class FDX12Sampler :
        public TComObjectRoot<FComMultiThreadModel>,
        public ISampler
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12Sampler)
            INTERFACE_ENTRY(IID_ISampler, ISampler)
        END_INTERFACE_MAP

        FDX12Sampler(const FDX12Context* cpContext, const FSamplerDesc& crDesc);

        BOOL Initialize();

        // ISampler
        FSamplerDesc GetDesc() const override { return m_Desc; }


        void CreateDescriptor(UINT64 stDescriptorAddress) const;

    private:
        const FDX12Context* m_cpContext;
        FSamplerDesc m_Desc;
        D3D12_SAMPLER_DESC m_D3D12SamplerDesc{};
    };

}



#endif

