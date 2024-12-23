#ifndef GLFW_WINDOW_H
#define GLFW_WINDOW_H


#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>

namespace fantasy
{
	class GlfwWindow
	{
	public:
		bool initialize_window(VkExtent2D size, bool full_screen = false, bool is_resizeable = true, bool limit_frame_rate = false);
		void terminate_window();

		void title_fps();
		bool should_close()
		{
			return glfwWindowShouldClose(_window);
		}

		GLFWwindow* get_window() const { return _window; }

	private:
		GLFWwindow* _window = nullptr;
		GLFWmonitor* _monitor = nullptr;
	};
}












#endif