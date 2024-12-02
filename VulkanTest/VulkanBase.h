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
#include "../Source/Math/include/Common.h"

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
		BOOL EnumerateSupportExtension();

		BOOL CreateDebugUtilsMessenger();
		BOOL DestroyDebugUtilsMessenger();

		BOOL PickPhysicalDevice();
		BOOL FindQueueFamily(auto& crPhysicalDevice);

		BOOL CreateDevice();

	private:
		VkInstance m_Instance;
		std::vector<std::string> m_InstanceLayers;
		std::vector<std::string> m_InstanceExtensions;

		VkDebugUtilsMessengerEXT m_DebugMessenger;
		std::vector<const CHAR*> m_ValidationLayers;
		std::vector<const CHAR*> m_Extensions;
		VkDebugUtilsMessengerEXT m_DebugCallback;

		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkSurfaceKHR m_Surface;

		struct 
		{
			UINT32 dwGraphicsIndex = INVALID_SIZE_32;
			UINT32 dwPresentIndex = INVALID_SIZE_32;
		} m_QueueFamilyIndex;

		VkDevice m_Device;
		VkQueue m_GraphicsQueue;
		VkQueue m_PresentQueue;

		FGlfwWindow m_GlfwWindow;
	};
}








#endif