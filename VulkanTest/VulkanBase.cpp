#include "VulkanBase.h"
#include "../Source/Core/include/ComRoot.h"
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
		EnumerateSupportExtension();

#ifdef DEBUG
		ReturnIfFalse(CreateDebugUtilsMessenger());
#endif

		PickPhysicalDevice();
		return true;
	}


	BOOL FVulkanBase::Destroy()
	{
#ifdef DEBUG
		ReturnIfFalse(DestroyDebugUtilsMessenger());
#endif
		vkDestroyInstance(m_Instance, nullptr);
		vkDestroyDevice(m_Device, nullptr);
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
		vkEnumerateInstanceLayerProperties(&dwLayerCount, nullptr);
		std::vector<VkLayerProperties> Properties(dwLayerCount);
		vkEnumerateInstanceLayerProperties(&dwLayerCount, Properties.data());

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

	void FVulkanBase::EnumerateSupportExtension()
	{
		UINT32 dwExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &dwExtensionCount, nullptr);
		std::vector<VkExtensionProperties> ExtensionProperties(dwExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &dwExtensionCount, ExtensionProperties.data());
		for (UINT32 ix = 0; ix < dwExtensionCount; ++ix)
		{
			LOG_INFO(ExtensionProperties[ix].extensionName);
		}
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
		vkEnumeratePhysicalDevices(m_Instance, &dwPhysicalDeviceCount, nullptr);
		ReturnIfFalse(dwPhysicalDeviceCount != 0);
		std::vector<VkPhysicalDevice> PhysicalDevices;
		vkEnumeratePhysicalDevices(m_Instance, &dwPhysicalDeviceCount, PhysicalDevices.data());

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

	BOOL FVulkanBase::FindQueueFamily(const auto& crPhysicalDevice)
	{
		UINT32 dwQueueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(crPhysicalDevice, &dwQueueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> Properties;
		vkGetPhysicalDeviceQueueFamilyProperties(crPhysicalDevice, &dwQueueFamilyCount, Properties.data());

		for (const auto& crProperty : Properties)
		{
			if (crProperty.queueCount > 0 && crProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				return true;
			}
			m_dwQueueFamilyIndex++;
		}
		return false;
	}


	BOOL FVulkanBase::CreateDevice()
	{
		VkDeviceQueueCreateInfo QueueCreateInfo{};
		QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		QueueCreateInfo.queueFamilyIndex = m_dwQueueFamilyIndex;
		QueueCreateInfo.queueCount = 1;
		
		float fQueuePriority = 1.0f;
		QueueCreateInfo.pQueuePriorities = &fQueuePriority;

		VkPhysicalDeviceFeatures DeviceFeatures{};

		VkDeviceCreateInfo DeviceCreateInfo{};
		DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		DeviceCreateInfo.pQueueCreateInfos = &QueueCreateInfo;
		DeviceCreateInfo.queueCreateInfoCount = 1;
		DeviceCreateInfo.pEnabledFeatures = &DeviceFeatures;
		DeviceCreateInfo.enabledExtensionCount = 0;
#if DEBUG
		DeviceCreateInfo.ppEnabledLayerNames = m_ValidationLayers.data();
		DeviceCreateInfo.enabledLayerCount = static_cast<UINT32>(m_ValidationLayers.size());
#endif

		ReturnIfFalse(vkCreateDevice(m_PhysicalDevice, &DeviceCreateInfo, nullptr, &m_Device) == VK_SUCCESS);
		vkGetDeviceQueue(m_Device, m_dwQueueFamilyIndex, 0, &m_GraphicsQueue);
		
		return true;  
	}

}