#ifndef RENDER_PASS_ATMOSPHERE_DEBUG_H
#define RENDER_PASS_ATMOSPHERE_DEBUG_H

#include "../render_graph/render_graph.h"
#include "../core/math/matrix.h"
#include "../render_pass/atmosphere/multi_scattering_lut.h"
#include "../render_pass/atmosphere/transmittance_lut.h"
#include "../render_pass/atmosphere/aerial_lut.h"
#include "../render_pass/atmosphere/sun_disk.h"
#include "../render_pass/atmosphere/sky_lut.h"
#include "../render_pass/atmosphere/sky.h"
#include "../render_pass/shadow/shadow_map.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct AtmosphereDebugPassConstant0
		{
			Matrix4x4 view_proj;
			Matrix4x4 world_matrix;
		};

		struct AtmosphereDebugPassConstant1
		{
			Vector3F sun_direction;
			float sun_theta = 0.0f;

			Vector3F sun_radiance;
			float max_aerial_distance = 2000.0f;

			Vector3F camera_position;
			float world_scale = 0.0f;

			Matrix4x4 shadow_view_proj;

			Vector2F jitter_factor;
			Vector2F blue_noise_uv_factor;

			Vector3F ground_albedo;
			float pad = 0.0f;
		};
	}


	class AtmosphereDebugPass : public RenderPassInterface
	{
	public:
		AtmosphereDebugPass() { type = RenderPassType::Graphics; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache);
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache);

		friend class AtmosphereTest;

	private:
		bool _writed_resource = false;
		float _jitter_radius = 1.0f;
		constant::AtmosphereDebugPassConstant0 _pass_constant0;
		constant::AtmosphereDebugPassConstant1 _pass_constant1;
		Image _blue_noise_image;

		std::shared_ptr<TextureInterface> _aerial_lut_texture;		
		std::shared_ptr<TextureInterface> _transmittance_texture;
		std::shared_ptr<TextureInterface> _multi_scattering_texture;
		std::shared_ptr<TextureInterface> _final_texture;

		std::shared_ptr<BufferInterface> _pass_constant1_buffer;
		std::shared_ptr<TextureInterface> _blue_noise_texture;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;
		std::unique_ptr<InputLayoutInterface> _input_layout;

		std::unique_ptr<Shader> _vs;
		std::unique_ptr<Shader> _ps;

		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::unique_ptr<GraphicsPipelineInterface> _pipeline;

		std::unique_ptr<BindingSetInterface> _binding_set;
		FGraphicsState _graphics_state;

		DrawArguments* _draw_arguments = nullptr;
		uint64_t _draw_argument_count = 0;
	};

	class AtmosphereTest
	{
	public:
		bool setup(RenderGraph* render_graph);

		RenderPassInterface* get_last_pass() { return _atmosphere_debug_pass.get(); }

	private:
		float _world_scale = 200.0f;
		Vector3F _ground_albedo = { 0.3f, 0.3f, 0.3f };

		std::shared_ptr<TransmittanceLUTPass> _transmittance_lut_pass;		
		std::shared_ptr<MultiScatteringLUTPass> _multi_scattering_lut_pass;
		std::shared_ptr<ShadowMapPass> _shadow_map_pass;
		std::shared_ptr<SkyLUTPass> _sky_lut_pass;
		std::shared_ptr<FAerialLUTPass> _aerial_lut_pass;
		std::shared_ptr<SkyPass> _sky_pass;
		std::shared_ptr<SunDiskPass> _sun_disk_pass;
		std::shared_ptr<AtmosphereDebugPass> _atmosphere_debug_pass;
	};
}












#endif