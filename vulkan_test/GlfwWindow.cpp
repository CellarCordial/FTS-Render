#include "GlfwWindow.h"
#include "../source/core/tools/log.h"


namespace fantasy
{
	bool GlfwWindow::initialize_window(VkExtent2D size, bool full_screen, bool is_resizeable, bool limit_frame_rate)
	{
		ReturnIfFalse(glfwInit());
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, is_resizeable);

		_monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* modw = glfwGetVideoMode(_monitor);

		_window = full_screen ?
			glfwCreateWindow(modw->width, modw->height, "VulkanTest", _monitor, nullptr) :
			glfwCreateWindow(size.width, size.height, "VulkanTest", nullptr, nullptr);

		if (!_window)
		{
			LOG_ERROR("Initialize window failed.");
			glfwTerminate();
			return false;
		}

		return true;
	}

	void GlfwWindow::terminate_window()
	{
		glfwTerminate();
	}

	void GlfwWindow::title_fps()
	{
		static double time0 = glfwGetTime();
		static double time1;
		static double dt;
		static uint32_t dframe = -1;
		static std::stringstream info;
		time1 = glfwGetTime();
		dframe++;
		if ((dt = time1 - time0) >= 1)
		{
			info.precision(1);
			info << "VulkanTest" << "    " << std::fixed << dframe / dt << " FPS";
			glfwSetWindowTitle(_window, info.str().c_str());
			info.str("");
			time0 = time1;
			dframe = 0;
		}
	}
}