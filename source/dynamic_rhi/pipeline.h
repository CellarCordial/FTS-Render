#ifndef RHI_PIPELINE_H
#define RHI_PIPELINE_H

#include "descriptor.h"
#include "frame_buffer.h"
#include "../core/math/matrix.h"

namespace fantasy
{
    struct VertexAttributeDesc
    {
        std::string name;
        Format format = Format::UNKNOWN;
        uint32_t array_size = 1;
        uint32_t buffer_index = 0;
        uint32_t offset = 0;
            
        uint32_t element_stride = 0;     // 对于大部分 api 来说, 该 stride 应设置为 sizeof(Vertex) 
        bool is_instanced = false;
    };

    using VertexAttributeDescArray = StackArray<VertexAttributeDesc, MAX_VERTEX_ATTRIBUTES>;


    struct InputLayoutInterface
    {
        virtual const VertexAttributeDesc& get_attribute_desc(uint32_t attribute_index) const = 0;
        virtual uint32_t get_attributes_num() const = 0;

		virtual ~InputLayoutInterface() = default;
    };
    

    enum class BlendFactor : uint8_t
    {
        Zero = 1,
        One = 2,
        SrcColor = 3,
        InvSrcColor = 4,
        SrcAlpha = 5,
        InvSrcAlpha = 6,
        DstAlpha  = 7,
        InvDstAlpha = 8,
        DstColor = 9,
        InvDstColor = 10,
        SrcAlphaSaturate = 11,
        ConstantColor = 14,
        InvConstantColor = 15,
        Src1Color = 16,
        InvSrc1Color = 17,
        Src1Alpha = 18,
        InvSrc1Alpha = 19,
    };

    enum class BlendOP : uint8_t
    {
        add             = 1,
        Subtract        = 2,
        ReverseSubtract = 3,
        min             = 4,
        max             = 5
    };

    enum class ColorMask : uint8_t
    {
        None    = 0,
        Red     = 1,
        Green   = 2,
        Blue    = 4,
        Alpha   = 8,
        All     = 0xF
    };
    ENUM_CLASS_FLAG_OPERATORS(ColorMask);


    struct BlendState
    {
        struct RenderTarget
        {
            bool        enable_blend = false;
            BlendFactor src_blend     = BlendFactor::One;
            BlendFactor dst_blend     = BlendFactor::Zero;
            BlendOP     blend_op      = BlendOP::add;
            
            BlendFactor src_blend_alpha = BlendFactor::One;
            BlendFactor dst_blend_alpha = BlendFactor::Zero;
            BlendOP     blend_op_alpha  = BlendOP::add;
            
            ColorMask color_write_mask = ColorMask::All;

            bool if_use_constant_color() const
            {
                return src_blend == BlendFactor::ConstantColor ||
                    src_blend == BlendFactor::InvConstantColor ||
                    dst_blend == BlendFactor::ConstantColor ||
                    dst_blend == BlendFactor::InvConstantColor ||
                    src_blend_alpha == BlendFactor::ConstantColor ||
                    src_blend_alpha == BlendFactor::InvConstantColor ||
                    dst_blend_alpha == BlendFactor::ConstantColor ||
                    dst_blend_alpha == BlendFactor::InvConstantColor;
            }
        };

        RenderTarget target_blends[MAX_RENDER_TARGETS];
        bool enable_alpha_to_converage = false;
        
        bool if_use_constant_color(uint32_t target_count) const
        {
            for (uint32_t ix = 0; ix < target_count; ++ix)
            {
                if (target_blends[ix].if_use_constant_color()) return true;
            }
            return false;
        }
    };

    enum class RasterFillMode : uint8_t
    {
        Solid,
        Wireframe,
    };

    enum class RasterCullMode : uint8_t
    {
        Back,
        Front,
        None
    };
    
    struct RasterState
    {
        RasterFillMode fill_mode = RasterFillMode::Solid;
        RasterCullMode cull_mode = RasterCullMode::Back;
        bool front_counter_clock_wise = false;

        bool enable_depth_clip = true;
        bool enable_scissor = false;
        bool enable_multi_sample = false;
        bool enable_anti_aliased_line = false;
        int32_t depth_bias = 0;
        float depth_bias_clamp = 0.0f;
        float slope_scale_depth_bias = 0.0f;

        uint8_t forced_sample_count = 0;
        bool enable_conservative_raster = false;
    };


    enum class StencilOP : uint8_t
    {
        Keep                = 1,
        Zero                = 2,
        Replace             = 3,
        IncrementAndClamp   = 4,
        DecrementAndClamp   = 5,
        Invert              = 6,
        IncrementAndWrap    = 7,
        DecrementAndWrap    = 8
    };

    enum class ComparisonFunc : uint8_t
    {
        Never           = 1,
        Less            = 2,
        Equal           = 3,
        LessOrEqual     = 4,
        Greater         = 5,
        NotEqual        = 6,
        GreaterOrEqual  = 7,
        Always          = 8
    };

    struct StencilOpDesc
    {
        StencilOP pass_op = StencilOP::Keep;
        StencilOP fail_op = StencilOP::Keep;
        StencilOP depth_fail_op = StencilOP::Keep;
        ComparisonFunc stencil_func = ComparisonFunc::Always;
    };

    struct DepthStencilState
    {
        bool            enable_depth_test = false;
        bool            enable_depth_write = false;
        ComparisonFunc depth_func = ComparisonFunc::Less;
        
        bool            enable_stencil = false;
        uint8_t           stencil_read_mask = 0xff;
        uint8_t           stencil_write_mask = 0xff;
        uint8_t           stencil_ref_value = 0;
        bool            dynamic_stencil_ref = false;
        StencilOpDesc   front_face_stencil;
        StencilOpDesc   back_face_stencil;
    };

    using ViewportArray = StackArray<Viewport, MAX_VIEWPORTS>;
    using RectArray = StackArray<Rect, MAX_VIEWPORTS>;

    struct ViewportState
    {
        ViewportArray viewports;
        RectArray rects;


        static ViewportState create_default_viewport(uint32_t width, uint32_t height)
        {
            ViewportState ret;
            ret.viewports.push_back(Viewport{ 0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), 0.0f, 1.0f });
            ret.rects.push_back(Rect{ 0, width, 0, height });
            return ret;
        }
    };


    enum class PrimitiveType : uint8_t
    {
        PointList,
        LineList,
        TriangleList,
        TriangleStrip,
        TriangleListWithAdjacency,
        TriangleStripWithAdjacency,
        PatchList
    };


    struct RenderState
    {
        BlendState blend_state;
        DepthStencilState depth_stencil_state;
        RasterState raster_state;
    };

    using BindingLayoutInterfaceArray = StackArray<BindingLayoutInterface*, MAX_BINDING_LAYOUTS>;
    
    struct GraphicsPipelineDesc
    {
        PrimitiveType PrimitiveType = PrimitiveType::TriangleList;
        uint32_t dwPatchControlPoints = 0;

        Shader* vertex_shader = nullptr;
        Shader* hull_shader = nullptr;
        Shader* domain_shader = nullptr;
        Shader* geometry_shader = nullptr;
        Shader* pixel_shader = nullptr;

        RenderState render_state;

        InputLayoutInterface* input_layout = nullptr;
        BindingLayoutInterfaceArray binding_layouts;
    };


    struct GraphicsPipelineInterface : public ResourceInterface
    {
        virtual const GraphicsPipelineDesc& get_desc() const = 0;
        virtual const FrameBufferInfo& get_frame_buffer_info() const = 0;
        virtual void* get_native_object() = 0;

		virtual ~GraphicsPipelineInterface() = default;
    };


    struct ComputePipelineDesc
    {
        Shader* compute_shader = nullptr;
        BindingLayoutInterfaceArray binding_layouts;
    };


    

    struct ComputePipelineInterface : public ResourceInterface
    {
        virtual const ComputePipelineDesc& get_desc() const = 0;
        virtual void* get_native_object() = 0;
        
		virtual ~ComputePipelineInterface() = default;
    };
    

    BlendState::RenderTarget Create_render_target_blend(BlendFactor src_blend, BlendFactor dst_blend);



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
            Format index_format;
            Format vertex_format;
            uint32_t index_count = 0;
            uint64_t index_offset = 0;
            uint32_t vertex_count = 0;
            uint64_t vertex_offset = 0;
            uint32_t vertex_stride = 0;
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
            Matrix3x4 affine_matrix;


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
            Matrix3x4 affine_matrix;

            uint32_t instance_id : 24 = 0;
            uint32_t instance_mask : 8 = 0;

            uint32_t instance_contibution_to_hit_group_index : 24 = 0;
            InstanceFlags flags : 8 = InstanceFlags::None;
            
            union 
            {
                AccelStructInterface* bottom_level_accel_struct = nullptr;
                size_t bias_device_address;
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
            uint64_t top_level_max_instance_num = 0;
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

    }

}
    




















#endif