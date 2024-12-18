#ifndef RENDER_PASS_GLOBAL_SDF_H
#define RENDER_PASS_GLOBAL_SDF_H

#include "../../render_graph/render_graph.h"
#include "../../core/math/matrix.h"
#include "../../scene/scene.h"
#include <memory>

namespace fantasy
{ 
	namespace constant
	{
		struct GlobalSdfConstants
		{
			Matrix4x4 voxel_world_matrix;
			
			Vector3I voxel_offset;  
			float gi_max_distance = 500;
			
			uint32_t mesh_sdf_begin = 0;
			uint32_t mesh_sdf_end = 0;
		};
		
		struct ModelSdfData
		{
			Matrix4x4 coord_matrix;

			Vector3F sdf_lower;
			Vector3F sdf_upper;

			uint32_t mesh_sdf_index = 0;
		};
	}

	class GlobalSdfPass : public RenderPassInterface
	{
	public:
		GlobalSdfPass() { type = RenderPassType::Precompute | RenderPassType::Exclude; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

		bool finish_pass() override;


	private:
		bool PipelineSetup(DeviceInterface* device);
		bool ComputeStateSetup(DeviceInterface* device, RenderResourceCache* cache);

	private:
		uint32_t _model_sdf_data_default_count = 64;
		std::vector<constant::GlobalSdfConstants> _pass_constants;
		std::vector<constant::ModelSdfData> _model_sdf_datas;

		std::shared_ptr<BufferInterface> _model_sdf_data_buffer;
		std::shared_ptr<TextureInterface> _global_sdf_texture;
		std::vector<const DistanceField::MeshDistanceField*> _mesh_sdfs;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;
		std::unique_ptr<BindingLayoutInterface> _dynamic_binding_layout;
		std::unique_ptr<BindingLayoutInterface> _bindingless_layout;

		std::unique_ptr<BindingLayoutInterface> clear_pass_binding_layout;

		std::unique_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<Shader> _clear_pass_cs;
		std::unique_ptr<ComputePipelineInterface> _clear_pass_pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		std::unique_ptr<BindingSetInterface> _dynamic_binding_set;
		std::unique_ptr<BindlessSetInterface> _bindless_set;
		ComputeState _compute_state;

		std::unique_ptr<BindingSetInterface> _clear_pass_binding_set;
		ComputeState _clear_pass_compute_state;
	};
}





#endif