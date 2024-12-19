#ifndef RENDER_PASS_GBUFFER_H
#define RENDER_PASS_GBUFFER_H

#include "../../render_graph/render_pass.h"
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
            Matrix4x4 view_proj;
            Matrix4x4 view_matrix;
            Matrix4x4 prev_view_matrix;
        };

        struct GeometryConstant
        {
            Matrix4x4 world_matrix;
            Matrix4x4 inv_trans_world;

            Vector4F diffuse;
            Vector4F emissive;
            float roughness;
            float metallic;
            float occlusion;
        };
    }

    class GBufferPass : public RenderPassInterface
    {
    public:
        bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
        bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

        bool finish_pass() override;

    private:
        bool update(CommandListInterface* cmdlist, RenderResourceCache* cache);

    private:
		bool _update_gbuffer = false;
        std::vector<uint32_t> _indices;
        std::vector<Vertex> _vertices;

        constant::GBufferPassConstant _pass_constant;
        std::vector<constant::GeometryConstant> _geometry_constants;

		std::shared_ptr<BufferInterface> _vertex_buffer;
		std::shared_ptr<BufferInterface> _index_buffer;
		std::shared_ptr<BufferInterface> _material_factor;

        std::shared_ptr<TextureInterface> _world_space_position_texture;
        std::shared_ptr<TextureInterface> _world_space_normal_texture;
        std::shared_ptr<TextureInterface> _base_color_texture;
        std::shared_ptr<TextureInterface> _pbr_texture;
        std::shared_ptr<TextureInterface> _emissive_texture;
        std::shared_ptr<TextureInterface> _view_space_velocity_texture;
        std::shared_ptr<TextureInterface> _depth_texture;
		
		std::unique_ptr<BindingLayoutInterface> _binding_layout;
		std::unique_ptr<InputLayoutInterface> _input_layout;

		std::unique_ptr<Shader> _vs;
		std::unique_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;

		std::vector<std::unique_ptr<BindingSetInterface>> _binding_sets;
		GraphicsState _graphics_state;
        std::vector<DrawArguments> _draw_arguments;
    };

}








#endif