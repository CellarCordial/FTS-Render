#ifndef RENDER_PASS_H
#define RENDER_PASS_H
 
#include "../../render_graph/render_pass.h"
#include "../../core/math/matrix.h"
#include "../../scene/geometry.h"
 
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
		VirtualGBufferPass() { type = RenderPassType::Graphics; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		constant::VirtualGBufferPassConstant _pass_constant;

		std::shared_ptr<BufferInterface> _buffer;
		std::shared_ptr<TextureInterface> _texture;
		
		std::unique_ptr<BindingLayoutInterface> _binding_layout;
		std::unique_ptr<InputLayoutInterface> _input_layout;

		std::unique_ptr<Shader> _vs;
		std::unique_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		GraphicsState _graphics_state;
        DrawArguments _draw_arguments;
	};

}
 
#endif



