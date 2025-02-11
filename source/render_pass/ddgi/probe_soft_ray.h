#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "../../render_graph/render_pass.h"
#include "../sdf/global_sdf_info.h"
#include "ddgi_volume.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct ProbeSoftRayPassConstant
		{
			DDGIVolumeDataGpu volume_data;
			GlobalSDFInfo sdf_data;    

			float4x4 random_orientation;
			
			float sdf_voxel_size = 0.0f;
			float sdf_chunk_size = 0.0f;
			float max_gi_distance = 0.0f;
			uint32_t surface_texture_resolution = 0;
			uint32_t surface_atlas_resolution = 0;
		};
	}

	class ProbeSoftRayPass : public RenderPassInterface
	{
	public:
		ProbeSoftRayPass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		bool _resource_writed = false;
		constant::ProbeSoftRayPassConstant _pass_constant;

		DDGIVolumeDataGpu _ddgi_volume_data;

		std::shared_ptr<TextureInterface> _ddgi_radiance_texture;
		std::shared_ptr<TextureInterface> _ddgi_direction_distance_texture;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif




