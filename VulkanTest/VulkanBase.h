#ifndef VULKAN_BASE_H
#define VULKAN_BASE_H


#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

#include "GlfwWindow.h"

namespace FTS
{
	class FVulkanBase
	{
	public:
		BOOL Run()
		{
			if (!m_GlfwWindow.InitializeWindow(VkExtent2D{ 1280,720 })) return false;

			Initilize();

			while (!m_GlfwWindow.ShouldClose())
			{
				glfwPollEvents();
				RenderLoop();
				m_GlfwWindow.TitleFps();
			}

			m_GlfwWindow.TerminateWindow();
			Destroy();
			return true;
		}

	private:
		BOOL Initilize();
		BOOL Destroy();
		BOOL RenderLoop();

		BOOL CreateInstance();
		BOOL CheckValidationLayerSupport();
		void EnumerateSupportExtension();

		BOOL CreateDebugUtilsMessenger();
		BOOL DestroyDebugUtilsMessenger();

		BOOL PickPhysicalDevice();
		BOOL FindQueueFamily(const auto& crPhysicalDevice);

	private:
		VkInstance m_Instance;
		std::vector<std::string> m_InstanceLayers;
		std::vector<std::string> m_InstanceExtensions;

		VkDebugUtilsMessengerEXT m_DebugMessenger;
		VkSurfaceKHR m_Surface;
		std::vector<const CHAR*> m_ValidationLayers;
		std::vector<const CHAR*> m_Extensions;
		VkDebugUtilsMessengerEXT m_DebugCallback;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;

		FGlfwWindow m_GlfwWindow;
	};
}








#endif