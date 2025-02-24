#ifndef RHI_DX12_RESOURCE_H
#define RHI_DX12_RESOURCE_H

#include "../binding.h"
#include "dx12_descriptor.h"
#include "../../core/tools/hash_table.h"
#include "dx12_forward.h"
#include <functional>
#include <memory>
#include <tuple>
#include <unordered_map>

namespace std 
{
    template<>
    struct hash<fantasy::TextureSubresourceSet>
    {
        std::size_t operator()(const fantasy::TextureSubresourceSet& key) const noexcept
        {
            return std::hash<uint32_t>()(key.array_slice_count) ^
                   std::hash<uint32_t>()(key.base_array_slice) ^
                   std::hash<uint32_t>()(key.base_mip_level) ^
                   std::hash<uint32_t>()(key.mip_level_count);
        }
    };
}

namespace fantasy
{
    class DX12Texture : public TextureInterface
    {
    public:
        using SubresourceViewKey = std::tuple<TextureSubresourceSet, Format, ResourceViewType>;

        struct Hash
        {
            std::size_t operator()(SubresourceViewKey const& s) const noexcept
            {
                const auto& [subresources, format, view_type] = s;

                size_t hash = 0;

                hash_combine(hash, subresources.base_mip_level);
                hash_combine(hash, subresources.mip_level_count);
                hash_combine(hash, subresources.base_array_slice);
                hash_combine(hash, subresources.array_slice_count);
                hash_combine(hash, format);
                hash_combine(hash, view_type);

                return hash;
            }
        };

    public:
        DX12Texture(const DX12Context* context, DX12DescriptorManager* descriptor_heaps, const TextureDesc& desc);
        ~DX12Texture() noexcept;

        bool initialize();

        const TextureDesc& get_desc() const override;
        MemoryRequirements get_memory_requirements() override;
		bool bind_memory(std::shared_ptr<HeapInterface> heap, uint64_t offset) override;
        const TextureTileInfo& get_tile_info() override; 
        void* get_native_object() override;

		uint32_t get_view_index(ResourceViewType view_type, const TextureSubresourceSet& subresource, Format format = Format::UNKNOWN);
        void create_srv(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const TextureSubresourceSet& subresource_set, Format format = Format::UNKNOWN);
        void create_uav(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const TextureSubresourceSet& subresource_set, Format format = Format::UNKNOWN);
        void create_rtv(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const TextureSubresourceSet& subresource_set, Format format = Format::UNKNOWN);
        void create_dsv(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const TextureSubresourceSet& subresource_set, Format format = Format::UNKNOWN);

    public:
        TextureDesc desc;
        TextureTileInfo tile_info;
        
        D3D12_RESOURCE_DESC d3d12_resource_desc;
        Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource;

        std::unordered_map<SubresourceViewKey, uint32_t, Hash> view_cache;
    
        std::shared_ptr<HeapInterface> heap;


    private:
        const DX12Context* _context;
        DX12DescriptorManager* _descriptor_manager;

        std::vector<uint32_t> _mip_uav_cache_for_clear;
    };


    class DX12Buffer : public BufferInterface
    {
    public:
        using SubrangeViewKey = std::tuple<BufferRange, Format, ResourceViewType>;

        struct Hash
        {
            std::size_t operator()(const SubrangeViewKey& s) const noexcept
            {
                const auto& [range, format, view_type] = s;

                size_t hash = 0;

                hash_combine(hash, range.byte_offset);
                hash_combine(hash, range.byte_size);
                hash_combine(hash, format);
                hash_combine(hash, view_type);

                return hash;
            }
        };

    public:
        DX12Buffer(const DX12Context* context, DX12DescriptorManager* descriptor_heaps, const BufferDesc& desc);
        ~DX12Buffer() noexcept;

        bool initialize();

        const BufferDesc& get_desc() const override;
        void* map(CpuAccessMode cpu_access) override;
        void unmap() override;
        MemoryRequirements get_memory_requirements() override;
        bool bind_memory(std::shared_ptr<HeapInterface> heap, uint64_t offset) override;
        void* get_native_object() override { return d3d12_resource.Get(); }

		uint32_t get_view_index(ResourceViewType view_type, const BufferRange& range, Format format = Format::UNKNOWN);
        void create_cbv(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const BufferRange& range);
        void create_srv(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const BufferRange& range, ResourceViewType type, Format format = Format::UNKNOWN);
        void create_uav(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const BufferRange& range, ResourceViewType type, Format format = Format::UNKNOWN);
        
    public:
        BufferDesc desc;

        D3D12_RESOURCE_DESC d3d12_resource_desc;
        Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource;

        std::shared_ptr<HeapInterface> heap;
        std::unordered_map<SubrangeViewKey, uint32_t, Hash> view_cache;
        
    private:
        const DX12Context* _context;
        DX12DescriptorManager* _descriptor_manager;
        
    };


    struct DX12SliceRegion
    {
        uint64_t offset;
        uint64_t size;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT d3d12_foot_print;
    };

    class DX12StagingTexture : public StagingTextureInterface
    {
    public:
        DX12StagingTexture(const DX12Context* context, const TextureDesc& desc, CpuAccessMode cpu_access_mode);

        bool initialize(DX12DescriptorManager* descriptor_heaps);

        const TextureDesc& get_desc() const override;
        void* map(const TextureSlice& texture_slice, CpuAccessMode mode, uint64_t* row_pitch) override;
        void unmap() override;
        void* get_native_object() override { return _buffer->get_native_object(); }

        DX12SliceRegion get_slice_region(uint32_t mip_level, uint32_t array_slice) const;
        BufferInterface* get_buffer() { return _buffer.get(); }

    public:
        TextureDesc desc;
        std::vector<uint64_t> subresource_offsets;
        CpuAccessMode access_mode = CpuAccessMode::None;
        
    private:
        const DX12Context* _context;

        D3D12_RESOURCE_DESC _d3d12_resource_desc;
        D3D12_RANGE _d3d12_mapped_range;
        std::unique_ptr<BufferInterface> _buffer;
    };

    class DX12Sampler : public SamplerInterface 
    {
    public:
        DX12Sampler(const DX12Context* context, const SamplerDesc& desc);

        bool initialize();

        const SamplerDesc& get_desc() const override { return desc; }

        void create_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor) const;
    
    public:
        SamplerDesc desc;
        D3D12_SAMPLER_DESC d3d12_sampler_desc;
    
    private:
        const DX12Context* _context;
    };
}



#endif

