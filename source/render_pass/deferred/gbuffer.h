#ifndef RENDER_PASS_GBUFFER_H
#define RENDER_PASS_GBUFFER_H

#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_texture.h"
#include "../../core/math/matrix.h"
#include "../../scene/geometry.h"
#include <cstdint>
#include <memory>

namespace fantasy 
{
    namespace constant 
    {
        struct GBufferPassConstant
        {
            float4x4 reverse_z_view_proj;
            float4x4 view_matrix;

            uint32_t geometry_id;
            uint32_t vt_page_size = VT_PAGE_SIZE;
        };
    }

    class GBufferPass : public RenderPassInterface
    {
    public:
		GBufferPass() { type = RenderPassType::Graphics; }

        bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
        bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;
        bool finish_pass(RenderResourceCache* cache) override;

    private:
        bool update(CommandListInterface* cmdlist, RenderResourceCache* cache);

    private:
		bool _update_gbuffer = false;
        bool _resource_writed = false;
        std::vector<uint32_t> _indices;
        std::vector<Vertex> _vertices;

        constant::GBufferPassConstant _pass_constant;
        std::vector<GeometryConstantGpu> _geometry_constants;

        Image _black_image;
        std::shared_ptr<TextureInterface> _black_texture;

		std::shared_ptr<BufferInterface> _vertex_buffer;
		std::shared_ptr<BufferInterface> _index_buffer;
		std::shared_ptr<BufferInterface> _geometry_constant_buffer;

        std::shared_ptr<TextureInterface> _world_position_view_depth_texture;
        std::shared_ptr<TextureInterface> _geometry_uv_mip_id_texture;
        std::shared_ptr<TextureInterface> _world_space_normal_texture;
        std::shared_ptr<TextureInterface> _world_space_tangent_texture;
        std::shared_ptr<TextureInterface> _base_color_texture;
        std::shared_ptr<TextureInterface> _pbr_texture;
        std::shared_ptr<TextureInterface> _emissive_texture;
        std::shared_ptr<TextureInterface> _reverse_depth_texture;

        std::shared_ptr<SamplerInterface> _anisotropic_warp_sampler;
		
		std::shared_ptr<BindingLayoutInterface> _binding_layout;
		std::shared_ptr<InputLayoutInterface> _input_layout;

		std::shared_ptr<Shader> _vs;
		std::shared_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;

		std::vector<std::unique_ptr<BindingSetInterface>> _binding_sets;
		GraphicsState _graphics_state;
        std::vector<DrawArguments> _draw_arguments;
    };

}








#endif