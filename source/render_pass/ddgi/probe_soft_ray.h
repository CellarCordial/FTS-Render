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
		struct Constant
		{
			DDGIVolumeDataGpu volume_data;
			GlobalSDFInfo sdf_data;    

			float4x4 random_orientation;
			
			float sdf_voxel_size;
			float sdf_chunk_size;
			float max_gi_distance;
			uint32_t surface_texture_resolution;
			uint32_t surface_atlas_resolution;
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
		constant::Constant _pass_constant;

		std::shared_ptr<BufferInterface> _buffer;
		std::shared_ptr<TextureInterface> _texture;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif




