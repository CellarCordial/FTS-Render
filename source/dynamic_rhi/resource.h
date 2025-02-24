#ifndef RHI_RESOURCE_H
#define RHI_RESOURCE_H

#include "format.h"
#include "forward.h"
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>
#include <windows.h>
#include <string>
#include "../core/math/common.h"

namespace fantasy
{
    enum class ResourceType : uint8_t
    {
        Heap,
        Texture,
        Buffer,
        StagingTexture,
        Sampler,
        GraphicsPipeline,
        ComputePipeline,
        FrameBuffer,
        BindingSet,
        RayTracing_Pipeline,
        RayTracing_ShaderTable
    };

    enum class TextureDimension : uint8_t
    {
        Unknown,
        Texture1D,
        Texture1DArray,
        Texture2D,
        Texture2DArray,
        TextureCube,
        TextureCubeArray,
        Texture3D
    };

    enum class CpuAccessMode : uint8_t
    {
        None,
        Read,
        Write
    };
    
    enum class ResourceStates : uint32_t
    {
        Common                  = 0,
        ConstantBuffer          = 1 << 1,
        VertexBuffer            = 1 << 2,
        IndexBuffer             = 1 << 3,
        GraphicsShaderResource  = 1 << 4,
        ComputeShaderResource   = 1 << 5,
        UnorderedAccess         = 1 << 6,
        RenderTarget            = 1 << 7,
        DepthWrite              = 1 << 8,
        DepthRead               = 1 << 9,
        StreamOut               = 1 << 10,
        CopyDst                 = 1 << 11,
        CopySrc                 = 1 << 12,
        Present                 = 1 << 13,
        IndirectArgument        = 1 << 14,
        // ,
        // AccelStructRead         = 1 << 14,
        // AccelStructWrite        = 1 << 15,
        // AccelStructBuildInput   = 1 << 16,
        // AccelStructBuildBlas    = 1 << 17
    };
    ENUM_CLASS_FLAG_OPERATORS(ResourceStates)



    struct TextureTilesMapping
    {
        struct Region
        {
            uint16_t mip_level = 0;
            uint16_t array_level = 0;
            uint32_t x = 0;
            uint32_t y = 0;
            uint32_t z = 0;

            uint32_t tiles_num = 0;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t depth = 0;

            uint64_t byte_offset;
        };
        
        std::vector<Region> regions;

        HeapInterface* heap = nullptr;
    };

    struct TextureTileInfo
    {
        uint32_t total_tile_num = 0;

        uint32_t standard_mip_num = 0;
        uint32_t packed_mip_num = 0;
        uint32_t tile_num_for_packed_mips = 0;
        uint32_t start_tile_index = 0;

        uint32_t width_in_texels = 0;
        uint32_t height_in_texels = 0;
        uint32_t depth_in_texels = 0;

        struct SubresourceTiling
        {
            uint32_t width_in_tiles = 0;
            uint32_t height_in_tiles = 0;
            uint32_t depth_in_tiles = 0;
            uint32_t start_tile_index = 0;
        };
        std::vector<SubresourceTiling> subresource_tilings;
    };

    struct TextureDesc
    {
        std::string name;

        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t array_size = 1;
        uint32_t mip_levels = 1;

        Format format = Format::UNKNOWN;
        TextureDimension dimension = TextureDimension::Texture2D;
        
        bool use_clear_value = false;
        Color clear_value;
        
        bool allow_render_target = false;
        bool allow_depth_stencil = false;
        bool allow_shader_resource = true;
        bool allow_unordered_access = false;
        
        bool is_tiled = false;

        bool is_virtual = false;
        uint64_t offset_in_heap = INVALID_SIZE_64;

        static TextureDesc create_tiled_shader_resource_texture(
            uint32_t width, 
            uint32_t height, 
            Format format, 
            std::string name = ""
        )
        {
            TextureDesc ret;
            ret.name = name;
            ret.width = width;
            ret.height = height;
            ret.format = format;
            ret.is_tiled = true;
            return ret;
        }

        static TextureDesc create_virtual_shader_resource_texture(
            uint32_t width, 
            uint32_t height, 
            Format format, 
            std::string name = ""
        )
        {
            TextureDesc ret;
            ret.name = name;
            ret.width = width;
            ret.height = height;
            ret.format = format;
            ret.is_virtual = true;
            return ret;
        }

        static TextureDesc create_shader_resource_texture(
            uint32_t width, 
            uint32_t height, 
            Format format, 
            std::string name = ""
        )
        {
            TextureDesc ret;
            ret.name = name;
            ret.width = width;
            ret.height = height;
            ret.format = format;
            return ret;
        }

        static TextureDesc create_shader_resource_texture(
            uint32_t width, 
            uint32_t height, 
            uint32_t depth, 
            Format format, 
            std::string name = ""
        )
        {
            TextureDesc ret;
            ret.name = name;
            ret.width = width;
            ret.height = height;
            ret.depth = depth;
            ret.format = format;
            ret.dimension = TextureDimension::Texture3D;
            return ret;
        }
        
        static TextureDesc create_read_write_texture(
            uint32_t width, 
            uint32_t height, 
            Format format, 
            std::string name = ""
        )
        {
            TextureDesc ret;
            ret.name = name;
            ret.width = width;
            ret.height = height;
            ret.format = format;
            ret.allow_unordered_access = true;
            return ret;
        }

        static TextureDesc create_read_write_texture(
            uint32_t width, 
            uint32_t height, 
            uint32_t depth, 
            Format format, 
            std::string name = ""
        )
        {
            TextureDesc ret;
            ret.name = name;
            ret.width = width;
            ret.height = height;
            ret.depth = depth;
            ret.format = format;
            ret.dimension = TextureDimension::Texture3D;
            ret.allow_unordered_access = true;
            return ret;
        }

        static TextureDesc create_render_target_texture(
            uint32_t width, 
            uint32_t height, 
            Format format, 
            std::string name = "",
            bool allow_unordered_access = false
        )
        {
            TextureDesc ret;
            ret.name = name;
            ret.width = width;
            ret.height = height;
            ret.format = format;
            ret.allow_render_target = true;
            ret.use_clear_value = true;
            ret.clear_value = Color{ 0.0f };
            ret.allow_unordered_access = allow_unordered_access;
            return ret;
        }

        static TextureDesc create_depth_stencil_texture(
            uint32_t width, 
            uint32_t height, 
            Format format, 
            std::string name = "",
            bool reverse_z = false
        )
        {
            TextureDesc ret;
            ret.name = name;
            ret.width = width;
            ret.height = height;
            ret.format = format;
            ret.allow_depth_stencil = true;
            ret.use_clear_value = true;
            ret.clear_value = reverse_z ? Color{ 0.0f } : Color{ 1.0f };
            return ret;
        }

        static TextureDesc create_read_back_texture(
            uint32_t width, 
            uint32_t height, 
            Format format, 
            std::string name = ""
        )
        {
            return create_shader_resource_texture(width, height, format, name);
        }

        
        static TextureDesc create_read_back_texture(
            uint32_t width, 
            uint32_t height, 
            uint32_t depth,
            Format format, 
            std::string name = ""
        )
        {
            return create_shader_resource_texture(width, height, depth, format, name);
        }
    };
    
    struct TextureSlice
    {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t z = 0;
        
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 0;

        uint32_t mip_level = 0;
        uint32_t array_slice = 0;
    };
    
    struct TextureSubresourceSet
    {
        uint32_t base_mip_level = 0;
        uint32_t mip_level_count = 1;
        uint32_t base_array_slice = 0;
        uint32_t array_slice_count = 1;

        bool operator ==(const TextureSubresourceSet& other) const
        {
            return base_mip_level == other.base_mip_level &&
                   mip_level_count == other.mip_level_count &&
                   base_array_slice == other.base_array_slice &&
                   array_slice_count == other.array_slice_count;
        }

        bool is_entire_texture(const TextureDesc& desc) const
        {
            if (base_mip_level > 0 || base_mip_level + mip_level_count < desc.mip_levels)
            {
                return false;
            }
            return true;
        }
    };

    struct TextureInterface : public ResourceInterface
    {
        virtual const TextureDesc& get_desc() const = 0;

        virtual bool bind_memory(std::shared_ptr<HeapInterface> heap, uint64_t offset) = 0;
        virtual MemoryRequirements get_memory_requirements() = 0;
        virtual const TextureTileInfo& get_tile_info() = 0; 

        virtual void* get_native_object() = 0;
        
		virtual ~TextureInterface() = default;
    };

    
    struct StagingTextureInterface : public ResourceInterface
    {
        virtual const TextureDesc& get_desc() const = 0;
        virtual void* map(const TextureSlice& texture_slice, CpuAccessMode cpu_access_mode, uint64_t* row_pitch) = 0;
        virtual void unmap() = 0;

        virtual void* get_native_object() = 0;

		virtual ~StagingTextureInterface() = default;
    };


    struct BufferDesc
    {
        std::string name;

        uint64_t byte_size = 0;
        uint32_t struct_stride = 0;
        Format format = Format::UNKNOWN;
        CpuAccessMode cpu_access = CpuAccessMode::None;
        
        bool allow_shader_resource = true;
        bool allow_unordered_access = false;

        bool is_constant_buffer = false;
        bool is_index_buffer = false;
        bool is_vertex_buffer = false;
        bool is_indirect_buffer = false;
        // bool is_shader_binding_table = false;
        // bool is_accel_struct_storage = false;

        bool is_virtual = false;
        uint64_t offset_in_heap = INVALID_SIZE_64;
        
        static BufferDesc create_constant_buffer(uint64_t size, std::string name = "")
        {
            BufferDesc ret;
			ret.name = name;
			ret.byte_size = size;
			ret.is_constant_buffer = true;
            return ret;
        }

        static BufferDesc create_structured_buffer(uint64_t size, uint32_t stride, std::string name = "")
        {
            BufferDesc ret;
			ret.name = name;
			ret.byte_size = size;
            ret.struct_stride = stride;
            return ret;
        }

        static BufferDesc create_read_write_structured_buffer(uint64_t size, uint32_t stride, std::string name = "")
        {
            BufferDesc ret;
			ret.name = name;
			ret.byte_size = size;
            ret.struct_stride = stride;
			ret.allow_unordered_access = true;
            return ret;
        }

        static BufferDesc create_read_back_buffer(uint64_t size, std::string name = "")
        {
            BufferDesc ret;
			ret.name = name;
			ret.byte_size = size;
            ret.cpu_access = CpuAccessMode::Read;
            return ret;
        }

        static BufferDesc create_vertex_buffer(uint64_t size, std::string name = "")
        {
            BufferDesc ret;
			ret.name = name;
			ret.byte_size = size;
            ret.is_vertex_buffer = true;
            return ret;
        }

        static BufferDesc create_index_buffer(uint64_t size, std::string name = "")
        {
            BufferDesc ret;
			ret.name = name;
			ret.byte_size = size;
            ret.is_index_buffer = true;
            return ret;
        }
    };

    struct BufferRange
    {
        uint64_t byte_offset = 0;
        uint64_t byte_size = 0;

        bool is_entire_buffer(const BufferDesc& desc) const
        {
            return  byte_offset == 0 && 
                    byte_size == static_cast<uint64_t>(-1) ||
                    byte_size == desc.byte_size;

        }

        bool operator==(const BufferRange& other) const 
        {
            return byte_offset == other.byte_offset && byte_size == other.byte_size;
        }
    };
    

    struct BufferInterface : public ResourceInterface
    {
        virtual const BufferDesc& get_desc() const = 0;
        virtual void* map(CpuAccessMode cpu_access) = 0;
        virtual void unmap() = 0;
        virtual MemoryRequirements get_memory_requirements() = 0;
        virtual bool bind_memory(std::shared_ptr<HeapInterface> heap, uint64_t offset) = 0;

        virtual void* get_native_object() = 0;
        
		virtual ~BufferInterface() = default;
    };


    enum class SamplerAddressMode : uint8_t
    {
        Clamp       = 1,
        Wrap        = 2,
        Border      = 3,
        Mirror      = 4,
        MirrorOnce  = 5,
    };

    enum class SamplerReductionType : uint8_t
    {
        Standard    = 1,
        Comparison  = 2,
        Minimum     = 3,
        Maximum     = 4
    };

    struct SamplerDesc
    {
        std::string name;

        Color border_color;
        float max_anisotropy = 1.0f;
        float mip_bias = 0.0f;

        bool min_filter = true;
        bool max_filter = true;
        bool mip_filter = true;
        SamplerAddressMode address_u = SamplerAddressMode::Wrap;
        SamplerAddressMode address_v = SamplerAddressMode::Wrap;
        SamplerAddressMode address_w = SamplerAddressMode::Wrap;
        SamplerReductionType reduction_type = SamplerReductionType::Standard;

        void SetAddressMode(SamplerAddressMode mode)
        {
			address_u = mode;
			address_v = mode;
            address_w = mode;
		}
        
        void SetFilter(bool min_max_mip)
        {
			min_filter = min_max_mip;
			max_filter = min_max_mip;
			mip_filter = min_max_mip;
        }
    };

    struct SamplerInterface : public ResourceInterface
    {
        virtual const SamplerDesc& get_desc() const = 0;
        
		virtual ~SamplerInterface() = default;
    };


    inline uint32_t calculate_texture_subresource(
        uint32_t mip_level,
        uint32_t array_slice,
        uint32_t mip_levels
    )
    {
        return mip_level + (array_slice * mip_levels);
    }
}


#endif