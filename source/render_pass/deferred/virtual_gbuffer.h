#ifndef RENDER_VIRTUAL_GBUFFER_PASS_H
#define RENDER_VIRTUAL_GBUFFER_PASS_H
 
#include "../../render_graph/render_pass.h"
#include "../../scene/virtual_texture.h"
#include "../../core/math/matrix.h"
#include "../../scene/geometry.h"
#include <vector>
 
namespace fantasy
{
	namespace constant
	{
		struct VirtualGBufferPassConstant
		{
            float4x4 view_proj;

            float4x4 view_matrix;
            float4x4 prev_view_matrix;

            uint32_t view_mode;
            uint32_t mip_level;
            uint32_t vt_page_size;
            uint32_t client_width;
		};

		struct GeometryConstant
        {
            float4x4 world_matrix;
            float4x4 inv_trans_world;

            float4 diffuse;   
            float4 emissive;
            float roughness = 0.0f;
            float metallic = 0.0f;
            float occlusion = 0.0f;

			uint2 texture_resolution;
        };
	}

	class VirtualGBufferPass : public RenderPassInterface
	{
	public:
		VirtualGBufferPass() { type = RenderPassType::Graphics | RenderPassType::Feedback; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

        bool feedback(CommandListInterface* cmdlist, RenderResourceCache* cache) override;
        bool finish_pass() override;
	
	private:
		bool _resource_writed = false;
		constant::VirtualGBufferPassConstant _pass_constant;

		VTIndirectTexture _vt_indirect_texture;
		VTPhysicalTexture _vt_physical_texture;

		std::vector<Vertex> _cluster_vertices;
		std::vector<uint32_t> _cluster_triangles;
		std::vector<GeometryConstantGpu> _geometry_constants;

		std::shared_ptr<BufferInterface> _geometry_constant_buffer;
		std::shared_ptr<BufferInterface> _cluster_vertex_buffer;
		std::shared_ptr<BufferInterface> _cluster_triangle_buffer;
		std::shared_ptr<BufferInterface> _vt_page_info_buffer;
		std::shared_ptr<BufferInterface> _draw_indexed_indirect_arguments_buffer;

		std::shared_ptr<TextureInterface> _world_position_view_depth_texture;
		std::shared_ptr<TextureInterface> _view_space_velocity_texture;
		std::shared_ptr<TextureInterface> _tile_uv_texture;
		std::shared_ptr<TextureInterface> _world_space_normal_texture;
		std::shared_ptr<TextureInterface> _world_space_tangent_texture;
		std::shared_ptr<TextureInterface> _base_color_texture;
		std::shared_ptr<TextureInterface> _pbr_texture;
		std::shared_ptr<TextureInterface> _emmisive_texture;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;
		std::unique_ptr<InputLayoutInterface> _input_layout;

		std::unique_ptr<Shader> _vs;
		std::unique_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;

		BindingSetItemArray _binding_set_items;
		std::unique_ptr<BindingSetInterface> _binding_set;
		GraphicsState _graphics_state;
        DrawArguments _draw_arguments;
	};

}
 
#endif



