#include "GlfwWindow.h"
#include "../Source/Core/include/ComRoot.h"


namespace FTS
{
	BOOL FGlfwWindow::InitializeWindow(VkExtent2D Size, BOOL bFullScreen /* = false */, BOOL bIsResizeable /* = true */, BOOL bLimitFrameRate /* = false */)
	{
		ReturnIfFalse(glfwInit());
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, bIsResizeable);

		m_pMonitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* cpMode = glfwGetVideoMode(m_pMonitor);

		m_pWindow = bFullScreen ?
			glfwCreateWindow(cpMode->width, cpMode->height, "VulkanTest", m_pMonitor, nullptr) :
			glfwCreateWindow(Size.width, Size.height, "VulkanTest", nullptr, nullptr);

		if (!m_pWindow)
		{
			LOG_ERROR("Initialize window failed.");
			glfwTerminate();
			return false;
		}

		return true;
	}

	void FGlfwWindow::TerminateWindow()
	{
		glfwTerminate();
	}

	void FGlfwWindow::TitleFps()
	{
		static DOUBLE time0 = glfwGetTime();
		static DOUBLE time1;
		static DOUBLE dt;
		static UINT32 dframe = -1;
		static std::stringstream info;
		time1 = glfwGetTime();
		dframe++;
		if ((dt = time1 - time0) >= 1)
		{
			info.precision(1);
			info << "VulkanTest" << "    " << std::fixed << dframe / dt << " FPS";
			glfwSetWindowTitle(m_pWindow, info.str().c_str());
			info.str("");
			time0 = time1;
			dframe = 0;
		}
	}
}