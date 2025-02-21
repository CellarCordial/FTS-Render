#ifndef RENDER_VIRTUAL_GBUFFER_PASS_H
#define RENDER_VIRTUAL_GBUFFER_PASS_H
 
#include "../../render_graph/render_pass.h"
#include "../../core/math/matrix.h"
#include "../../scene/geometry.h"
#include "../../scene/virtual_texture.h"
#include <memory>
#include <vector>
 
namespace fantasy
{
	namespace constant
	{
		enum class VirtualGBufferViewMode : uint32_t
		{
			Triangle,
			Cluster,
			ClusterGroup,
			ClusterMip
		};

		struct VirtualGBufferPassConstant
		{
			float4x4 reverse_z_view_proj;

			float4x4 view_matrix;
			float4x4 prev_view_matrix;
			
			float4x4 shadow_view_proj;
		
			uint32_t view_mode = 0;
			uint32_t vt_page_size = VT_PAGE_SIZE;
			uint32_t virtual_shadow_resolution = VIRTUAL_SHADOW_RESOLUTION;
			uint32_t virtual_shadow_page_size = VIRTUAL_SHADOW_PAGE_SIZE;
			
			float3 camera_position;
			uint32_t client_width = CLIENT_WIDTH;
		};
	}

	class VirtualGBufferPass : public RenderPassInterface
	{
	public:
		VirtualGBufferPass() { type = RenderPassType::Graphics; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

        bool finish_pass() override;
	
	private:
		bool _resource_writed = false;
		constant::VirtualGBufferPassConstant _pass_constant;

		std::vector<Vertex> _cluster_vertices;
		std::vector<uint32_t> _cluster_triangles;
		std::vector<GeometryConstantGpu> _geometry_constants;

		std::shared_ptr<BufferInterface> _pass_constant_buffer;
		std::shared_ptr<BufferInterface> _geometry_constant_buffer;
		std::shared_ptr<BufferInterface> _cluster_vertex_buffer;
		std::shared_ptr<BufferInterface> _cluster_triangle_buffer;
		std::shared_ptr<BufferInterface> _draw_indexed_indirect_arguments_buffer;
		std::shared_ptr<BufferInterface> _vt_page_info_buffer;
		std::shared_ptr<BufferInterface> _virtual_shadow_page_buffer;
		std::shared_ptr<BufferInterface> _vt_page_info_read_back_buffer;
		std::shared_ptr<BufferInterface> _virtual_shadow_page_read_back_buffer;


		std::shared_ptr<TextureInterface> _world_position_view_depth_texture;
		std::shared_ptr<TextureInterface> _view_space_velocity_texture;
		std::shared_ptr<TextureInterface> _tile_uv_texture;
		std::shared_ptr<TextureInterface> _world_space_normal_texture;
		std::shared_ptr<TextureInterface> _world_space_tangent_texture;
		std::shared_ptr<TextureInterface> _base_color_texture;
		std::shared_ptr<TextureInterface> _pbr_texture;
		std::shared_ptr<TextureInterface> _emissive_texture;
        std::shared_ptr<TextureInterface> _reverse_depth_texture;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;
		std::shared_ptr<InputLayoutInterface> _input_layout;

		std::shared_ptr<Shader> _vs;
		std::shared_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;

		BindingSetItemArray _binding_set_items;
		std::unique_ptr<BindingSetInterface> _binding_set;
		GraphicsState _graphics_state;
        DrawArguments _draw_arguments;
	};

}
 
#endif



