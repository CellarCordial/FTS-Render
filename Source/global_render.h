#ifndef GLOBAL_RENDER_H
#define GLOBAL_RENDER_H


#include <memory>
#include <wrl.h>
#include <dxgi.h>
#include "core/tools/timer.h"
#include "core/tools/ecs.h"
#include "gui/gui_pass.h"
#include "render_graph/render_graph.h"
#include "render_pass/atomsphere/atmosphere_debug.h"
#include "render_pass/sdf/sdf_debug.h"
#include "scene/camera.h"

namespace fantasy
{
	class GlobalRender
	{
	public:
		~GlobalRender();

		bool Init();
		bool run();

	private:
		bool D3D12Init();
		bool VulkanInit();

		bool create_samplers();

	private:
		Timer _timer;
		World _world;
		Camera* _camera = nullptr;
		GLFWwindow* _window = nullptr;

		// D3D12.
		Microsoft::WRL::ComPtr<IDXGISwapChain> _swap_chain;
		uint32_t _current_back_buffer_index = 0;

		std::shared_ptr<DeviceInterface> _device;
		std::shared_ptr<TextureInterface> _final_texture;
		std::shared_ptr<TextureInterface> _back_buffers[NUM_FRAMES_IN_FLIGHT];

		std::unique_ptr<RenderGraph> _render_graph;
		std::shared_ptr<GuiPass> _gui_pass;
		AtmosphereDebugRender _atmosphere_debug_render;
		SdfDebugRender _sdf_debug_render;

	};
}





#endif