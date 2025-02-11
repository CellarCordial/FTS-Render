#ifndef RENDER_PASS_CULLING_CLUSTERED_LIGHT_H
#define RENDER_PASS_CULLING_CLUSTERED_LIGHT_H

#include "../../render_graph/render_pass.h"
#include "../../scene/light.h"
#include <cstdint>
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct ClusterLightCullingPassConstant
		{
			float4x4 view_proj;

			uint3 divide_count;
			float half_fov_y = 0.0f;
			float near_z = 0.0f;

			uint32_t point_light_count = 0;
			uint32_t spot_light_count = 0;
		};
	}


	class ClusterLightCullingPass : public RenderPassInterface
	{
	public:
		ClusterLightCullingPass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

        bool finish_pass() override;

		static const uint32_t max_cluster_light_num = 100u;
	
	private:
		bool _resource_writed = false;
		uint2 _tile_count = { CLIENT_WIDTH / 64, CLIENT_HEIGHT / 32 };
		constant::ClusterLightCullingPassConstant _pass_constant;

		std::vector<PointLight> _point_lights;
		std::vector<SpotLight> _spot_lights;

		std::shared_ptr<BufferInterface> _point_light_buffer;
		std::shared_ptr<BufferInterface> _spot_light_buffer;
		std::shared_ptr<BufferInterface> _light_index_buffer;
		std::shared_ptr<BufferInterface> _light_cluster_buffer;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		BindingSetItemArray _binding_set_items;
		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};
}
#endif

