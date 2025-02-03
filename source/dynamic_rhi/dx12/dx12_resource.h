#ifndef RHI_DX12_RESOURCE_H
#define RHI_DX12_RESOURCE_H

#include "../descriptor.h"
#include "dx12_descriptor.h"
#include <functional>
#include <memory>
#include <unordered_map>
namespace fantasy
{
    struct DX12TextureBindingKey : public TextureSubresourceSet
    {
        Format format;
        bool is_read_only_dsv;

        DX12TextureBindingKey(const TextureSubresourceSet& subresource_set, Format format, bool is_read_only_dsv = false) :
            TextureSubresourceSet{ subresource_set }, 
            format(format), 
            is_read_only_dsv(is_read_only_dsv)
        {
        }

        bool operator==(const DX12TextureBindingKey& other) const
        {
            return  base_mip_level == other.base_mip_level &&
                    mip_level_count == other.mip_level_count &&
                    base_array_slice == other.base_array_slice &&
                    array_slice_count == other.array_slice_count &&
                    format == other.format &&
                    is_read_only_dsv == other.is_read_only_dsv;
        }
    };

    template <typename T>
    using TextureBindingKeyHashMap = std::unordered_map<DX12TextureBindingKey, T>;
}

namespace std 
{
    template<>
    struct hash<fantasy::DX12TextureBindingKey>
    {
        std::size_t operator()(const fantasy::DX12TextureBindingKey& key) const noexcept
        {
            return std::hash<uint32_t>()(key.array_slice_count) ^
                   std::hash<uint32_t>()(key.base_array_slice) ^
                   std::hash<uint32_t>()(key.base_mip_level) ^
                   std::hash<uint32_t>()(key.mip_level_count) ^
                   std::hash<bool>()(key.is_read_only_dsv) ^
                   std::hash<fantasy::Format>()(key.format);
        }
    };
}

namespace fantasy
{
    class DX12Texture : public TextureInterface
    {
    public:
        DX12Texture(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps, const TextureDesc& desc);
        ~DX12Texture() noexcept;

        bool initialize();

        // TextureInterface
        const TextureDesc& get_desc() const override { return _desc; }
        MemoryRequirements get_memory_requirements() override;
		bool bind_memory(HeapInterface* heap, uint64_t offset) override;
		uint32_t get_view_index(ViewType type, TextureSubresourceSet subresource, bool is_read_only_dsv = false) override;
        void* get_native_object() override { return _d3d12_resource.Get(); }

        uint32_t GetClearMipLevelUAVIndex(uint32_t mip_level);
        void create_srv(size_t descriptor_address, Format format, TextureDimension dimension, const TextureSubresourceSet& subresource_set);
        void create_uav(size_t descriptor_address, Format format, TextureDimension dimension, const TextureSubresourceSet& subresource_set);
        void CreateRTV(size_t descriptor_address, Format format, const TextureSubresourceSet& subresource_set);
        void CreateDSV(size_t descriptor_address, const TextureSubresourceSet& subresource_set, bool is_read_only);

    public:
        uint8_t plane_count = 1;

        TextureBindingKeyHashMap<uint32_t> rtv_indices;
        TextureBindingKeyHashMap<uint32_t> dsv_indices;
    
        Microsoft::WRL::ComPtr<ID3D12Resource> _d3d12_resource;
        HANDLE shared_handle;

    private:
        const DX12Context* _context;
        DX12DescriptorHeaps* _descriptor_heaps;

        TextureDesc _desc;
        D3D12_RESOURCE_DESC _d3d12_resource_desc{};

        TextureBindingKeyHashMap<uint32_t> _srv_indices;
        TextureBindingKeyHashMap<uint32_t> _uav_indices;
        
		uint32_t _srv_index = INVALID_SIZE_32;

        HeapInterface* _heap;

        std::vector<uint32_t> _clear_mip_level_uav_indices;
    };


    class DX12Buffer : public BufferInterface
    {
    public:
        DX12Buffer(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps, const BufferDesc& desc);
        ~DX12Buffer() noexcept;

        bool initialize();

        // BufferInterface
        const BufferDesc& get_desc() const override { return _desc; }
        void* map(CpuAccessMode cpu_access, HANDLE fence_event) override;
        void unmap() override;
        MemoryRequirements get_memory_requirements() override;
        bool bind_memory(HeapInterface* heap, uint64_t offset) override;
		uint32_t get_view_index(ViewType type, const BufferRange& range) override;
        void* get_native_object() override { return _d3d12_resource.Get(); }

        uint32_t get_clear_uav_index();
        void create_cbv(uint64_t descriptor_address, const BufferRange& range);
        void create_srv(uint64_t descriptor_address, Format format, const BufferRange& range, ResourceViewType type);
        void create_uav(uint64_t descriptor_address, Format format, const BufferRange& range, ResourceViewType type);

    public:
        // 用于 map 资源
        Microsoft::WRL::ComPtr<ID3D12Fence> last_used_d3d12_fence;
        uint64_t last_used_fence_value = 0;

        Microsoft::WRL::ComPtr<ID3D12Resource> _d3d12_resource;

    private:
        const DX12Context* _context;
        DX12DescriptorHeaps* _descriptor_heaps;
        
        BufferDesc _desc;

        D3D12_RESOURCE_DESC _d3d12_resource_desc{};
        
        HeapInterface* _heap;

        uint32_t _srv_index = INVALID_SIZE_32;
        uint32_t _uav_index = INVALID_SIZE_32;
        uint32_t _clear_uav_index = INVALID_SIZE_32;
    };


    class DX12StagingTexture : public StagingTextureInterface
    {
    public:
        DX12StagingTexture(const DX12Context* context, const TextureDesc& desc, CpuAccessMode mode);

        bool initialize(DX12DescriptorHeaps* descriptor_heaps);

        // StagingTextureInterface
        const TextureDesc& get_desc() const override { return _desc; }
        void* map(const TextureSlice& texture_slice, CpuAccessMode mode, HANDLE fence_event, uint64_t* row_pitch) override;
        void unmap() override;
        void* get_native_object() override { return _buffer->get_native_object(); }

        
        DX12SliceRegion get_slice_region(const TextureSlice& texture_slice) const;
        uint64_t get_required_size() const;
        void compute_subresource_offsets();
        BufferInterface* get_buffer() { return _buffer.get(); }

    public:
        Microsoft::WRL::ComPtr<ID3D12Fence> last_used_d3d12_fence;
        uint64_t last_used_fence_value = 0;

    private:
        const DX12Context* _context;

        TextureDesc _desc;
        std::unique_ptr<BufferInterface> _buffer;

        D3D12_RESOURCE_DESC _d3d12_resource_desc{};
        CpuAccessMode _mapped_cpu_access_mode = CpuAccessMode::None;
        std::vector<uint64_t> _subresource_offsets;

        DX12SliceRegion _mapped_region{};
        CpuAccessMode _mapped_access_mode = CpuAccessMode::None;
    };

    class DX12Sampler : public SamplerInterface
    {
    public:
        DX12Sampler(const DX12Context* context, const SamplerDesc& desc);

        bool initialize();

        // SamplerInterface
        const SamplerDesc& get_desc() const override { return _desc; }

        void create_descriptor(uint64_t descriptor_address) const;

    private:
        const DX12Context* _context;
        SamplerDesc _desc;
        D3D12_SAMPLER_DESC _d3d12_sampler_desc{};
    };
}



#endif

