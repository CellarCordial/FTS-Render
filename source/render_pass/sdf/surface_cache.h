#ifndef RENDER_PASS_SDF_SURFACE_CACHE_H
#define RENDER_PASS_SDF_SURFACE_CACHE_H
 
#include "../../render_graph/render_pass.h"
#include "../../core/math/matrix.h"
#include <memory>
 
namespace fantasy
{
	namespace constant
	{
		struct SurfaceCachePassConstant
		{
            float4x4 view_proj;
            float4x4 world_matrix[6];
		};
	}

	class SurfaceCachePass : public RenderPassInterface
	{
	public:
		SurfaceCachePass() { type = RenderPassType::Precompute | RenderPassType::Exclude; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

        bool finish_pass() override;

	private:
		constant::SurfaceCachePassConstant _pass_constant;

        Entity* _model_entity = nullptr;

        std::shared_ptr<BufferInterface> _vertex_buffer;
        std::shared_ptr<BufferInterface> _index_buffer;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;
		std::unique_ptr<InputLayoutInterface> _input_layout;

		std::unique_ptr<Shader> _vs;
		std::unique_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;

        GraphicsPipelineDesc _pipeline_desc;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;

        BindingSetItemArray _binding_set_items;
		std::unique_ptr<BindingSetInterface> _binding_set;
		GraphicsState _graphics_state;
        DrawArguments _draw_arguments;
	};

}






#endif