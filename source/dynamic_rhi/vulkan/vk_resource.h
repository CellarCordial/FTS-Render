#ifndef DYNAMIC_RHI_VULKAN_RESOURCE_H
#define DYNAMIC_RHI_VULKAN_RESOURCE_H

#include "vk_allocator.h"
#include "../command_list.h"
#include "../../core/tools/hash_table.h"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace fantasy
{
    struct VKTexture;

    struct VKTextureSubresourceView
    {
        VKTexture* texture;
        TextureSubresourceSet subresource;

        vk::ImageView view = nullptr;
        vk::ImageSubresourceRange subresource_range;

        VKTextureSubresourceView(VKTexture* texture)  : texture(texture) {}
        bool operator==(const VKTextureSubresourceView& other) const
        {
            return &texture == &other.texture &&
                    subresource == other.subresource &&
                    view == other.view &&
                    subresource_range == other.subresource_range;
        }
    };

    class VKTexture : public TextureInterface
    {
    public:
        enum class TextureSubresourceViewType
        {
            AllAspects,
            DepthOnly,
            StencilOnly
        };

        using SubresourceViewKey = std::tuple<TextureSubresourceSet, TextureSubresourceViewType, TextureDimension, Format, vk::ImageUsageFlags>;

        struct Hash
        {
            std::size_t operator()(SubresourceViewKey const& s) const noexcept
            {
                const auto& [subresources, view_type, dimension, format, usage] = s;

                size_t hash = 0;

                hash_combine(hash, subresources.base_mip_level);
                hash_combine(hash, subresources.mip_level_count);
                hash_combine(hash, subresources.base_array_slice);
                hash_combine(hash, subresources.array_slice_count);
                hash_combine(hash, view_type);
                hash_combine(hash, dimension);
                hash_combine(hash, format);
                hash_combine(hash, static_cast<uint32_t>(usage));

                return hash;
            }
        };

    public:
        VKTexture(const VKContext* context, const VKMemoryAllocator* allocator, const TextureDesc& desc_);
        ~VKTexture() override;

        bool initialize();

        const TextureDesc& get_desc() const override { return desc; }

		uint32_t get_view_index(ViewType view_type, TextureSubresourceSet subresource, bool is_read_only_dsv = false) override;
        bool bind_memory(HeapInterface* heap, uint64_t offset) override;
        MemoryRequirements get_memory_requirements() override;
        void* get_native_object() override;

        VKTextureSubresourceView& get_subresource_view(
            const TextureSubresourceSet& subresources, 
            TextureDimension dimension,
            Format format, 
            vk::ImageUsageFlags usage, 
            TextureSubresourceViewType view_type = TextureSubresourceViewType::AllAspects
        );
        
        uint32_t get_numSubresources() const;
        uint32_t get_subresource_index(uint32_t mip_level, uint32_t array_layer) const;

    public:
        static constexpr uint32_t tile_byte_size = 65536;

        TextureDesc desc;

        vk::ImageCreateInfo image_info;
        vk::Image image;

        std::shared_ptr<HeapInterface> heap;
        void* shared_handle = nullptr;
        std::unordered_map<SubresourceViewKey, VKTextureSubresourceView, VKTexture::Hash> subresource_views;

    private:
        const VKContext* _context;
        const VKMemoryAllocator* _allocator;
        std::mutex _mutex;

        vk::DeviceMemory _vk_device_memory;
    };

    struct VKVolatileBufferState
    {
        int latest_version = 0;
        int min_version = 0;
        int max_version = 0;
        bool initialized = false;
    };
    
    struct VKBufferVersionItem : public std::atomic<uint64_t>
    {
        VKBufferVersionItem() : std::atomic<uint64_t>() {}
        VKBufferVersionItem(const VKBufferVersionItem& other) { store(other); }
        VKBufferVersionItem& operator=(const uint64_t a) { store(a); return *this; }
    };

    class VKBuffer : public BufferInterface
    {
    public:
        VKBuffer(const VKContext* context, const VKMemoryAllocator* allocator) :
            _context(context), _allocator(allocator)
        {
        }
        ~VKBuffer() override;
        
        const BufferDesc& get_desc() const override { return desc; }
        uint32_t get_view_index(ViewType view_type, const BufferRange& range) override;
        
        void* map(CpuAccessMode cpu_access, HANDLE fence_event) override;
        void unmap() override;
        
        MemoryRequirements get_memory_requirements() override;
        bool bind_memory(HeapInterface* heap, uint64_t offset) override;
        
        void* get_native_object() override;

    public:
        BufferDesc desc;

        vk::Buffer buffer;
        vk::DeviceAddress deviceAddress = 0;
        std::shared_ptr<HeapInterface> heap;

        std::unordered_map<uint64_t, vk::BufferView> view_cache;
        std::vector<VKBufferVersionItem> version_tracking;
        void* mapped_memory = nullptr;
        void* shared_handle = nullptr;
        uint32_t version_search_start = 0;

        CommandQueueType last_use_queue = CommandQueueType::Graphics;
        uint64_t last_use_commandList_id = 0;

    private:
        const VKContext* _context;
        const VKMemoryAllocator* _allocator;
    };
    
    struct VKStagingTextureRegion
    {
        off_t offset;
        size_t size;
    };

    class VKStagingTexture : public StagingTextureInterface
    {
    public:
        VKStagingTexture() = default;
        ~VKStagingTexture() override = default;

        void* map(const TextureSlice& texture_slize, CpuAccessMode cpu_access_mode, HANDLE fence_event, uint64_t* row_pitch) override;
        void unmap() override;
        const TextureDesc& get_desc() const override { return desc; }
        void* get_native_object() override;

        size_t compute_slice_size(uint32_t mipLevel);
        const VKStagingTextureRegion& get_slice_region(uint32_t mipLevel, uint32_t arraySlice, uint32_t z);
        void populate_slice_regions();

        size_t get_buffer_size()
        {
            assert(sliceRegions.size());
            size_t size = slice_regions.back().offset + slice_regions.back().size;
            assert(size > 0);
            return size;
        }

    public:
        TextureDesc desc;
        std::unique_ptr<BufferInterface> buffer;
        std::vector<VKStagingTextureRegion> slice_regions;
    };

    class VKSampler : public SamplerInterface
    {
    public:
        explicit VKSampler(const VKContext* context)  : _context(context) {}
        ~VKSampler() override;

        const SamplerDesc& get_desc() const override;
    
    public:
        SamplerDesc desc;
        vk::Sampler sampler;
        vk::SamplerCreateInfo sampler_info;

    private:
        const VKContext* _context;
    };
}

#endif