#ifndef RHI_RESOURCE_H
#define RHI_RESOURCE_H

#include "format.h"
#include "forward.h"
#include <cassert>
#include <cstdint>
#include <memory>
#include <windows.h>
#include <string>
#include "../core/math/common.h"

namespace fantasy
{
    enum class ResourceType : uint8_t
    {
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

    struct ResourceInterface
    {
		virtual ~ResourceInterface() = default;
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
        ShaderResource          = 1 << 4,
        UnorderedAccess         = 1 << 5,
        RenderTarget            = 1 << 6,
        DepthWrite              = 1 << 7,
        DepthRead               = 1 << 8,
        StreamOut               = 1 << 9,
        CopyDst                 = 1 << 10,
        CopySrc                 = 1 << 11,
        Present                 = 1 << 12,
        IndirectArgument        = 1 << 13
        // ,
        // AccelStructRead         = 1 << 14,
        // AccelStructWrite        = 1 << 15,
        // AccelStructBuildInput   = 1 << 16,
        // AccelStructBuildBlas    = 1 << 17
    };
    ENUM_CLASS_FLAG_OPERATORS(ResourceStates)


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
        
        bool is_virtual = false;


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
            std::string name = ""
        )
        {
            TextureDesc ret;
            ret.name = name;
            ret.width = width;
            ret.height = height;
            ret.format = format;
            ret.allow_render_target = true;
            return ret;
        }

        static TextureDesc create_depth_stencil_texture(
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
            ret.allow_depth_stencil = true;
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

    inline const TextureSubresourceSet entire_subresource_set = TextureSubresourceSet{
        .base_mip_level     = 0,
        .mip_level_count   = ~0u,
        .base_array_slice  = 0,
        .array_slice_count = ~0u
    };

    struct TextureInterface : public ResourceInterface
    {
        virtual const TextureDesc& get_desc() const = 0;

        virtual bool bind_memory(std::shared_ptr<HeapInterface> heap, uint64_t offset) = 0;
        virtual MemoryRequirements get_memory_requirements() = 0;

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
        bool is_volatile_constant_buffer = false;

        bool is_index_buffer = false;
        bool is_vertex_buffer = false;
        bool is_indirect_buffer = false;
        // bool is_shader_binding_table = false;
        // bool is_accel_struct_storage = false;

        bool is_virtual = false;
        
        static BufferDesc create_constant_buffer(uint64_t size, std::string name = "")
        {
            BufferDesc ret;
			ret.name = name;
			ret.byte_size = size;
			ret.is_constant_buffer = true;
            return ret;
        }

        static BufferDesc create_volatile_constant_buffer(uint64_t size, std::string name = "")
        {
            BufferDesc ret;
			ret.name = name;
			ret.byte_size = size;
			ret.is_volatile_constant_buffer = true;
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
    
    inline const BufferRange entire_buffer_range = BufferRange{ 0, ~0ull };


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