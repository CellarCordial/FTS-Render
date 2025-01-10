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
#include "../core/math/matrix.h"

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
        Texture2DMS,
        Texture2DMSArray,
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
        PixelShaderResource     = 1 << 4,
        NonPixelShaderResource  = 1 << 5,
        UnorderedAccess         = 1 << 6,
        RenderTarget            = 1 << 7,
        DepthWrite              = 1 << 8,
        DepthRead               = 1 << 9,
        StreamOut               = 1 << 10,
        CopyDest                = 1 << 11,
        CopySource              = 1 << 12,
        ResolveDst              = 1 << 13,
        ResolveSrc              = 1 << 14,
        Present                 = 1 << 15,
        AccelStructRead         = 1 << 16,
        AccelStructWrite        = 1 << 17,
        AccelStructBuildInput   = 1 << 18,
        AccelStructBuildBlas    = 1 << 19
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

        uint32_t sample_count = 1;
        uint32_t sample_quality = 0;
        
        Format format = Format::UNKNOWN;
        TextureDimension dimension = TextureDimension::Texture2D;
        
        bool is_shader_resource = true;
        bool is_render_target = false;
        bool is_depth_stencil = false;
        bool is_uav = false;
        bool is_type_less = false;

        bool is_virtual = false;

        bool use_clear_value = false;
        Color clear_value;

        ResourceStates initial_state = ResourceStates::Common;


        static TextureDesc create_render_target(uint32_t width, uint32_t height, Format format, std::string name = "")
        {
            TextureDesc ret;
            ret.is_render_target = true;
            ret.initial_state = ResourceStates::RenderTarget;
            ret.clear_value = Color{ 0.0f, 0.0f, 0.0f, 0.0f };
            ret.use_clear_value = true;
            ret.width = width;
            ret.height = height;
            ret.format = format;
            ret.name = name;
            return ret;
        }

        static TextureDesc create_depth(uint32_t width, uint32_t height, Format format, std::string name = "")
        {
            TextureDesc ret;
            ret.is_depth_stencil = true;
            ret.initial_state = ResourceStates::DepthWrite;
            ret.use_clear_value = true;
            ret.clear_value = Color{ 1.0f, 0.0f, 0.0f, 0.0f };
            ret.width = width;
            ret.height = height;
            ret.format = format;
            ret.name = name;
            return ret;
        }

        static TextureDesc create_shader_resource(
            uint32_t width, uint32_t height, Format format, bool bComputePass = false, std::string name = ""
        )
        {
            TextureDesc ret;
            ret.is_shader_resource = true;
            ret.width = width;
            ret.height = height;
            ret.format = format;
			ret.initial_state = bComputePass ? ResourceStates::NonPixelShaderResource : ResourceStates::PixelShaderResource;
            ret.name = name;
            return ret;
        }

        static TextureDesc create_shader_resource(
            uint32_t width, uint32_t height, uint32_t depth, Format format, bool bComputePass = false, std::string name = ""
        )
        {
            TextureDesc ret;
            ret.is_shader_resource = true;
			ret.width = width;
            ret.height = height;
            ret.depth = depth;
            ret.dimension = TextureDimension::Texture3D;
            ret.format = format;
            ret.initial_state = bComputePass ? ResourceStates::NonPixelShaderResource : ResourceStates::PixelShaderResource;
            ret.name = name;
            return ret;
        }

        static TextureDesc create_read_write(uint32_t width, uint32_t height, Format format, std::string name = "")
        {
            TextureDesc ret;
            ret.width = width;
            ret.height = height;
            ret.format = format;
            ret.initial_state = ResourceStates::UnorderedAccess;
            ret.is_uav = true;
            ret.name = name;
            return ret;
        }

		static TextureDesc create_read_write(uint32_t width, uint32_t height, uint32_t depth, Format format, std::string name = "")
		{
			TextureDesc ret;
			ret.width = width;
			ret.height = height;
            ret.depth = depth;
			ret.format = format;
            ret.dimension = TextureDimension::Texture3D;
			ret.initial_state = ResourceStates::UnorderedAccess;
			ret.is_uav = true;
			ret.name = name;
			return ret;
		}

        static TextureDesc create_read_back(uint32_t width, uint32_t height, Format format, std::string name = "")
        {
            TextureDesc ret;
            ret.width = width;
            ret.height = height;
            ret.format = format;
            ret.initial_state = ResourceStates::CopyDest;
            ret.name = name;
            return ret;
        }

		static TextureDesc create_read_back(uint32_t width, uint32_t height, uint32_t depth, Format format, std::string name = "")
		{
			TextureDesc ret;
			ret.width = width;
			ret.height = height;
			ret.depth = depth;
			ret.format = format;
			ret.dimension = TextureDimension::Texture3D;
			ret.initial_state = ResourceStates::CopyDest;
			ret.name = name;
			return ret;
		}
    };
    
    struct TextureSlice
    {
        // 纹理切片的起始坐标
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t z = 0;
        
        // 当值为 -1 时，表示该维度（分量）的整个尺寸都是切片的一部分
        // 使用 resolve 方法，会根据给定的纹理描述 desc 来确定实际的尺寸
        uint32_t width = static_cast<uint32_t>(-1);
        uint32_t height = static_cast<uint32_t>(-1);
        uint32_t depth = static_cast<uint32_t>(-1);

        uint32_t mip_level = 0;
        uint32_t array_slice = 0;

        TextureSlice resolve(const TextureDesc& desc) const
        {
            TextureSlice ret(*this);

    #ifndef NDEBUG
            assert(mip_level < desc.mip_levels);
    #endif
            if (width == static_cast<uint32_t>(-1))
                ret.width = std::max(desc.width >> mip_level, 1u);

            if (height == static_cast<uint32_t>(-1))
                ret.height = std::max(desc.height >> mip_level, 1u);

            if (depth == static_cast<uint32_t>(-1))
            {
                if (desc.dimension == TextureDimension::Texture3D)
                {
                    ret.depth = std::max(desc.depth >> mip_level, 1u);
                    
                }
                else
                {
                    ret.depth = 1;
                }
            }

            return ret;
        }
    };
    
    struct TextureSubresourceSet
    {
        uint32_t base_mip_level = 0;
        uint32_t mip_level_count = 1;
        uint32_t base_array_slice = 0;
        uint32_t array_slice_count = 1;

        bool is_entire_texture(const TextureDesc& desc) const
        {
            if (base_mip_level > 0 || base_mip_level + mip_level_count < desc.mip_levels)
            {
                return false;
            }

            switch (desc.dimension)
            {
            case TextureDimension::Texture1DArray:
            case TextureDimension::Texture2DArray:
            case TextureDimension::TextureCube:
            case TextureDimension::TextureCubeArray:
            case TextureDimension::Texture2DMSArray:
                if (base_array_slice > 0 ||
                    base_array_slice + array_slice_count < desc.array_size)
                {
                    return false;
                }
                break;
            default:
                return true;
            }
            return true;
        }

        TextureSubresourceSet resolve(const TextureDesc& desc, bool bIsSingleMipLevel) const
        {
            TextureSubresourceSet ret(*this);
            if (bIsSingleMipLevel)
            {
                ret.mip_level_count = 1;
            }
            else
            {
                const uint32_t dwLastMipLevelPlusOne = std::min(
                    base_mip_level + mip_level_count,
                    desc.mip_levels
                );
                ret.mip_level_count = std::max(0u, dwLastMipLevelPlusOne - base_mip_level);
            }

            switch (desc.dimension)
            {
            case TextureDimension::Texture1DArray:
            case TextureDimension::Texture2DArray:
            case TextureDimension::TextureCube:
            case TextureDimension::TextureCubeArray:
            case TextureDimension::Texture2DMSArray:
                {
                    ret.base_array_slice = base_array_slice;
                    
                    // 使用 *this 和 desc 中较小的 ArraySlicesNum
                    const uint32_t dwLastArraySlicePlusOne = std::min(
                        base_array_slice + array_slice_count,
                        desc.array_size
                    );
                    ret.array_slice_count = std::max(0u, dwLastArraySlicePlusOne - base_array_slice);
                }
                break;
            default:
                ret.base_array_slice = 0;
                ret.array_slice_count = 1;
            }

            return ret;
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
		virtual uint32_t get_view_index(ViewType view_type, TextureSubresourceSet subresource, bool is_read_only_dsv = false) = 0;

        virtual bool bind_memory(HeapInterface* heap, uint64_t offset) = 0;
        virtual MemoryRequirements get_memory_requirements() = 0;

        virtual void* get_native_object() = 0;
        
		virtual ~TextureInterface() = default;
    };

    
    struct StagingTextureInterface : public ResourceInterface
    {
        virtual const TextureDesc& get_desc() const = 0;
        virtual void* map(const TextureSlice& texture_slize, CpuAccessMode cpu_access_mode, HANDLE fence_event, uint64_t* row_pitch) = 0;
        virtual void unmap() = 0;

        virtual void* get_native_object() = 0;

		virtual ~StagingTextureInterface() = default;
    };



    struct BufferDesc
    {
        std::string name;

        uint64_t byte_size = 0;
        uint32_t struct_stride = 0; // 若不为 0, 则表示该 Buffer 为 Structured Buffer
        
        Format format = Format::UNKNOWN; // 供 Typed Buffer 使用 
        
        // dynamic/upload 缓冲区，其内容仅存在于当前命令列表中
        bool is_volatile = false;

        bool can_have_uavs = false;
        bool is_vertex_buffer = false;
        bool is_index_buffer = false;
        bool is_constant_buffer = false;
        bool is_shader_binding_table = false;
        bool is_accel_struct_storage = false;

        bool is_virtual = false;
        
        ResourceStates initial_state = ResourceStates::Common;

        CpuAccessMode cpu_access = CpuAccessMode::None;

        uint32_t ma_versions = 0; // 仅在 Vulkan 的 volatile 缓冲区中有效，表示最大版本数，必须为非零值

        static BufferDesc create_constant(uint64_t byte_size, bool is_volatile = true, std::string name = "")
        {
            BufferDesc ret;
            ret.is_volatile = is_volatile;
            ret.is_constant_buffer = true;
            ret.byte_size = byte_size;
            ret.initial_state = ResourceStates::ConstantBuffer;
            ret.name = name;
            return ret;
        }

        static BufferDesc create_vertex(uint64_t byte_size, std::string name = "")
        {
            BufferDesc ret;
            ret.is_vertex_buffer = true;
            ret.byte_size = byte_size;
            ret.format = Format::RGB32_FLOAT;
            ret.initial_state = ResourceStates::VertexBuffer;
            ret.name = name;
            return ret;
        }

        static BufferDesc create_index(uint64_t byte_size, std::string name = "")
        {
            BufferDesc ret;
            ret.is_index_buffer = true;
            ret.byte_size = byte_size;
            ret.format = Format::R32_UINT;
            ret.initial_state = ResourceStates::IndexBuffer;
            ret.name = name;
            return ret;
        }

        static BufferDesc create_structured(uint64_t byte_size, uint32_t stride, bool used_in_compute_pass = false, std::string name = "")
        {
            BufferDesc ret;
			ret.initial_state = used_in_compute_pass ? ResourceStates::NonPixelShaderResource : ResourceStates::PixelShaderResource;
			ret.byte_size = byte_size;
            ret.struct_stride = stride;
            ret.name = name;
            return ret;
        }

        static BufferDesc create_rwstructured(uint64_t byte_size, uint32_t stride, std::string name = "")
        {
            BufferDesc ret;
            ret.initial_state = ResourceStates::UnorderedAccess;
            ret.can_have_uavs = true;
            ret.byte_size = byte_size;
            ret.struct_stride = stride;
            ret.name = name;
            return ret;
        }

        static BufferDesc create_read_back(uint64_t byte_size, std::string name = "")
        {
            BufferDesc ret;
            ret.initial_state = ResourceStates::CopyDest;
            ret.cpu_access = CpuAccessMode::Read;
            ret.byte_size = byte_size;
            ret.name = name;
            return ret;
        }

        static BufferDesc create_accel_struct(uint64_t byte_size, bool is_top_level, std::string name = "")
        {
            BufferDesc ret;
            ret.can_have_uavs = true;
            ret.byte_size = byte_size;
            ret.initial_state = is_top_level ? ResourceStates::AccelStructRead : ResourceStates::AccelStructBuildBlas;
            ret.is_accel_struct_storage = true;
            ret.name = name;
            return ret;
        }
    };

    struct BufferRange
    {
        uint64_t byte_offset = 0;
        uint64_t byte_size = 0;

        BufferRange resolve(const BufferDesc& desc) const
        {   
            BufferRange ret;

            // 若 byte_offset 超过了 desc 中所描述的总大小，则选择后者
            ret.byte_offset = std::min(byte_offset, desc.byte_size);

            // 若没有指定 byte_size, 则用总大小减去偏移
            // 若已经指定 byte_size, 则在 已经指定的值 和 用总大小减去偏移的结果 之间选择较小值
            if (byte_size == 0)
            {
                ret.byte_size = desc.byte_size - ret.byte_offset;
            }
            else
            {
                ret.byte_size = std::min(byte_size, desc.byte_size - ret.byte_offset);
            }

            return ret;
        }

        bool is_entire_buffer(const BufferDesc& desc) const
        {
            return  byte_offset == 0 && 
                    byte_size == static_cast<uint64_t>(-1) ||
                    byte_size == desc.byte_size;

        }
    };
    
    
    inline const BufferRange entire_buffer_range = BufferRange{ 0, ~0ull };


    

    struct BufferInterface : public ResourceInterface
    {
        virtual const BufferDesc& get_desc() const = 0;
        virtual void* map(CpuAccessMode cpu_access, HANDLE fence_event) = 0;
        virtual void unmap() = 0;
        virtual MemoryRequirements get_memory_requirements() = 0;
        virtual bool bind_memory(HeapInterface* heap, uint64_t offset) = 0;
        virtual uint32_t get_view_index(ViewType view_type, const BufferRange& range) = 0;

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


    namespace ray_tracing
    {
                // Forward declaration.
        struct ShaderTableInterface;


        enum class GeometryFlags : uint8_t
        {
            None = 0,
            Opaque = 1,
            NoDuplicateAnyHitInvocation = 2
        };
        ENUM_CLASS_FLAG_OPERATORS(GeometryFlags);

        enum class GeometryType : uint8_t
        {
            Triangle,
            BoundingBox
        };

        struct GeometryTriangles
        {
            std::shared_ptr<BufferInterface> index_buffer;
            std::shared_ptr<BufferInterface> vertex_buffer;
            Format index_format = Format::R32_UINT;
            Format vertex_format = Format::RGB32_FLOAT;
            uint64_t index_count = 0;
            uint64_t index_offset = 0;
            uint64_t vertex_count = 0;
            uint64_t vertex_offset = 0;
            uint32_t vertex_stride = 0;

            GeometryTriangles(
                const std::shared_ptr<BufferInterface>& in_index_buffer, 
                const std::shared_ptr<BufferInterface>& in_vertex_buffer,
                uint32_t in_vertex_stride,
                uint64_t in_index_offset = 0,
                uint64_t in_vertex_offset = 0
            ) :
                index_buffer(in_index_buffer), vertex_buffer(in_vertex_buffer),
                index_offset(in_index_offset), vertex_offset(in_vertex_offset),
                vertex_stride(in_vertex_stride)
            {
                const auto& index_buffer_desc = index_buffer->get_desc();
                const auto& vertex_buffer_desc = vertex_buffer->get_desc();
                index_format = index_buffer_desc.format;
                vertex_format = vertex_buffer_desc.format;
                index_count = index_buffer_desc.byte_size / sizeof(uint32_t);
                vertex_count = vertex_buffer_desc.byte_size / vertex_stride;

                assert(index_format == Format::R32_UINT);
            }
        };

        struct GeometryBoundingBoxes
        {
            std::shared_ptr<BufferInterface> buffer;		
            std::shared_ptr<BufferInterface> unused_buffer;
            uint64_t offset = 0;
            uint32_t count = 0;
            uint32_t stride = 0;
        };


        struct GeometryDesc
        {
            GeometryFlags flags = GeometryFlags::None;
            GeometryType type = GeometryType::Triangle;

            union 
            {
                GeometryTriangles triangles;
                GeometryBoundingBoxes aabbs;
            };

            bool use_transform = false;
            float3x4 affine_matrix;



            explicit GeometryDesc(const GeometryTriangles& in_triangles, GeometryFlags in_flags = GeometryFlags::None) :
                triangles(in_triangles), type(GeometryType::Triangle), flags(in_flags)
            {
            }

            explicit GeometryDesc(const GeometryBoundingBoxes& in_aabbs, GeometryFlags in_flags = GeometryFlags::None) :
                aabbs(in_aabbs), type(GeometryType::BoundingBox), flags(in_flags)
            {
            }

            GeometryDesc(const GeometryDesc& other) :
                flags(other.flags), 
                type(other.type),
                use_transform(other.use_transform),
                affine_matrix(other.affine_matrix)
            {
                if (type == GeometryType::Triangle) triangles = other.triangles;
                else if (type == GeometryType::BoundingBox) aabbs = other.aabbs;
            }

            GeometryDesc& operator=(const GeometryDesc& other)
            {
                if (this != &other) 
                {
                    flags = other.flags;
                    type = other.type;
                    
                    use_transform = other.use_transform;
                    affine_matrix = other.affine_matrix;

                    if (type == GeometryType::Triangle) triangles = other.triangles;
                    else if (type == GeometryType::BoundingBox) aabbs = other.aabbs;
                }

                return *this;
            }

            ~GeometryDesc()
            {
                if (type == GeometryType::Triangle)
                {
                    triangles.index_buffer.reset();
                    triangles.vertex_buffer.reset();
                }
                else if (type == GeometryType::BoundingBox)
                {
                    aabbs.buffer.reset();
                    aabbs.unused_buffer.reset();
                }
            }
        };

        enum class InstanceFlags : uint8_t
        {
            None							= 0,
            TriangleCullDisable				= 1,
            TriangleFrontCounterclockwise	= 1 << 1,
            ForceOpaque						= 1 << 2,
            ForceNonOpaque					= 1 << 3,
        };
        ENUM_CLASS_FLAG_OPERATORS(InstanceFlags);

        struct AccelStructInterface;

        struct InstanceDesc
        {
            float3x4 affine_matrix;

            uint32_t instance_id : 24 = 0;
            uint32_t instance_mask : 8 = 0;

            uint32_t instance_contibution_to_hit_group_index : 24 = 0;
            InstanceFlags flags : 8 = InstanceFlags::None;
            
            union 
            {
                AccelStructInterface* bottom_level_accel_struct = nullptr;
                size_t blas_device_address;
            };
        };

        enum class AccelStructBuildFlags : uint8_t
        {
            None				= 0,
            AllowUpdate			= 1,
            AllowCompaction		= 1 << 1,
            PreferFastTrace		= 1 << 2,
            PreferFastBuild		= 1 << 3,
            MinimizeMemory		= 1 << 4,
            PerformUpdate		= 1 << 5
        };
        ENUM_CLASS_FLAG_OPERATORS(AccelStructBuildFlags);

        struct AccelStructDesc
        {
            std::string name;

            bool is_virtual = false;
            bool is_top_level = false;
            uint64_t top_level_max_instance_num = 1;
            std::vector<GeometryDesc> bottom_level_geometry_descs;

            AccelStructBuildFlags flags = AccelStructBuildFlags::None;
        };

        
        struct AccelStructInterface : ResourceInterface
        {
            virtual const AccelStructDesc& get_desc() const = 0;
            virtual MemoryRequirements get_memory_requirements() = 0;
            virtual bool bind_memory(HeapInterface* heap, uint64_t offset = 0) = 0;
            virtual BufferInterface* get_buffer() const = 0;

            virtual ~AccelStructInterface() = default;
        };
    }


    inline uint32_t calculate_texture_subresource(
        uint32_t mip_level,
        uint32_t array_slice,
        uint32_t dwPlaneSlice,
        uint32_t mip_levels,
        uint32_t array_size
    )
    {
        return mip_level + (array_slice * mip_levels) + (dwPlaneSlice * mip_levels * array_size);
    }

    inline uint32_t calculate_texture_subresource(uint32_t mip_level, uint32_t array_slice, const TextureDesc& desc)
    {
        return mip_level + array_slice * desc.mip_levels;
    }
}


#endif