#ifndef RENDER_PASS_GUI_H
#define RENDER_PASS_GUI_H

#include "../render_graph/render_pass.h"
#include "gui_panel.h"
#include <memory>

namespace fantasy
{
	class GuiPass : public RenderPassInterface
	{
	public:
		GuiPass() { type = RenderPassType::Graphics; }
		~GuiPass() { gui::destroy(_device); }

		bool compile(DeviceInterface* device, RenderResourceCache* cache);
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache);

		void init(GLFWwindow* window, DeviceInterface* device) { gui::initialize(window, device); }

	private:
		DeviceInterface* _device = nullptr;
		std::shared_ptr<TextureInterface> _final_texture;
	};

}

#endif