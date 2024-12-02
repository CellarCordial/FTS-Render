#ifndef GLFW_WINDOW_H
#define GLFW_WINDOW_H


#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "../Source/Core/include/SysCall.h"

namespace FTS
{
	class FGlfwWindow
	{
	public:
		BOOL InitializeWindow(VkExtent2D Size, BOOL bFullScreen = false, BOOL bIsResizeable = true, BOOL bLimitFrameRate = false);
		void TerminateWindow();

		void TitleFps();
		BOOL ShouldClose()
		{
			return glfwWindowShouldClose(m_pWindow);
		}

		GLFWwindow* GetWindow() const { return m_pWindow; }

	private:
		GLFWwindow* m_pWindow = nullptr;
		GLFWmonitor* m_pMonitor = nullptr;
	};
}












#endif