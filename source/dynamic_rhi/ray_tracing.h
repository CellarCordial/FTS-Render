#ifndef DYNAMIC_RHI_RAY_TRACING_H
#define DYNAMIC_RHI_RAY_TRACING_H

#include "binding.h"
#include "pipeline.h"
#include "draw.h"
#include "../core/math/matrix.h"

namespace fantasy
{
    namespace ray_tracing
    {
        
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
                const std::shared_ptr<BufferInterface>& index_buffer_, 
                const std::shared_ptr<BufferInterface>& vertex_buffer_,
                uint32_t vertex_stride_,
                uint64_t index_offset_ = 0,
                uint64_t vertex_offset_ = 0
            ) :
                index_buffer(index_buffer_), vertex_buffer(vertex_buffer_),
                index_offset(index_offset_), vertex_offset(vertex_offset_),
                vertex_stride(vertex_stride_)
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



            explicit GeometryDesc(const GeometryTriangles& triangles_, GeometryFlags flags_ = GeometryFlags::None) :
                triangles(triangles_), type(GeometryType::Triangle), flags(flags_)
            {
            }

            explicit GeometryDesc(const GeometryBoundingBoxes& aabbs_, GeometryFlags flags_ = GeometryFlags::None) :
                aabbs(aabbs_), type(GeometryType::BoundingBox), flags(flags_)
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


        struct ShaderDesc
        {
            Shader* shader = nullptr;
            BindingLayoutInterface* binding_layout = nullptr;
        };

        struct HitGroupDesc
        {
            std::string export_name;
            Shader* closest_hit_shader = nullptr;
            Shader* any_hit_shader = nullptr;
            Shader* intersect_shader = nullptr;
            BindingLayoutInterface* binding_layout = nullptr;
            
            bool is_procedural_primitive = false;
        };

        struct PipelineDesc
        {
            std::vector<ShaderDesc> shader_descs;
            std::vector<HitGroupDesc> hit_group_descs;

            BindingLayoutInterfaceArray global_binding_layouts;
            uint32_t max_payload_size = 0;
            uint32_t max_attribute_size = sizeof(float) * 2;
            uint32_t max_recursion_depth = 1;
        };

        
        struct PipelineInterface : public ResourceInterface
        {
            virtual const PipelineDesc& get_desc() const = 0;
            virtual ShaderTableInterface* create_shader_table() = 0;
            virtual void* get_native_object() = 0;

            virtual ~PipelineInterface() = default;
        };

        
        struct ShaderTableInterface : public ResourceInterface
        {
            virtual void set_raygen_shader(const char* name, BindingSetInterface* binding_set = nullptr) = 0;

            virtual int32_t add_miss_shader(const char* name, BindingSetInterface* binding_set = nullptr) = 0;
            virtual int32_t add_hit_group(const char* name, BindingSetInterface* binding_set = nullptr) = 0;
            virtual int32_t add_callable_shader(const char* name, BindingSetInterface* binding_set = nullptr) = 0;
            
            virtual void clear_miss_shaders() = 0;
            virtual void clear_hit_groups() = 0;
            virtual void clear_callable_shaders() = 0;

            virtual PipelineInterface* get_pipeline() const = 0;
            
            virtual ~ShaderTableInterface() = default;
        };


        struct PipelineState
        {
            ShaderTableInterface* shader_table = nullptr;
            PipelineStateBindingSetArray binding_sets;
        };

        struct DispatchRaysArguments
        {
            uint32_t width = 1;	
            uint32_t height = 1;
            uint32_t depth = 1;
        };
    }
}


#endif