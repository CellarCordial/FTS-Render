#ifndef RENDER_PASS_RAY_TRACING_SHADOW_MAP_H
#define RENDER_PASS_RAY_TRACING_SHADOW_MAP_H
 
#include "../../render_graph/render_pass.h"
#include <cstdint>
#include <memory>
#include <vector>
 
namespace fantasy
{
	namespace constant
	{
		struct RayTracingShadowMapPassConstant
		{
            Vector3F sun_direction;
            float sun_angular_radius;
            
            uint32_t frame_index;
            Vector3I pad;
		};
	}

	class RayTracingShadowMapPass : public RenderPassInterface
	{
	public:
		RayTracingShadowMapPass() { type = RenderPassType::Graphics; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
        bool _update_vertex_buffer = false;
		constant::RayTracingShadowMapPassConstant _pass_constant;
        std::vector<ray_tracing::GeometryDesc> _ray_tracing_geometry_descs;
        std::vector<ray_tracing::InstanceDesc> _ray_tracing_instance_descs;

		std::shared_ptr<ray_tracing::AccelStructInterface> _top_level_accel_struct;
		std::shared_ptr<ray_tracing::AccelStructInterface> _bottom_level_accel_struct;

        std::shared_ptr<TextureInterface> _shadow_map_texture;
		
		std::unique_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _ray_gen_shader;
		std::unique_ptr<Shader> _miss_shader;

		std::unique_ptr<ray_tracing::PipelineInterface> _pipeline;
		std::unique_ptr<ray_tracing::ShaderTableInterface> _shader_table;

		std::unique_ptr<BindingSetInterface> _binding_set;
		ray_tracing::PipelineState _ray_tracing_state;
        ray_tracing::DispatchRaysArguments _dispatch_rays_arguments;
	};
}
 
#endif
