#ifndef RENDER_PASS_GUI_H
#define RENDER_PASS_GUI_H

#include "../render_graph/render_graph.h"
#include "gui_panel.h"
#include <memory>

namespace fantasy
{
	namespace constant
	{
		struct GuiPassConstant
		{

		};
	}

	class GuiPass : public RenderPassInterface
	{
	public:
		GuiPass() { type = RenderPassType::Graphics; }
		~GuiPass() { gui::destroy(); }

		bool compile(DeviceInterface* device, RenderResourceCache* cache);
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache);

		void Init(GLFWwindow* window, DeviceInterface* device) { gui::initialize(window, device); }

	private:
		std::unique_ptr<FrameBufferInterface> _frame_buffer;
		std::shared_ptr<TextureInterface> _final_texture = nullptr;
	};

}

#endif