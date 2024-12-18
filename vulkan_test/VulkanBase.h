#ifndef VULKAN_BASE_H
#define VULKAN_BASE_H


#include "vulkan/vulkan_core.h"
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

#include "GlfwWindow.h"
#include "../source/core/math/common.h"

namespace fantasy
{
	class VulkanBase
	{
	public:
		bool run()
		{
			if (!_window.initialize_window(VkExtent2D{ 1280,720 })) return false;

			initialize();

			while (!_window.should_close())
			{
				glfwPollEvents();
				render_loop();
				_window.title_fps();
			}

			_window.terminate_window();
			destroy();
			return true;
		}

	private:
		bool initialize();
		bool destroy();
		bool render_loop();

		bool create_instance();
		bool check_validation_layer_support();
		bool enumerate_support_extension();

		bool create_debug_utils_messager();
		bool destroy_debug_utils_messager();

		bool pick_physical_device();
		bool find_queue_family(auto& physical_device);

		bool create_device();

	private:
		VkInstance _instance;
		std::vector<std::string> _instance_layers;
		std::vector<std::string> _instance_extensions;

		VkDebugUtilsMessengerEXT _debug_messager;
		std::vector<const char*> _validation_layers;
		std::vector<const char*> _extensions;
		VkDebugUtilsMessengerEXT _debug_callback;

		VkPhysicalDevice _physical_device = VK_NULL_HANDLE;
		VkSurfaceKHR _surface;

		struct 
		{
			uint32_t graphics_index = INVALID_SIZE_32;
			uint32_t present_index = INVALID_SIZE_32;
		} _queue_family_index;

		VkDevice _device;
		VkQueue _graphics_queue;
		VkQueue _present_queue;

		GlfwWindow _window;
	};
}








#endif