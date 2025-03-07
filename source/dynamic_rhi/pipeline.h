﻿#ifndef RHI_PIPELINE_H
#define RHI_PIPELINE_H

#include "binding.h"
#include "resource.h"
#include <memory>

namespace fantasy
{
    struct VertexAttributeDesc
    {
        std::string name;
        Format format = Format::UNKNOWN;
        uint32_t offset = 0;
        uint32_t element_stride = 0; 
        
        uint32_t array_size = 1;
        uint32_t buffer_slot = 0;
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
        Add             = 1,
        Subtract        = 2,
        ReverseSubtract = 3,
        Min             = 4,
        Max             = 5
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
            BlendOP     blend_op      = BlendOP::Add;
            
            BlendFactor src_blend_alpha = BlendFactor::One;
            BlendFactor dst_blend_alpha = BlendFactor::Zero;
            BlendOP     blend_op_alpha  = BlendOP::Add;
            
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

    struct StencilOPDesc
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
        uint8_t         stencil_read_mask = 0xff;
        uint8_t         stencil_write_mask = 0xff;
        uint8_t         stencil_ref_value = 0;
        bool            dynamic_stencil_ref = false;
        StencilOPDesc   front_face_stencil;
        StencilOPDesc   back_face_stencil;
    };

    using ViewportArray = StackArray<Viewport, MAX_VIEWPORTS>;
    using RectArray = StackArray<Rect, MAX_VIEWPORTS>;

    struct ViewportState
    {
        ViewportArray viewports;
        RectArray scissor_rects;


        static ViewportState create_default_viewport(uint32_t width, uint32_t height)
        {
            ViewportState ret;
            ret.viewports.push_back(Viewport{ 0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), 0.0f, 1.0f });
            ret.scissor_rects.push_back(Rect{ 0, width, 0, height });
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

    using BindingLayoutInterfaceArray = StackArray<std::shared_ptr<BindingLayoutInterface>, MAX_BINDING_LAYOUTS>;
    
    struct GraphicsPipelineDesc
    {
        std::shared_ptr<InputLayoutInterface> input_layout;
        PrimitiveType primitive_type = PrimitiveType::TriangleList;
        uint32_t patch_control_points = 0;

        std::shared_ptr<Shader> vertex_shader;
        std::shared_ptr<Shader> hull_shader;
        std::shared_ptr<Shader> domain_shader;
        std::shared_ptr<Shader> geometry_shader;
        std::shared_ptr<Shader> pixel_shader;

        RenderState render_state;
        BindingLayoutInterfaceArray binding_layouts;
    };


    struct GraphicsPipelineInterface : public ResourceInterface
    {
        virtual const GraphicsPipelineDesc& get_desc() const = 0;
        virtual void* get_native_object() = 0;

		virtual ~GraphicsPipelineInterface() = default;
    };


    struct ComputePipelineDesc
    {
        std::shared_ptr<Shader> compute_shader;
        BindingLayoutInterfaceArray binding_layouts;
    };

    struct ComputePipelineInterface : public ResourceInterface
    {
        virtual const ComputePipelineDesc& get_desc() const = 0;
        virtual void* get_native_object() = 0;
        
		virtual ~ComputePipelineInterface() = default;
    };
    

    BlendState::RenderTarget create_render_target_blend(BlendFactor src_blend, BlendFactor dst_blend);
}
    




















#endif