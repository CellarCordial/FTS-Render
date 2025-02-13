#include "vulkan/vulkan_core.h"
#include <cstdint>
#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>

#include "test_base.h"

#include "../core/tools/log.h"
#include "../dynamic_rhi/dynamic_rhi.h"
#include "../dynamic_rhi/resource.h"
#include "../shader/shader_compiler.h"
#include "../core/parallel/parallel.h"
#include "../scene/scene.h"
#include "../scene/camera.h"
#include <d3d12.h>
#include <memory>
#include <set>

#define VK_USE_PLATFORM_WIN32_KHR
#include <GLFW/glfw3native.h>

namespace fantasy 
{
    TestBase::TestBase()
    {
        parallel::initialize();
    }

    TestBase::~TestBase()
    {
        parallel::destroy();
		glfwDestroyWindow(_window);
		glfwTerminate();
    }

    bool TestBase::initialize(GraphicsAPI api)
    {
        ReturnIfFalse(init_window() && init_scene());

        switch (api)
        {
        case GraphicsAPI::D3D12: ReturnIfFalse(init_d3d12()); set_shader_platform(ShaderPlatform::DXIL); break;
        case GraphicsAPI::Vulkan: ReturnIfFalse(init_vulkan()); set_shader_platform(ShaderPlatform::SPIRV); break;
        }

        return create_samplers() && init_gui();
    }

    bool TestBase::run()
    {
        init_render_pass(_render_graph.get())->precede(_gui_pass.get());
        
        ReturnIfFalse(_render_graph->compile());
		while (!glfwWindowShouldClose(_window))
		{
			glfwPollEvents();
			_world.tick(_timer.tick());
			ReturnIfFalse(_render_graph->execute());
		}

        return true;
    }

    bool TestBase::init_gui()
    {
        _gui_pass = std::make_shared<GuiPass>();
		_gui_pass->init(_window, _device.get());
		_render_graph->add_pass(_gui_pass);
        return true;
    }


    bool TestBase::init_window()
    {
		ReturnIfFalse(glfwInit());
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		_window = glfwCreateWindow(CLIENT_WIDTH, CLIENT_HEIGHT, "Fantasy-Render", nullptr, nullptr);
		return _window != nullptr;
    }

    bool TestBase::init_scene()
    {
		_world.register_system(new SceneSystem());
		auto* camera = _world.get_global_entity()->assign<Camera>(_window);
        gui::add(
            [camera]()
            {
				ImGui::SliderInt("Camera Speed", &camera->speed, 1, 10);
                ImGui::Separator();
            }
        );
        return true;
    }

    bool TestBase::init_d3d12()
    {
#ifdef DEBUG
		{
			Microsoft::WRL::ComPtr<ID3D12Debug> d3d12_debug_controller;
			D3D12GetDebugInterface(IID_PPV_ARGS(d3d12_debug_controller.GetAddressOf()));
			d3d12_debug_controller->EnableDebugLayer();
		}
#endif
		ID3D12Device* d3d12_device;
		ReturnIfFalse(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d12_device))));

		ID3D12CommandQueue* d3d12_graphics_cmd_queue;
		ID3D12CommandQueue* d3d12_compute_cmd_queue;
		D3D12_COMMAND_QUEUE_DESC d3d12_queue_desc{};
		d3d12_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ReturnIfFalse(SUCCEEDED(d3d12_device->CreateCommandQueue(&d3d12_queue_desc, IID_PPV_ARGS(&d3d12_graphics_cmd_queue))));
		d3d12_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		ReturnIfFalse(SUCCEEDED(d3d12_device->CreateCommandQueue(&d3d12_queue_desc, IID_PPV_ARGS(&d3d12_compute_cmd_queue))));

		Microsoft::WRL::ComPtr<IDXGIFactory> dxgi_factory;
		ReturnIfFalse(SUCCEEDED(CreateDXGIFactory(IID_PPV_ARGS(dxgi_factory.GetAddressOf()))));

		DXGI_SWAP_CHAIN_DESC swap_chain_desc;
		swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_chain_desc.BufferDesc.Width = CLIENT_WIDTH;
		swap_chain_desc.BufferDesc.Height = CLIENT_HEIGHT;
		swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
		swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
		swap_chain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swap_chain_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swap_chain_desc.BufferCount = FLIGHT_FRAME_NUM;
		swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_chain_desc.OutputWindow = glfwGetWin32Window(_window);
		swap_chain_desc.SampleDesc = { 1, 0 };
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swap_chain_desc.Windowed = true;
		swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		ReturnIfFalse(SUCCEEDED(dxgi_factory->CreateSwapChain(d3d12_graphics_cmd_queue, &swap_chain_desc, _swap_chain.GetAddressOf())));

		ID3D12Resource* swap_chain_buffers[FLIGHT_FRAME_NUM];
		for (uint32_t ix = 0; ix < FLIGHT_FRAME_NUM; ++ix)
		{
			_swap_chain->GetBuffer(ix, IID_PPV_ARGS(&swap_chain_buffers[ix]));
		}


		DX12DeviceDesc dx12_device_desc;
		dx12_device_desc.d3d12_device = d3d12_device;
		dx12_device_desc.d3d12_graphics_cmd_queue = d3d12_graphics_cmd_queue;
		dx12_device_desc.d3d12_compute_cmd_queue = d3d12_compute_cmd_queue;
		ReturnIfFalse(_device = std::shared_ptr<DeviceInterface>(CreateDevice(dx12_device_desc)));

		_render_graph = std::make_unique<RenderGraph>(
			_device,
			[this]() 
            { 
                _swap_chain->Present(0, 0); 
                _current_back_buffer_index = (_current_back_buffer_index + 1) % FLIGHT_FRAME_NUM; 
            }
		);
		ReturnIfFalse(_render_graph->initialize(&_world));

		RenderResourceCache* cache = _render_graph->GetResourceCache();
		cache->collect_constants("back_buffer_index", &_current_back_buffer_index);

		TextureDesc back_buffer_desc;
		back_buffer_desc.width = CLIENT_WIDTH;
		back_buffer_desc.height = CLIENT_HEIGHT;
		back_buffer_desc.format = Format::RGBA8_UNORM;
		back_buffer_desc.use_clear_value = true;
		back_buffer_desc.clear_value = Color(0.0f, 0.0f, 0.0f, 1.0f);

		for (uint32_t ix = 0; ix < FLIGHT_FRAME_NUM; ++ix)
		{
			back_buffer_desc.name = "back_buffer" + std::to_string(ix);
			ReturnIfFalse(_back_buffers[ix] = std::shared_ptr<TextureInterface>(_device->create_texture_from_native(swap_chain_buffers[ix], back_buffer_desc)));
			cache->collect(_back_buffers[ix], ResourceType::Texture);
		}

		ReturnIfFalse(_final_texture = std::shared_ptr<TextureInterface>(_device->create_texture(
			TextureDesc::create_render_target_texture(CLIENT_WIDTH, CLIENT_HEIGHT, Format::RGBA8_UNORM, "final_texture")
		)));
		cache->collect(_final_texture, ResourceType::Texture);


        return true;
    }

	bool TestBase::create_samplers()
	{
		std::shared_ptr<SamplerInterface> linear_clamp_sampler;
		std::shared_ptr<SamplerInterface> point_clamp_sampler;
		std::shared_ptr<SamplerInterface> linear_warp_sampler;
		std::shared_ptr<SamplerInterface> point_wrap_sampler;
		std::shared_ptr<SamplerInterface> anisotropic_wrap_sampler;

		SamplerDesc sampler_desc;
		sampler_desc.name = "linear_wrap_sampler";
		ReturnIfFalse(linear_warp_sampler = std::shared_ptr<SamplerInterface>(_device->create_sampler(sampler_desc)));
		sampler_desc.SetFilter(false);
		sampler_desc.name = "point_wrap_sampler";
		ReturnIfFalse(point_wrap_sampler = std::shared_ptr<SamplerInterface>(_device->create_sampler(sampler_desc)));
		sampler_desc.SetAddressMode(SamplerAddressMode::Clamp);
		sampler_desc.name = "point_clamp_sampler";
		ReturnIfFalse(point_clamp_sampler = std::shared_ptr<SamplerInterface>(_device->create_sampler(sampler_desc)));
		sampler_desc.SetFilter(true);
		sampler_desc.name = "linear_clamp_sampler";
		ReturnIfFalse(linear_clamp_sampler = std::shared_ptr<SamplerInterface>(_device->create_sampler(sampler_desc)));

		sampler_desc.name = "anisotropic_wrap_sampler";
		sampler_desc.SetAddressMode(SamplerAddressMode::Wrap);
		sampler_desc.max_anisotropy = 8.0f;
		ReturnIfFalse(anisotropic_wrap_sampler = std::shared_ptr<SamplerInterface>(_device->create_sampler(sampler_desc)));


		RenderResourceCache* cache = _render_graph->GetResourceCache();
		cache->collect(linear_clamp_sampler, ResourceType::Sampler);
		cache->collect(point_clamp_sampler, ResourceType::Sampler);
		cache->collect(linear_warp_sampler, ResourceType::Sampler);
		cache->collect(point_wrap_sampler, ResourceType::Sampler);
		cache->collect(anisotropic_wrap_sampler, ResourceType::Sampler);

		return true;
	}

	vk::Bool32 debug_call_back(
		VkDebugUtilsMessageSeverityFlagBitsEXT message_serverity,
		VkDebugUtilsMessageTypeFlagsEXT message_type,
		const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
		void* user_data
	)
	{
		LOG_ERROR(callback_data->pMessage);
		return VK_FALSE;
	}

    bool TestBase::init_vulkan()
    {
        VKDeviceDesc desc;
		
        return true;
    }
	bool TestBase::create_instance()
	{
		// Instance info 需要 layers 和 extensions.
		vk::InstanceCreateInfo instance_info{};
		
		vk::ApplicationInfo app_info{};
		app_info.pApplicationName = "Vulkan Test";

		// 和 SPIR-V 版本有关.
		app_info.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
		app_info.engineVersion = VK_MAKE_VERSION(1, 3, 0);
		app_info.apiVersion = VK_API_VERSION_1_3;	
		
		instance_info.pApplicationInfo = &app_info;


		uint32_t glfw_extension_count = 0;
		const CHAR** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
		_vk_instance_extensions.insert(_vk_instance_extensions.end(), glfw_extensions, glfw_extensions + glfw_extension_count);

#ifdef DEBUG
		_vk_validation_layers.push_back("VK_LAYER_KHRONOS_validation");

		ReturnIfFalse(check_validation_layer_support());
		
		instance_info.enabledLayerCount = static_cast<uint32_t>(_vk_validation_layers.size());
		instance_info.ppEnabledLayerNames = _vk_validation_layers.data();


		_vk_instance_extensions.push_back("VK_EXT_debug_utils");
#endif

		instance_info.enabledExtensionCount = static_cast<uint32_t>(_vk_instance_extensions.size());
		instance_info.ppEnabledExtensionNames = _vk_instance_extensions.data();

		return vk::createInstance(&instance_info, nullptr, &_vk_instance) == vk::Result::eSuccess;
	}

	bool TestBase::check_validation_layer_support()
	{
		uint32_t layer_count = 0;
		ReturnIfFalse(vk::enumerateInstanceLayerProperties(&layer_count, nullptr) == vk::Result::eSuccess);
		std::vector<vk::LayerProperties> properties(layer_count);
		ReturnIfFalse(vk::enumerateInstanceLayerProperties(&layer_count, properties.data()) == vk::Result::eSuccess);

		for (const auto& layer : _vk_validation_layers)
		{
			bool found = false;
			for (const auto& property : properties)
			{
				if (strcmp(layer, property.layerName) == 0)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				LOG_ERROR("Extension " + std::string(layer) + " is not support.");
				return false;
			}
		}
		return true;
	}

	bool TestBase::enumerate_support_extension()
	{
		uint32_t extension_count = 0;
		ReturnIfFalse(vk::enumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr) == vk::Result::eSuccess);
		std::vector<vk::ExtensionProperties> extension_properties(extension_count);
		ReturnIfFalse(vk::enumerateInstanceExtensionProperties(nullptr, &extension_count, extension_properties.data()) == vk::Result::eSuccess);
		for (uint32_t ix = 0; ix < extension_count; ++ix)
		{
			LOG_INFO(extension_properties[ix].extensionName);
		}
		return true;
	}

	bool TestBase::create_debug_utils_messager()
	{
		vk::DebugUtilsMessengerCreateInfoEXT debug_util_info{};
		debug_util_info.messageSeverity = 
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
		debug_util_info.messageType = 
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
		debug_util_info.pfnUserCallback = debug_call_back;

		// 于 vkCreateDebugUtilsMessengerEXT 函数是一个扩展函数, 不会被 Vulkan 库自动加载, 
		// 所以需要我们自己使用 vkGetInstanceProcAddr 函数来加载它.
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_vk_instance, "vkCreateDebugUtilsMessengerEXT");
		
		if (func)
		{
			VkDebugUtilsMessengerCreateInfoEXT create_info = (VkDebugUtilsMessengerCreateInfoEXT)debug_util_info;
			func(_vk_instance, &create_info, nullptr, &_vk_debug_callback);
		}
		else
		{
			LOG_ERROR("Get vkCreateDebugUtilsMessengerEXT process address failed.");
			return false;
		}
		return true;
	}

	bool TestBase::destroy_debug_utils_messager()
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_vk_instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func)
		{
			func(_vk_instance, _vk_debug_callback, nullptr);
		}
		else
		{
			LOG_ERROR("Get vkDestroyDebugUtilsMessengerEXT process address failed.");
			return false; 
		}
		return true;
	}

	bool TestBase::pick_physical_device()
	{
		uint32_t physical_device_count = 0;
		ReturnIfFalse(_vk_instance.enumeratePhysicalDevices(&physical_device_count, nullptr) == vk::Result::eSuccess);
		ReturnIfFalse(physical_device_count != 0);
		std::vector<vk::PhysicalDevice> physical_devices(physical_device_count);
		ReturnIfFalse(_vk_instance.enumeratePhysicalDevices(&physical_device_count, physical_devices.data()) == vk::Result::eSuccess);

		for (const auto& device : physical_devices)
		{
			vk::PhysicalDeviceProperties properties;
			vk::PhysicalDeviceFeatures features;
			device.getProperties(&properties);
			device.getFeatures(&features);

			bool device_type_support = 
				properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu || 
				properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu;
			
			if (
				device_type_support && 
				find_queue_family(device) &&
				check_device_extension(device) && 
				check_swapchain_support(device)
			)
			{
				_vk_physical_device = device;
				_vk_physical_device.getMemoryProperties(&_vk_memory_properties);

				break;
			}
		}

		return true;
	}

	bool TestBase::find_queue_family(const auto& physical_device)
	{
		// 需要先确立 queue 的需求再创建 vkdevice.
		uint32_t queue_family_count = 0;
		physical_device.getQueueFamilyProperties(&queue_family_count, nullptr);
		std::vector<vk::QueueFamilyProperties> properties(queue_family_count);
		physical_device.getQueueFamilyProperties(&queue_family_count, properties.data());

		for (uint32_t ix = 0; ix < properties.size(); ++ix)
		{
			vk::Bool32 present_support = false;

			// 验证所获取的 surface 能否支持该物理设备的 queue family 进行 present.
			if (physical_device.getSurfaceSupportKHR(ix, _surface, &present_support) == vk::Result::eSuccess && present_support) 
			{
				_queue_family_index.present_index = ix;
			}
			if (properties[ix].queueCount > 0 && properties[ix].queueFlags & vk::QueueFlagBits::eGraphics)
			{
				_queue_family_index.graphics_index = ix;
			}


			if (
				_queue_family_index.graphics_index != INVALID_SIZE_32 && 
				_queue_family_index.graphics_index != INVALID_SIZE_32
			) 
			{
				break;
			}
		}
		return true;
	}


	bool TestBase::create_device()
	{
		vk::DeviceCreateInfo device_create_info{};

		std::set<uint32_t> queue_family_indices = { _queue_family_index.graphics_index, _queue_family_index.present_index };
		std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;

		FLOAT queue_priority = 1.0f;
		for (uint32_t ix : queue_family_indices)
		{
			auto& create_info = queue_create_infos.emplace_back();
			create_info.queueFamilyIndex = ix;
			create_info.queueCount = 1;
			create_info.pQueuePriorities = &queue_priority;
		}

		device_create_info.pQueueCreateInfos = queue_create_infos.data();
		device_create_info.queueCreateInfoCount = 1;
		
		// 暂时不设立 features.
		vk::PhysicalDeviceFeatures device_features{};
		device_create_info.pEnabledFeatures = &device_features;

		device_create_info.enabledExtensionCount = static_cast<uint32_t>(_vk_device_extensions.size());
		device_create_info.ppEnabledExtensionNames = _vk_device_extensions.data();
#if DEBUG
		device_create_info.ppEnabledLayerNames = _vk_validation_layers.data();
		device_create_info.enabledLayerCount = static_cast<uint32_t>(_vk_validation_layers.size());
#endif

		ReturnIfFalse(_vk_physical_device.createDevice(&device_create_info, nullptr, &_vk_device) == vk::Result::eSuccess);
		_vk_device.getQueue(_queue_family_index.graphics_index, 0, &_vk_graphics_queue);
		_vk_device.getQueue(_queue_family_index.present_index, 0, &_vk_present_queue);
		
		return true;  
	}

	bool TestBase::check_device_extension(const auto& physical_device)
	{
		_vk_device_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		
		// 检查物理设备是否支持所需扩展, 现在就只有交换链这一个设备扩展.
		uint32_t extension_count = 0;
		ReturnIfFalse(physical_device.enumerateDeviceExtensionProperties(nullptr, &extension_count, nullptr) == vk::Result::eSuccess);
		std::vector<vk::ExtensionProperties> extension_properties(extension_count);
		ReturnIfFalse(physical_device.enumerateDeviceExtensionProperties(nullptr, &extension_count, extension_properties.data()) == vk::Result::eSuccess);

		std::set<std::string> unavailble_extensions(_vk_device_extensions.begin(), _vk_device_extensions.end());
		for (const auto& property : extension_properties)
		{
			unavailble_extensions.erase(property.extensionName);
		}

		return unavailble_extensions.empty();
	}

	bool TestBase::check_swapchain_support(const auto& physical_device)
	{
		ReturnIfFalse(vk::Result::eSuccess == physical_device.getSurfaceCapabilitiesKHR(
			_surface, &_vk_swapchain_info.surface_capabilities
		));

		uint32_t surface_format_count = 0;
		ReturnIfFalse(vk::Result::eSuccess == physical_device.getSurfaceFormatsKHR(
			_surface, 
			&surface_format_count, 
			nullptr
		));
		ReturnIfFalse(surface_format_count > 0);

		_vk_swapchain_info.surface_formats.resize(surface_format_count);
		ReturnIfFalse(vk::Result::eSuccess == physical_device.getSurfaceFormatsKHR(
			_surface, 
			&surface_format_count, 
			_vk_swapchain_info.surface_formats.data()
		));

		uint32_t present_mode_count = 0;
		ReturnIfFalse(vk::Result::eSuccess == physical_device.getSurfacePresentModesKHR(
			_surface, 
			&present_mode_count, 
			nullptr
		));
		ReturnIfFalse(present_mode_count > 0);

		_vk_swapchain_info.present_modes.resize(present_mode_count);
		ReturnIfFalse(vk::Result::eSuccess == physical_device.getSurfacePresentModesKHR(
			_surface, 
			&present_mode_count, 
			_vk_swapchain_info.present_modes.data()
		));
		
		return true;
	}


	bool TestBase::create_swapchain()
	{
		// 确定缓冲区个数.
		// 若 maxImageCount 的值为 0 表明，只要内存可以满足，我们可以使用任意数量的图像.
		uint32_t frames_in_flight_count = std::min(_vk_swapchain_info.surface_capabilities.minImageCount + 1, static_cast<uint32_t>(FLIGHT_FRAME_NUM));
		
		if (
			_vk_swapchain_info.surface_capabilities.maxImageCount > 0 &&
			frames_in_flight_count > _vk_swapchain_info.surface_capabilities.maxImageCount
		)
		{
			frames_in_flight_count = _vk_swapchain_info.surface_capabilities.maxImageCount;
		}

		vk::SwapchainCreateInfoKHR swapchain_create_info{};
		swapchain_create_info.surface = _surface;
		swapchain_create_info.minImageCount = frames_in_flight_count;

		// 确定缓冲区 format.
		if (
			_vk_swapchain_info.surface_formats.size() == 1 &&
			_vk_swapchain_info.surface_formats[0].format == vk::Format::eUndefined
		)
		{
			swapchain_create_info.imageFormat = vk::Format::eR8G8B8A8Unorm;
			swapchain_create_info.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
		}
		else 
		{
			bool found = false;
			for (const auto& surface_format : _vk_swapchain_info.surface_formats)
			{
				if (
					surface_format.format == vk::Format::eR8G8B8A8Unorm &&
					surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear
				)
				{
					found = true;
					swapchain_create_info.imageFormat = vk::Format::eR8G8B8A8Unorm;
					swapchain_create_info.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
				}
			}
			if (!found)
			{
				swapchain_create_info.imageFormat = _vk_swapchain_info.surface_formats[0].format;
				swapchain_create_info.imageColorSpace = _vk_swapchain_info.surface_formats[0].colorSpace; 
			}
		}
		_vk_swapchain_format = swapchain_create_info.imageFormat;

		for (const auto& present_mode : _vk_swapchain_info.present_modes)
		{
			if (present_mode == vk::PresentModeKHR::eMailbox) { swapchain_create_info.presentMode = present_mode; break; }
			else if (present_mode == vk::PresentModeKHR::eImmediate) { swapchain_create_info.presentMode = present_mode; break; }
			else if (present_mode == vk::PresentModeKHR::eFifo) { swapchain_create_info.presentMode = present_mode; break; }
		}

		// 确定缓冲区分辨率.
		swapchain_create_info.imageExtent = vk::Extent2D{ CLIENT_WIDTH, CLIENT_HEIGHT };
		swapchain_create_info.imageArrayLayers = 1;
		swapchain_create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
		swapchain_create_info.clipped = VK_TRUE;

		uint32_t queue_family_indices[] = { _queue_family_index.graphics_index, _queue_family_index.present_index };
		if (_queue_family_index.graphics_index != _queue_family_index.present_index)
		{
			swapchain_create_info.imageSharingMode = vk::SharingMode::eConcurrent;
			swapchain_create_info.queueFamilyIndexCount = 2;
			swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
		}
		else 
		{
			// 一张图像同一时间只能被一个队列族所拥有, 在另一队列族使用它之前, 必须显式地改变图像所有权.
			swapchain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
			swapchain_create_info.queueFamilyIndexCount = 0;
			swapchain_create_info.pQueueFamilyIndices = nullptr;
		}

		// 我们可以为交换链中的图像指定一个固定的变换操作 (需要交换链具有 supportedTransforms 特性),
		// 比如顺时针旋转 90 度或是水平翻转. 
		// 如果读者不需要进行任何变换操作, 指定使用 currentTransform 变换即可.
		swapchain_create_info.preTransform = _vk_swapchain_info.surface_capabilities.currentTransform;

		// compositeAlpha 成员变量用于指定alpha通道是否被用来和窗口系统中的其它窗口进行混合操作.
		// 设置为 VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR 来忽略掉 alpha 通道.
		swapchain_create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

		swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

		ReturnIfFalse(_vk_device.createSwapchainKHR(&swapchain_create_info, nullptr, &_vk_swapchain) == vk::Result::eSuccess);

		// 获取缓冲区 buffer.
		uint32_t back_buffer_count = 0;
		ReturnIfFalse(_vk_device.getSwapchainImagesKHR(_vk_swapchain, &back_buffer_count, nullptr) == vk::Result::eSuccess);
		ReturnIfFalse(back_buffer_count == FLIGHT_FRAME_NUM);
		ReturnIfFalse(_vk_device.getSwapchainImagesKHR(_vk_swapchain, &back_buffer_count, _vk_back_buffers.data()) == vk::Result::eSuccess);
		
		_back_buffer_views.resize(back_buffer_count);
		for (uint32_t ix = 0; ix < back_buffer_count; ++ix)
		{
			vk::ImageViewCreateInfo view_create_info{};
			view_create_info.viewType = vk::ImageViewType::e2D;
			view_create_info.image = _vk_back_buffers[ix];
			view_create_info.format = swapchain_create_info.imageFormat;
			
			// components 成员变量用于进行图像颜色通道的映射.
			// 比如, 对于单色纹理, 我们可以将所有颜色通道映射到红色通道. 我们也可以直接将颜色通道的值映射为常数 0 或 1.
			// 这里使用默认的 VK_COMPONENT_SWIZZLE_IDENTITY.
			view_create_info.components.r = vk::ComponentSwizzle::eIdentity;
			view_create_info.components.g = vk::ComponentSwizzle::eIdentity;
			view_create_info.components.b = vk::ComponentSwizzle::eIdentity;
			view_create_info.components.a = vk::ComponentSwizzle::eIdentity;

			// subresourceRange 成员变量用于指定图像的用途和图像的哪一部分可以被访问.
			// 这里图像被用作渲染目标, 并且没有细分级别, 只存在一个图层.
			// VK_IMAGE_ASPECT_COLOR_BIT: 表示图像的颜色方面，用于颜色附件或纹理.
			// VK_IMAGE_ASPECT_DEPTH_BIT: 表示图像的深度方面，用于深度缓冲区.
			// VK_IMAGE_ASPECT_STENCIL_BIT: 表示图像的模板方面，用于模板缓冲区.
			view_create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			view_create_info.subresourceRange.baseMipLevel = 0;
			view_create_info.subresourceRange.levelCount = 1;
			view_create_info.subresourceRange.baseArrayLayer = 0;
			view_create_info.subresourceRange.layerCount = 1;

			ReturnIfFalse(_vk_device.createImageView(&view_create_info, nullptr, &_back_buffer_views[ix]) == vk::Result::eSuccess);
		}
		return true;
	}
}