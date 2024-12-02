#include "VulkanBase.h"
#include "../Source/Core/include/ComRoot.h"
#include "glfw3.h"
#include <minwindef.h>
#include <set>
#include "vulkan/vulkan_core.h"

namespace FTS
{
	VkBool32 DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT MessageTypes,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData
	)
	{
		LOG_ERROR(pCallbackData->pMessage);
		return VK_FALSE;
	}

	BOOL FVulkanBase::Initilize()
	{
		ReturnIfFalse(CreateInstance());
		ReturnIfFalse(EnumerateSupportExtension());

#ifdef DEBUG
		ReturnIfFalse(CreateDebugUtilsMessenger());
#endif
		ReturnIfFalse(glfwCreateWindowSurface(m_Instance, m_GlfwWindow.GetWindow(), nullptr, &m_Surface) == VK_SUCCESS);
		PickPhysicalDevice();
		CreateDevice();
		return true;
	}


	BOOL FVulkanBase::Destroy()
	{
#ifdef DEBUG
		ReturnIfFalse(DestroyDebugUtilsMessenger());
#endif
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		vkDestroyDevice(m_Device, nullptr);
		vkDestroyInstance(m_Instance, nullptr);
		return true;
	}

	BOOL FVulkanBase::RenderLoop()
	{
		return true;
	}

	BOOL FVulkanBase::CreateInstance()
	{
		VkApplicationInfo AppInfo{};
		AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		AppInfo.pApplicationName = "Vulkan Test";
		AppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		AppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		AppInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

		m_ValidationLayers.push_back("VK_LAYER_KHRONOS_validation");

		VkInstanceCreateInfo InstanceInfo{};
		InstanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		InstanceInfo.pApplicationInfo = &AppInfo;

		UINT32 dwGlfwExtensionCount = 0;
		const CHAR** cppcGlfwExtensions = glfwGetRequiredInstanceExtensions(&dwGlfwExtensionCount);
		m_Extensions.insert(m_Extensions.end(), cppcGlfwExtensions, cppcGlfwExtensions + dwGlfwExtensionCount);
		m_Extensions.push_back("VK KHR SWAPCHAIN EXTENSION NAME");

#ifdef DEBUG
		m_Extensions.push_back("VK_EXT_debug_utils");

		ReturnIfFalse(CheckValidationLayerSupport());
		InstanceInfo.enabledLayerCount = static_cast<UINT32>(m_ValidationLayers.size());
		InstanceInfo.ppEnabledLayerNames = m_ValidationLayers.data();
#endif

		InstanceInfo.enabledExtensionCount = static_cast<UINT32>(m_Extensions.size());
		InstanceInfo.ppEnabledExtensionNames = m_Extensions.data();

		return vkCreateInstance(&InstanceInfo, nullptr, &m_Instance) == VK_SUCCESS;
	}

	BOOL FVulkanBase::CheckValidationLayerSupport()
	{
		UINT32 dwLayerCount = 0;
		ReturnIfFalse(vkEnumerateInstanceLayerProperties(&dwLayerCount, nullptr) == VK_SUCCESS);
		std::vector<VkLayerProperties> Properties(dwLayerCount);
		ReturnIfFalse(vkEnumerateInstanceLayerProperties(&dwLayerCount, Properties.data()) == VK_SUCCESS);

		for (const auto& crLayer : m_ValidationLayers)
		{
			BOOL bFound = false;
			for (const auto& crProperty : Properties)
			{
				if (strcmp(crLayer, crProperty.layerName) == 0)
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				LOG_ERROR("Extension " + std::string(crLayer) + " is not support.");
				return false;
			}
		}
		return true;
	}

	BOOL FVulkanBase::EnumerateSupportExtension()
	{
		UINT32 dwExtensionCount = 0;
		ReturnIfFalse(vkEnumerateInstanceExtensionProperties(nullptr, &dwExtensionCount, nullptr) == VK_SUCCESS);
		std::vector<VkExtensionProperties> ExtensionProperties(dwExtensionCount);
		ReturnIfFalse(vkEnumerateInstanceExtensionProperties(nullptr, &dwExtensionCount, ExtensionProperties.data()) == VK_SUCCESS);
		for (UINT32 ix = 0; ix < dwExtensionCount; ++ix)
		{
			LOG_INFO(ExtensionProperties[ix].extensionName);
		}
		return true;
	}

	BOOL FVulkanBase::CreateDebugUtilsMessenger()
	{
		VkDebugUtilsMessengerCreateInfoEXT DebugUtilInfo{};
		DebugUtilInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		DebugUtilInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		DebugUtilInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		DebugUtilInfo.pfnUserCallback = DebugCallback;

		auto Func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT");
		if (Func)
		{
			Func(m_Instance, &DebugUtilInfo, nullptr, &m_DebugCallback);
		}
		else
		{
			LOG_ERROR("Get vkCreateDebugUtilsMessengerEXT process address failed.");
			return false;
		}
		return true;
	}

	BOOL FVulkanBase::DestroyDebugUtilsMessenger()
	{
		auto Func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT");
		if (Func)
		{
			Func(m_Instance, m_DebugCallback, nullptr);
		}
		else
		{
			LOG_ERROR("Get vkDestroyDebugUtilsMessengerEXT process address failed.");
			return false; 
		}
		return true;
	}

	BOOL FVulkanBase::PickPhysicalDevice()
	{
		UINT32 dwPhysicalDeviceCount = 0;
		ReturnIfFalse(vkEnumeratePhysicalDevices(m_Instance, &dwPhysicalDeviceCount, nullptr) == VK_SUCCESS);
		ReturnIfFalse(dwPhysicalDeviceCount != 0);
		std::vector<VkPhysicalDevice> PhysicalDevices(dwPhysicalDeviceCount);
		ReturnIfFalse(vkEnumeratePhysicalDevices(m_Instance, &dwPhysicalDeviceCount, PhysicalDevices.data()) == VK_SUCCESS);

		for (const auto& crDevice : PhysicalDevices)
		{
			VkPhysicalDeviceProperties Properties;
			VkPhysicalDeviceFeatures Features;
			vkGetPhysicalDeviceProperties(crDevice, &Properties);
			vkGetPhysicalDeviceFeatures(crDevice, &Features);

			if (
				Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && 
				Features.geometryShader && 
				FindQueueFamily(crDevice)
			)
			{
				m_PhysicalDevice = crDevice;
				break;
			}
		}

		return true;
	}

	BOOL FVulkanBase::FindQueueFamily(auto& crPhysicalDevice)
	{
		UINT32 dwQueueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(crPhysicalDevice, &dwQueueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> Properties(dwQueueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(crPhysicalDevice, &dwQueueFamilyCount, Properties.data());

		for (UINT32 ix = 0; ix < Properties.size(); ++ix)
		{
			VkBool32 bPresentSupport = false;
			if (vkGetPhysicalDeviceSurfaceSupportKHR(crPhysicalDevice , ix, m_Surface, &bPresentSupport) == VK_SUCCESS && bPresentSupport) 
			{
				m_QueueFamilyIndex.dwPresentIndex = ix;
			}
			if (Properties[ix].queueCount > 0 && Properties[ix].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				m_QueueFamilyIndex.dwGraphicsIndex = ix;
			}

			if (m_QueueFamilyIndex.dwGraphicsIndex != INVALID_SIZE_32 && m_QueueFamilyIndex.dwGraphicsIndex != INVALID_SIZE_32) break;
		}
		return true;
	}


	BOOL FVulkanBase::CreateDevice()
	{
		std::set<UINT32> QueueFamilyIndices = { m_QueueFamilyIndex.dwGraphicsIndex, m_QueueFamilyIndex.dwPresentIndex };
		std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;

		FLOAT fQueuePriority = 1.0f;
		for (UINT32 ix : QueueFamilyIndices)
		{
			auto& rQueueCreateInfo = QueueCreateInfos.emplace_back();
			rQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			rQueueCreateInfo.queueFamilyIndex = ix;
			rQueueCreateInfo.queueCount = 1;
			rQueueCreateInfo.pQueuePriorities = &fQueuePriority;
		}


		VkPhysicalDeviceFeatures DeviceFeatures{};

		VkDeviceCreateInfo DeviceCreateInfo{};
		DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		DeviceCreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
		DeviceCreateInfo.queueCreateInfoCount = 1;
		DeviceCreateInfo.pEnabledFeatures = &DeviceFeatures;
		DeviceCreateInfo.enabledExtensionCount = 0;
#if DEBUG
		DeviceCreateInfo.ppEnabledLayerNames = m_ValidationLayers.data();
		DeviceCreateInfo.enabledLayerCount = static_cast<UINT32>(m_ValidationLayers.size());
#endif

		ReturnIfFalse(vkCreateDevice(m_PhysicalDevice, &DeviceCreateInfo, nullptr, &m_Device) == VK_SUCCESS);
		vkGetDeviceQueue(m_Device, m_QueueFamilyIndex.dwGraphicsIndex, 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, m_QueueFamilyIndex.dwPresentIndex, 0, &m_PresentQueue);
		
		return true;  
	}

}