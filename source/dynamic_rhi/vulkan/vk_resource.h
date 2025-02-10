#ifndef DYNAMIC_RHI_VULKAN_RESOURCE_H
#define DYNAMIC_RHI_VULKAN_RESOURCE_H

#include "vk_memory.h"
#include "../binding.h"
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

        vk::ImageView vk_view = nullptr;
        vk::ImageSubresourceRange vk_subresource_range = vk::ImageSubresourceRange();

        explicit VKTextureSubresourceView(VKTexture* texture, const TextureSubresourceSet& subresource_) : 
            texture(texture), subresource(subresource_) 
        {
        }
        bool operator==(const VKTextureSubresourceView& other) const
        {
            return &texture == &other.texture &&
                    subresource == other.subresource &&
                    vk_view == other.vk_view &&
                    vk_subresource_range == other.vk_subresource_range;
        }
    };

    class VKTexture : public TextureInterface
    {
    public:
        using SubresourceViewKey = std::pair<TextureSubresourceSet, ResourceViewType>;

        struct Hash
        {
            std::size_t operator()(SubresourceViewKey const& s) const noexcept
            {
                const auto& [subresources, view_type] = s;

                size_t hash = 0;

                hash_combine(hash, subresources.base_mip_level);
                hash_combine(hash, subresources.mip_level_count);
                hash_combine(hash, subresources.base_array_slice);
                hash_combine(hash, subresources.array_slice_count);
                hash_combine(hash, view_type);

                return hash;
            }
        };

    public:
        VKTexture(const VKContext* context, const VKMemoryAllocator* allocator, const TextureDesc& desc_);
        ~VKTexture() override;

        bool initialize();

        const TextureDesc& get_desc() const override;
        bool bind_memory(std::shared_ptr<HeapInterface> heap, uint64_t offset) override;
        MemoryRequirements get_memory_requirements() override;
        void* get_native_object() override;

		vk::ImageView get_view(ResourceViewType view_type, const TextureSubresourceSet& subresource);

    public:
        TextureDesc desc;

        vk::Image vk_image;
        vk::ImageCreateInfo vk_image_info;
        vk::DeviceMemory vk_device_memory;

        std::shared_ptr<HeapInterface> heap;
        std::unordered_map<SubresourceViewKey, VKTextureSubresourceView, VKTexture::Hash> view_cache;

    private:
        const VKContext* _context;
        const VKMemoryAllocator* _allocator;
        std::mutex _mutex;
    };

    struct VKBufferVersion : public std::atomic<uint64_t>
    {
        VKBufferVersion() : std::atomic<uint64_t>() {}
        VKBufferVersion(const VKBufferVersion& other) { store(other); }
        VKBufferVersion& operator=(const uint64_t a) { store(a); return *this; }
    };

    class VKBuffer : public BufferInterface
    {
    public:
        VKBuffer(const VKContext* context, const VKMemoryAllocator* allocator, const BufferDesc& desc_);
        ~VKBuffer() override;

        bool initialize();
        
        const BufferDesc& get_desc() const override;
        void* map(CpuAccessMode cpu_access) override;
        void unmap() override;
        MemoryRequirements get_memory_requirements() override;
        bool bind_memory(std::shared_ptr<HeapInterface> heap, uint64_t offset) override;
        void* get_native_object() override;

        vk::BufferView get_typed_buffer_view(const BufferRange& range, ResourceViewType type);

    public:
        BufferDesc desc;

        vk::Buffer vk_buffer;
        vk::BufferCreateInfo vk_buffer_info;
        vk::DeviceMemory vk_device_memory;

        std::shared_ptr<HeapInterface> heap;
        std::unordered_map<uint64_t, vk::BufferView> view_cache;
        
        CommandQueueType last_used_cmdqueue_type = CommandQueueType::Graphics;
        uint64_t last_used_cmdlist_id = INVALID_SIZE_64;

        void* mapped_volatile_memory = nullptr;
        std::vector<VKBufferVersion> version_tracking;
        uint32_t version_search_start = 0;

    private:
        const VKContext* _context;
        const VKMemoryAllocator* _allocator;
    };

    
    struct VKSliceRegion
    {
        uint32_t offset;
        size_t size;
    };

    class VKStagingTexture : public StagingTextureInterface
    {
    public:
        VKStagingTexture(const VKContext* context, const TextureDesc& desc, CpuAccessMode access_mode);
        ~VKStagingTexture() override = default;

        bool initialize(const VKMemoryAllocator* allocator);

        const TextureDesc& get_desc() const override;
        void* map(const TextureSlice& texture_slice, CpuAccessMode cpu_access_mode, uint64_t* row_pitch) override;
        void unmap() override;
        void* get_native_object() override;

        std::shared_ptr<BufferInterface> get_buffer();
        const VKSliceRegion& get_slice_region(uint32_t mip_level, uint32_t array_slice, uint32_t depth_index);

    public:
        TextureDesc desc;
        std::vector<VKSliceRegion> slice_regions;
        CpuAccessMode access_mode = CpuAccessMode::None;

    private:
        const VKContext* _context;

        std::shared_ptr<BufferInterface> _buffer;
    };


    class VKSampler : public SamplerInterface
    {
    public:
        explicit VKSampler(const VKContext* context, const SamplerDesc& desc);
        ~VKSampler() override;

        bool initialize();

        const SamplerDesc& get_desc() const override { return desc; }
    
    public:
        SamplerDesc desc;

        vk::Sampler vk_sampler;
        vk::SamplerCreateInfo vk_sampler_info;

    private:
        const VKContext* _context;
    };
    

}

#endif